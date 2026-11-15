// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_controller.hpp"

#include "event_haptics.hpp"
#include "event_sounds.hpp"
#include "external_links.hpp"
#include "heart_boom_maths/profile.hpp"
#include "level_text.hpp"
#include "services/cloud_sync.hpp"
#include "services/notices.hpp"

#include <QLoggingCategory>
#include <QRandomGenerator>
#include <algorithm>
#include <utility>

namespace heart_boom_maths::game
{

namespace
{

Q_LOGGING_CATEGORY(lcGame, "hbm.game")

// The sign shown for taking away. One place, so the font's coverage can be checked and the glyph swapped.
constexpr auto MINUS_SIGN = "-";

constexpr auto REQUIRED_LINK_PREFIX = "https://";

QString to_qstring(const std::string& text)
{
    return QString::fromStdString(text);
}

QStringList parts_for(const Question& q)
{
    QStringList parts;
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        parts << QString::number(q.terms[0]) << QStringLiteral("+") << QStringLiteral("?") << QStringLiteral("=")
              << QString::number(q.terms[1]);
        return parts;
    }
    for (int i = 0; i < q.term_count; ++i)
    {
        if (i > 0)
        {
            parts << (q.operators[static_cast<std::size_t>(i - 1)] == Operator::ADD ? QStringLiteral("+")
                                                                                    : QString::fromUtf8(MINUS_SIGN));
        }
        parts << QString::number(q.terms[static_cast<std::size_t>(i)]);
    }
    parts << QStringLiteral("=") << QStringLiteral("?");
    return parts;
}

QList<int> tiles_for(const Question& q)
{
    QList<int> out;
    for (const int tile : q.tiles)
    {
        out << tile;
    }
    return out;
}

bool same_question(const Question& a, const Question& b) noexcept
{
    return a.shape == b.shape && a.term_count == b.term_count && a.terms == b.terms && a.operators == b.operators &&
           a.answer == b.answer && a.tiles == b.tiles && a.correct_tile == b.correct_tile;
}

/// A saved run is only ever written from the pause screen, so only a running or paused session is resumed. The core
/// would also restore a level-clear card, a win or a finished game from a hand-made file, and the screens for those
/// have numbers that only the events of the game on screen fill in.
bool is_resumable(SessionState state) noexcept
{
    return state == SessionState::PLAYING || state == SessionState::PAUSED;
}

} // namespace

GameOptions GameOptions::with_random_seed()
{
    GameOptions options;
    options.session_seed = QRandomGenerator::system()->generate64();
    return options;
}

GameController::GameController(GameServices services, GameOptions options, QObject* parent)
    : QObject(parent), m_Services(services), m_Cloud(services.cloud != nullptr ? services.cloud : &m_NullCloud),
      m_Tuning(options.tuning), m_Session(*options.tuning, options.session_seed),
      m_SaveManager(*services.store, *options.tuning), m_Driver(options.tuning->ticks_per_second),
      m_TileGuardNs(options.tile_guard_ns), m_GateSeedSource(std::move(options.gate_seed_source)),
      m_IdSource(std::move(options.id_source)), m_BuildInfo(std::move(options.build_info))
{
    if (!m_GateSeedSource)
    {
        m_GateSeedSource = [] { return QRandomGenerator::system()->generate64(); };
    }
    if (!m_IdSource)
    {
        m_IdSource = [] { return QRandomGenerator::system()->generate64(); };
    }

    m_Timer.setInterval(TICK_POLL_INTERVAL_MS);
    m_Timer.setTimerType(Qt::PreciseTimer);
    connect(&m_Timer, &QTimer::timeout, this, &GameController::advance_time);

    HBM_ZONE("controller.construct");
    LoadResult loaded = m_SaveManager.load();
    switch (loaded.status)
    {
    case LoadStatus::OK:
        m_Save = std::move(loaded.data);
        break;
    case LoadStatus::DAMAGED:
        // Moved aside before any save, which would replace it. Defaults run this session.
        qCWarning(lcGame) << "The save file is damaged; keeping it as save.json.bad and starting on defaults";
        if (m_Services.quarantine != nullptr && !m_Services.quarantine->set_damaged_aside())
        {
            qCWarning(lcGame) << "The damaged save could not be moved aside";
        }
        break;
    case LoadStatus::NEWER:
        // The manager refuses every save from here on, so the newer file is never overwritten.
        qCWarning(lcGame) << "The save file comes from a newer version; running on defaults and not saving";
        m_SaveBlocked = true;
        break;
    }

    assign_ids();
    // A run whose player is gone (removed on another device and synced here) can never be offered: drop it.
    if (m_Save.device.run.has_value())
    {
        const Player* owner = find_player(m_Save.shared, m_Save.device.run_player);
        if (owner == nullptr || owner->removed)
        {
            forget_saved_run();
        }
    }
    take_current_player_into_session();
    m_Shown = m_Session.view();
    apply_settings();
    update_music();
    // Start-up, after the save has loaded: listen for the other devices and take what they sent, if the switch is on.
    m_CloudAvailableShown = m_Cloud->available();
    apply_cloud_sync();
    request_cloud_sync();
}

GameController::~GameController()
{
    // The store may outlive the controller, and a change it reports must not reach a controller that is gone.
    if (m_CloudWatching)
    {
        m_Cloud->set_change_handler({});
    }
}

// ---- Commands ----------------------------------------------------------------------------------------------------

void GameController::startGame()
{
    HBM_ZONE("controller.start_game");
    const SessionState before = m_Session.view().state;
    if (before != SessionState::IDLE && before != SessionState::GAME_OVER)
    {
        return;
    }
    m_Session.start_game();
    arm_tile_guard();
    m_ScoreSubmitted = false;
    m_NameRank = 0;
    m_NewBest = false;
    m_CoinsEarned = 0;
    m_CoinsEarnedKnown = true;
    emit nameEntryChanged();
    if (m_HighlightRank != 0)
    {
        m_HighlightRank = 0;
        emit topScoresChanged();
    }
    // A new game replaces the run the title screen offered to continue. Another player's run is left alone: it goes
    // only when the device has to save a run for the player now playing (SPEC, Players).
    if (run_is_current_players())
    {
        forget_saved_run();
        emit savedRunChanged();
    }
    process();
    persist();
}

bool GameController::continueGame()
{
    HBM_ZONE("controller.continue_game");
    const SessionState before = m_Session.view().state;
    if ((before != SessionState::IDLE && before != SessionState::GAME_OVER) || !run_is_current_players())
    {
        return false;
    }
    const std::vector<std::int64_t> values = *m_Save.device.run;
    // The run is spent as soon as it is read, whether or not it restores (SPEC: it resumes once).
    forget_saved_run();
    emit savedRunChanged();
    // A scratch session decides first, so a run the game would not offer leaves the real session untouched.
    GameSession scratch(*m_Tuning, 0);
    const bool restored = scratch.restore(values) && is_resumable(scratch.view().state) && m_Session.restore(values);
    if (!restored)
    {
        qCWarning(lcGame) << "The saved run could not be restored; starting clean";
        persist();
        emit continueFailed();
        return false;
    }
    m_ScoreSubmitted = false;
    // The save holds the session, not what the screens added up from its events: the coins of the game so far, and the
    // answers of a level that was part way through, are not known.
    m_CoinsEarned = 0;
    m_CoinsEarnedKnown = false;
    m_LevelCountsKnown = m_Session.view().questions_answered == 0;
    m_LevelAnswerPoints = 0;
    m_LevelAnswerCoins = 0;
    m_LevelStreakCoins = 0;
    if (m_Session.view().state == SessionState::PLAYING)
    {
        // Only a paused run is ever written. Anything else waits on the pause screen too, never running by itself.
        m_Session.pause();
    }
    // The snapshot holds the wallet as it was at the pause; the player's wallet may have moved since.
    take_current_player_into_session();
    process();
    persist();
    return true;
}

void GameController::arm_tile_guard()
{
    m_TileGuardUntilNs = m_Services.clock->now_ns() + m_TileGuardNs;
}

void GameController::answer(int tile)
{
    HBM_ZONE("controller.answer");
    if (m_Services.clock->now_ns() < m_TileGuardUntilNs)
    {
        return;
    }
    m_Session.answer(tile);
    process();
}

void GameController::useHint()
{
    HBM_ZONE("controller.use_hint");
    m_Session.use_hint();
    process();
}

void GameController::pause()
{
    HBM_ZONE("controller.pause");
    m_Session.pause();
    process();
    if (m_Session.view().state == SessionState::PAUSED)
    {
        remember_run();
        persist();
    }
}

void GameController::resume()
{
    if (m_Session.view().state != SessionState::PAUSED)
    {
        return;
    }
    m_Session.resume();
    arm_tile_guard();
    forget_own_saved_run();
    emit savedRunChanged();
    process();
    persist();
}

void GameController::quit()
{
    m_Session.quit();
    process();
}

void GameController::nextLevel()
{
    HBM_ZONE("controller.next_level");
    m_Session.next_level();
    arm_tile_guard();
    process();
}

void GameController::keepGoing()
{
    m_Session.keep_going();
    arm_tile_guard();
    process();
}

void GameController::finish()
{
    m_Session.finish();
    process();
}

int GameController::saveName(const QString& typed)
{
    HBM_ZONE("controller.save_name");
    if (m_NameRank == 0)
    {
        return 0;
    }
    const Player* player = current_player(m_Save);
    if (player == nullptr)
    {
        return 0;
    }
    // A player who has a name never reaches the name screen; should one get here, the entry takes the name they have.
    const std::string name =
        player->name.empty() ? trimmed_name(normalise_name(typed.toStdString(), *m_Tuning)) : player->name;
    const std::string player_id = player->id;
    const auto rank = record_score(m_Save, player_id, name, m_FinalScore, *m_Tuning);
    if (!rank.has_value())
    {
        return 0; // an empty name: the screen keeps SAVE disabled until there is one
    }
    if (current_player(m_Save)->name.empty() && !rename_player(m_Save.shared, player_id, name, *m_Tuning))
    {
        qCWarning(lcGame) << "The player could not be given the name of the entry";
    }
    m_NameRank = 0;
    m_HighlightRank = *rank;
    emit nameEntryChanged();
    emit topScoresChanged();
    emit playersChanged();
    persist();
    return *rank;
}

void GameController::skipName()
{
    if (m_NameRank == 0)
    {
        return;
    }
    // The entry is dropped and the player stays unnamed. The score still counted towards the player's best, so NEW
    // BEST! stays as it was.
    m_NameRank = 0;
    emit nameEntryChanged();
    emit playersChanged();
    request_cloud_sync(); // the title may take a changed wallet now that no name is waiting
}

bool GameController::canSaveName(const QString& typed) const
{
    return normalise_name(typed.toStdString(), *m_Tuning).find_first_not_of(' ') != std::string::npos;
}

QString GameController::normaliseName(const QString& typed) const
{
    return to_qstring(normalise_name(typed.toStdString(), *m_Tuning));
}

bool GameController::selectPlayer(const QString& id)
{
    HBM_ZONE("controller.select_player");
    if (!can_change_players())
    {
        return false;
    }
    const std::string target = id.toStdString();
    const Player* player = find_player(m_Save.shared, target);
    if (player == nullptr || player->removed)
    {
        return false;
    }
    if (target != m_Save.device.current_player)
    {
        switch_to_player(target);
    }
    return true;
}

QString GameController::addPlayer()
{
    HBM_ZONE("controller.add_player");
    if (!can_change_players() || !can_add_player())
    {
        return {};
    }
    // Two draws never meet in practice; a few tries cover a source that repeats itself.
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        std::string id = hex_id(m_IdSource());
        const auto added = add_player(m_Save.shared, id, *m_Tuning);
        if (added.has_value())
        {
            switch_to_player(id);
            return to_qstring(id);
        }
        if (added.error() != PlayerError::ID_TAKEN)
        {
            break;
        }
    }
    qCWarning(lcGame) << "A player could not be added";
    return {};
}

bool GameController::renamePlayer(const QString& id, const QString& typed)
{
    HBM_ZONE("controller.rename_player");
    const std::string name = trimmed_name(normalise_name(typed.toStdString(), *m_Tuning));
    const Player* player = find_player(m_Save.shared, id.toStdString());
    if (name.empty() || player == nullptr || player->removed)
    {
        return false;
    }
    if (player->name == name)
    {
        return true; // nothing to change, and no change to count for a sync
    }
    if (!rename_player(m_Save.shared, id.toStdString(), name, *m_Tuning))
    {
        return false;
    }
    emit playersChanged();
    persist();
    return true;
}

bool GameController::requestRemovePlayer(const QString& id)
{
    if (gate_active() || !can_change_players() || !can_remove_player())
    {
        return false;
    }
    const Player* player = find_player(m_Save.shared, id.toStdString());
    if (player == nullptr || player->removed)
    {
        return false;
    }
    m_GatePlayer = id.toStdString();
    begin_gate(GatePurpose::RemovePlayer);
    return true;
}

void GameController::setSoundEnabled(bool enabled)
{
    if (m_Save.device.sound == enabled)
    {
        return;
    }
    m_Save.device.sound = enabled;
    apply_settings();
    emit settingsChanged();
    persist();
}

void GameController::setMusicEnabled(bool enabled)
{
    if (m_Save.device.music == enabled)
    {
        return;
    }
    m_Save.device.music = enabled;
    apply_settings();
    emit settingsChanged();
    persist();
}

void GameController::setHapticsEnabled(bool enabled)
{
    if (m_Save.device.haptics == enabled)
    {
        return;
    }
    m_Save.device.haptics = enabled;
    apply_settings();
    emit settingsChanged();
    persist();
}

void GameController::tap()
{
    play_haptic(HapticId::TAP);
}

void GameController::requestGameCenter(bool enabled)
{
    if (!game_center_available())
    {
        return;
    }
    if (!enabled)
    {
        if (m_Save.device.game_center)
        {
            m_Save.device.game_center = false;
            apply_settings();
            emit settingsChanged();
            persist();
        }
        return;
    }
    if (!m_Save.device.game_center)
    {
        begin_gate(GatePurpose::GameCenter);
    }
}

void GameController::requestCloudSync(bool enabled)
{
    if (!cloud_sync_available())
    {
        return;
    }
    if (!enabled)
    {
        // Off is immediate and needs no gate. The copy in the family's iCloud stays as it is.
        if (m_Save.device.cloud_sync)
        {
            m_Save.device.cloud_sync = false;
            m_CloudOwed = false;
            apply_cloud_sync();
            emit settingsChanged();
            persist();
        }
        return;
    }
    // A save that came from a newer build is never written, so a switch could not be kept.
    if (!m_Save.device.cloud_sync && !m_SaveBlocked)
    {
        begin_gate(GatePurpose::CloudSync);
    }
}

bool GameController::requestLink(const QString& url)
{
    if (!url.startsWith(QLatin1String(REQUIRED_LINK_PREFIX), Qt::CaseInsensitive))
    {
        return false;
    }
    m_GateUrl = url.toStdString();
    begin_gate(GatePurpose::Link);
    return true;
}

bool GameController::submitGate(const QString& typed)
{
    HBM_ZONE("controller.submit_gate");
    if (m_GatePurpose == GatePurpose::None)
    {
        return false;
    }
    if (!gate_answer_matches(m_GateChallenge, typed.toStdString()))
    {
        // A fresh number each time, so the answer cannot be found by trying again.
        m_GateChallenge = make_gate_challenge(m_GateSeedSource());
        emit gateChanged();
        emit gateFailed();
        return false;
    }
    const GatePurpose purpose = m_GatePurpose;
    const std::string url = std::move(m_GateUrl);
    const std::string gated_player = std::move(m_GatePlayer);
    end_gate();
    if (purpose == GatePurpose::Link)
    {
        m_Services.links->open(url);
    }
    else if (purpose == GatePurpose::GameCenter)
    {
        m_Save.device.game_center = true;
        apply_settings();
        emit settingsChanged();
        persist();
    }
    else if (purpose == GatePurpose::CloudSync)
    {
        m_Save.device.cloud_sync = true;
        apply_cloud_sync();
        emit settingsChanged();
        save_now();
        request_cloud_sync(); // the switch being turned on is a trigger
    }
    else if (purpose == GatePurpose::RemovePlayer)
    {
        remove_player_now(gated_player);
    }
    emit gatePassed(purpose);
    return true;
}

void GameController::cancelGate()
{
    if (m_GatePurpose != GatePurpose::None)
    {
        end_gate();
    }
}

bool GameController::gateMatches(const QString& typed) const
{
    return m_GatePurpose != GatePurpose::None && gate_answer_matches(m_GateChallenge, typed.toStdString());
}

// ---- Hooks -------------------------------------------------------------------------------------------------------

void GameController::on_application_state(bool active)
{
    HBM_ZONE("controller.on_application_state");
    m_Active = active;
    if (active)
    {
        m_Services.audio->resume();
        sync_driver(); // a paused run stays paused: the driver only runs while playing
        sync_haptics();
        // A person may have signed in to or out of iCloud while the app was away, and the other devices may have moved
        // on.
        apply_cloud_sync();
        request_cloud_sync();
        return;
    }
    // A grown-up's question does not wait for them to come back: whoever picks the device up next meets no open gate.
    cancelGate();
    if (m_Session.view().state == SessionState::PLAYING)
    {
        m_Session.pause();
        process();
    }
    if (m_Session.view().state == SessionState::PAUSED)
    {
        remember_run();
    }
    sync_driver();
    sync_haptics();
    m_Services.audio->suspend();
    persist();
}

void GameController::advance_time()
{
    HBM_ZONE("controller.advance_time");
    if (!m_Driver.running())
    {
        return;
    }
    const int due = m_Driver.ticks_due(m_Services.clock->now_ns());
    HBM_PLOT("controller.ticks_due", due);
    for (int i = 0; i < due && m_Session.view().state == SessionState::PLAYING; ++i)
    {
        m_Session.tick();
        process();
    }
}

// ---- Internals ---------------------------------------------------------------------------------------------------

void GameController::process()
{
    HBM_ZONE("controller.process");
    const std::vector<Event> events = m_Session.take_events();
    refresh_view();
    for (const Event& event : events)
    {
        handle_event(event);
    }
    // One haptic for everything this command or tick produced, before the pause that may follow it stops them.
    if (const std::optional<HapticId> haptic = haptic_for_events(events); haptic.has_value())
    {
        play_haptic(*haptic);
    }
    sync_driver();
    sync_haptics();
}

void GameController::refresh_view()
{
    HBM_ZONE("controller.refresh_view");
    const SessionView& v = m_Session.view();
    const SessionView old = m_Shown;
    m_Before = old;
    m_Shown = v;

    const bool level_changed = v.level != old.level || v.endless != old.endless ||
                               v.questions_per_level != old.questions_per_level || v.ticks_total != old.ticks_total ||
                               v.seconds_per_right_answer != old.seconds_per_right_answer ||
                               v.seconds_per_wrong_answer != old.seconds_per_wrong_answer;

    if (v.state != old.state)
    {
        emit stateChanged();
        emit playersChanged(); // canChangePlayers follows the state
        update_music();
    }
    if (level_changed)
    {
        emit levelChanged();
    }
    if (v.correct_answers != old.correct_answers)
    {
        emit correctAnswersChanged();
    }
    if (v.question_number != old.question_number || !same_question(v.question, old.question))
    {
        emit questionChanged();
    }
    if (v.removed_tiles != old.removed_tiles)
    {
        emit removedTilesChanged();
    }
    if (v.hint_available != old.hint_available || v.hint_used != old.hint_used)
    {
        emit hintAvailableChanged();
    }
    if (v.ticks_left != old.ticks_left || v.seconds_left != old.seconds_left)
    {
        emit timeChanged();
    }
    if (v.alert != old.alert)
    {
        emit alertChanged();
    }
    if (v.points != old.points)
    {
        emit pointsChanged();
    }
    if (v.wallet != old.wallet)
    {
        emit walletChanged();
    }
    if (v.run != old.run)
    {
        emit runChanged();
    }
    if (v.multiplier != old.multiplier)
    {
        emit multiplierChanged();
    }

    // The ticking is a long sound: stop it when the timer leaves the alert zone or the level stops running, and
    // start it again on returning to a level that is still in the alert zone (ALERT_ENTERED does not repeat).
    const bool ticking_wanted = v.alert && v.state == SessionState::PLAYING;
    if (m_TickingPlaying && !ticking_wanted)
    {
        m_Services.audio->stop_effect(SoundId::TICKING);
        m_TickingPlaying = false;
    }
    else if (!m_TickingPlaying && ticking_wanted && old.state == SessionState::PAUSED)
    {
        m_Services.audio->play_effect(SoundId::TICKING);
        m_TickingPlaying = true;
    }
}

void GameController::handle_event(const Event& event)
{
    HBM_ZONE("controller.handle_event");
    for (const SoundId sound : sounds_for_event(event))
    {
        m_Services.audio->play_effect(sound);
        if (sound == SoundId::TICKING)
        {
            m_TickingPlaying = true;
        }
    }

    const int ticks_per_second = m_Tuning->ticks_per_second;
    switch (event.kind)
    {
    case EventKind::LEVEL_STARTED:
        m_LevelAnswerPoints = 0;
        m_LevelAnswerCoins = 0;
        m_LevelStreakCoins = 0;
        m_LevelCountsKnown = true;
        emit levelStarted(event.level);
        break;
    case EventKind::QUESTION_SHOWN:
        emit questionShown(event.question_number);
        break;
    case EventKind::ANSWER_CORRECT:
        m_LevelAnswerPoints += event.points;
        m_LevelAnswerCoins += event.coins;
        m_LevelStreakCoins += event.streak_bonus_coins;
        m_CoinsEarned += event.coins;
        remember_answer(event, true);
        emit answerCorrect(event.tile, event.points, event.coins, event.streak_bonus_coins, event.run, event.multiplier,
                           event.multiplier_rose, event.ticks_delta / ticks_per_second);
        break;
    case EventKind::ANSWER_WRONG:
        remember_answer(event, false);
        emit answerWrong(event.tile, event.correct_tile, -event.ticks_delta / ticks_per_second, event.multiplier);
        break;
    case EventKind::HINT_USED:
        emit hintWasUsed(event.removed_tiles, -event.coins);
        break;
    case EventKind::ALERT_ENTERED:
        emit alertEntered();
        break;
    case EventKind::LEVEL_CLEARED:
        m_CoinsEarned += event.coins;
        m_LevelClear = {
            {QStringLiteral("level"), event.level},
            {QStringLiteral("correctAnswers"), event.correct_answers},
            {QStringLiteral("secondsLeft"), event.seconds_left},
            {QStringLiteral("timeBonusPoints"), event.time_bonus_points},
            {QStringLiteral("timeBonusPerSecond"), m_Tuning->time_bonus_points_per_second},
            {QStringLiteral("perfectBonusPoints"), event.perfect_bonus_points},
            {QStringLiteral("perfect"), event.perfect},
            {QStringLiteral("points"), event.points},
            {QStringLiteral("coins"), event.coins},
            {QStringLiteral("totalPoints"), static_cast<qlonglong>(event.total_points)},
            {QStringLiteral("wallet"), static_cast<qlonglong>(event.wallet)},
            {QStringLiteral("answerPoints"), m_LevelAnswerPoints},
            {QStringLiteral("answerCoins"), m_LevelAnswerCoins},
            {QStringLiteral("streakCoins"), m_LevelStreakCoins},
            {QStringLiteral("levelPoints"), m_LevelCountsKnown ? m_LevelAnswerPoints + event.points : 0},
            {QStringLiteral("levelCoins"), m_LevelCountsKnown ? m_LevelAnswerCoins + event.coins : 0},
            {QStringLiteral("complete"), m_LevelCountsKnown},
            {QStringLiteral("nextUp"), to_qstring(next_up_text(*m_Tuning, event.level + 1))},
        };
        emit levelClearChanged();
        emit levelCleared();
        persist();
        break;
    case EventKind::GAME_WON:
        m_CoinsEarned += event.coins;
        m_WinBonus = event.coins;
        emit winBonusChanged();
        emit gameWon(event.coins);
        persist();
        break;
    case EventKind::GAME_OVER:
        finish_game(event);
        break;
    }
}

void GameController::remember_answer(const Event& event, bool correct)
{
    HBM_ZONE("controller.remember_answer");
    const Question& q = m_Before.question;
    const QList<int> tiles = tiles_for(q);
    QVariantList tile_values;
    for (const int tile : tiles)
    {
        tile_values.append(tile);
    }
    m_Answered = {
        {QStringLiteral("questionNumber"), m_Before.question_number},
        {QStringLiteral("parts"), parts_for(q)},
        {QStringLiteral("tiles"), tile_values},
        {QStringLiteral("removedTiles"), m_Before.removed_tiles},
        {QStringLiteral("tile"), event.tile},
        {QStringLiteral("correctTile"), event.correct_tile},
        {QStringLiteral("correct"), correct},
    };
    emit answeredChanged();
}

void GameController::finish_game(const Event& event)
{
    HBM_ZONE("controller.finish_game");
    // The run this player paused is spent with the game. Another player's run stays.
    forget_own_saved_run();
    emit savedRunChanged();

    m_EndReason = event.reason == GameOverReason::BOMB   ? EndReason::Bomb
                  : event.reason == GameOverReason::QUIT ? EndReason::Quit
                                                         : EndReason::Finish;
    m_FinalScore = event.total_points;
    m_FinalLevel = event.level;

    // Every finished game counts towards the player's best, whether or not it earns a place in the top 10. NEW BEST!
    // means it beat the best from before this game, and a score of 0 never does.
    Player* player = current_player(m_Save);
    m_NewBest = player != nullptr && m_FinalScore > 0 && m_FinalScore > player->best;
    if (player != nullptr)
    {
        record_best(*player, m_FinalScore);
    }

    const std::optional<int> rank = rank_for_score(m_Save.shared.top10, m_FinalScore, *m_Tuning);
    m_NameRank = rank.value_or(0);
    m_HighlightRank = 0;
    int saved_rank = 0;
    if (rank.has_value() && player != nullptr && !player->name.empty())
    {
        // A player with a name gets no name screen: the entry goes in under that name at once.
        const std::string name = player->name;
        const std::string player_id = player->id;
        if (const auto placed = record_score(m_Save, player_id, name, m_FinalScore, *m_Tuning); placed.has_value())
        {
            saved_rank = *placed;
            m_NameRank = 0;
            m_HighlightRank = saved_rank;
        }
    }

    if (!m_ScoreSubmitted)
    {
        m_ScoreSubmitted = true;
        m_Services.scores->submit(m_FinalScore);
    }
    // Saved without a sync here: the rank the screens are told below must be the rank in the list as it is now. The
    // sync is the last thing this does.
    save_now();
    emit topScoresChanged();
    emit playersChanged();
    emit nameEntryChanged();
    emit gameOver(m_EndReason, static_cast<qint64>(m_FinalScore), m_NewBest, saved_rank != 0 ? saved_rank : m_NameRank);
    if (saved_rank != 0)
    {
        emit scoreSavedAtOnce(saved_rank);
    }
    request_cloud_sync();
}

void GameController::sync_driver()
{
    HBM_ZONE("controller.sync_driver");
    const bool should_run = m_Active && m_Session.view().state == SessionState::PLAYING;
    if (should_run && !m_Driver.running())
    {
        // Counted from now: the time spent on a card or the pause screen is not part of the level.
        m_Driver.start(m_Services.clock->now_ns());
        m_Timer.start();
    }
    else if (!should_run && m_Driver.running())
    {
        m_Driver.stop();
        m_Timer.stop();
    }
}

void GameController::update_music()
{
    HBM_ZONE("controller.update_music");
    MusicId wanted = MusicId::MENU;
    switch (m_Session.view().state)
    {
    case SessionState::PLAYING:
    case SessionState::PAUSED:
    case SessionState::LEVEL_CLEAR:
    case SessionState::WON:
        wanted = MusicId::PLAY;
        break;
    case SessionState::IDLE:
    case SessionState::GAME_OVER:
        break;
    }
    if (wanted != m_Music)
    {
        m_Music = wanted;
        m_Services.audio->play_music(wanted);
    }
}

void GameController::record_session_in_save()
{
    const SessionView& v = m_Session.view();
    if (Player* player = current_player(m_Save); player != nullptr)
    {
        // The wallet goes back to the player who played, as this device's earned or spent. A fall counts as coins
        // spent, so the session's wallet must never be older than the player's: a sync that changes the player's wallet
        // runs only where the session can take it, and takes it at once (sync_with_cloud_now).
        if (!record_wallet(*player, m_Save.device.id, v.wallet))
        {
            qCWarning(lcGame) << "The wallet could not be recorded";
        }
        player->won_once = v.won_once;
    }
}

void GameController::persist()
{
    HBM_ZONE("controller.persist");
    save_now();
    request_cloud_sync(); // a save is a trigger: it runs the sync where the session can take a changed wallet
}

void GameController::save_now()
{
    HBM_ZONE("controller.save_now");
    record_session_in_save();
    const SaveOutcome outcome = m_SaveManager.save(m_Save);
    switch (outcome)
    {
    case SaveOutcome::SAVED:
        break;
    case SaveOutcome::REFUSED_NEWER:
        break; // the file of a newer build is left as it is
    case SaveOutcome::NOT_LOADED:
    case SaveOutcome::INVALID:
    case SaveOutcome::STORE_FAILED:
        qCWarning(lcGame) << "The save was not written";
        break;
    }
}

void GameController::remember_run()
{
    HBM_ZONE("controller.remember_run");
    if (m_Session.view().state != SessionState::PAUSED)
    {
        return;
    }
    // The device keeps one run. Saving this player's replaces whoever's it was.
    m_Save.device.run = m_Session.snapshot();
    m_Save.device.run_player = m_Save.device.current_player;
    emit savedRunChanged();
}

void GameController::forget_saved_run()
{
    m_Save.device.run.reset();
    m_Save.device.run_player.clear();
}

void GameController::forget_own_saved_run()
{
    if (run_is_current_players())
    {
        forget_saved_run();
    }
}

bool GameController::run_is_current_players() const
{
    return m_Save.device.run.has_value() && m_Save.device.run_player == m_Save.device.current_player;
}

void GameController::take_current_player_into_session()
{
    if (const Player* player = current_player(m_Save); player != nullptr)
    {
        m_Session.set_wallet(player_wallet(*player));
        m_Session.set_won_once(player->won_once);
    }
}

void GameController::switch_to_player(const std::string& id)
{
    // The wallet goes back to the player who is leaving, as this device's earned or spent, before the session lets it
    // go.
    persist();
    if (!set_current_player(m_Save, id))
    {
        return;
    }
    take_current_player_into_session();
    refresh_view();          // the wallet and the coins of the new player reach the screens
    emit topScoresChanged(); // the best score is the player's
    emit savedRunChanged();  // and so is the run CONTINUE offers
    emit playersChanged();
    persist();
}

void GameController::remove_player_now(const std::string& id)
{
    if (!can_change_players())
    {
        return;
    }
    const bool was_current = id == m_Save.device.current_player;
    if (!remove_player(m_Save.shared, id))
    {
        return;
    }
    if (m_Save.device.run.has_value() && m_Save.device.run_player == id)
    {
        forget_saved_run();
    }
    if (was_current)
    {
        // Someone is always left: the last player cannot be removed. The empty id is never used for that reason.
        (void)ensure_current_player(m_Save, std::string(), *m_Tuning);
        take_current_player_into_session();
        refresh_view();
        emit topScoresChanged();
    }
    emit savedRunChanged();
    emit playersChanged();
    persist();
}

QString GameController::display_name_of(const std::string& id) const
{
    int place = 0;
    for (const Player& player : m_Save.shared.players)
    {
        if (player.removed)
        {
            continue;
        }
        ++place;
        if (player.id == id)
        {
            return player.name.empty() ? QStringLiteral("PLAYER %1").arg(place) : to_qstring(player.name);
        }
    }
    return {};
}

std::string GameController::trimmed_name(const std::string& normalised)
{
    const std::size_t first = normalised.find_first_not_of(' ');
    if (first == std::string::npos)
    {
        return {};
    }
    return normalised.substr(first, normalised.find_last_not_of(' ') - first + 1);
}

void GameController::assign_ids()
{
    // The ids come from the random source, not the core, which has none. Each is drawn only when it is needed, so a
    // source that is a fixed list of values is used in a known order.
    if (m_Save.device.id.empty() && !assign_device_id(m_Save, hex_id(m_IdSource())))
    {
        qCWarning(lcGame) << "The device id could not be assigned";
    }
    if (current_player(m_Save) == nullptr && !ensure_current_player(m_Save, hex_id(m_IdSource()), *m_Tuning))
    {
        qCWarning(lcGame) << "There is no current player and none could be made";
    }
}

// ---- iCloud sync -------------------------------------------------------------------------------------------------

bool GameController::cloud_sync_active() const
{
    // The switch is looked at first: with it off the store is not asked anything.
    return m_Save.device.cloud_sync && !m_SaveBlocked && m_Cloud->available();
}

void GameController::apply_cloud_sync()
{
    const bool available = m_Cloud->available(); // a local check, never a read of the cloud copy
    const bool wanted = m_Save.device.cloud_sync && !m_SaveBlocked && available;
    if (wanted != m_CloudWatching)
    {
        m_CloudWatching = wanted;
        if (wanted)
        {
            m_Cloud->set_change_handler([this] { request_cloud_sync(); });
        }
        else
        {
            m_Cloud->set_change_handler({});
            m_CloudOwed = false;
        }
    }
    if (available != m_CloudAvailableShown)
    {
        m_CloudAvailableShown = available;
        emit settingsChanged();
    }
}

bool GameController::can_take_synced_wallet() const
{
    // The session takes a wallet from the save only in these states (GameSession::set_wallet). The title also waits for
    // a name that is still to be typed, because the top 10 must not move under the name screen.
    switch (m_Session.view().state)
    {
    case SessionState::IDLE:
    case SessionState::PAUSED:
        return true;
    case SessionState::GAME_OVER:
        return m_NameRank == 0;
    case SessionState::PLAYING:
    case SessionState::LEVEL_CLEAR:
    case SessionState::WON:
        return false;
    }
    return false;
}

void GameController::request_cloud_sync()
{
    if (!cloud_sync_active())
    {
        return;
    }
    m_CloudOwed = true;
    run_owed_cloud_sync();
}

void GameController::run_owed_cloud_sync()
{
    if (!m_CloudOwed || m_CloudSyncing || !can_take_synced_wallet())
    {
        return;
    }
    m_CloudOwed = false;
    sync_with_cloud_now();
}

void GameController::announce_wallet()
{
    const std::int64_t wallet = m_Session.view().wallet;
    if (wallet != m_Shown.wallet)
    {
        m_Shown.wallet = wallet;
        emit walletChanged();
    }
}

void GameController::sync_with_cloud_now()
{
    HBM_ZONE("controller.cloud_sync");
    m_CloudSyncing = true;
    // What this device earned since it last saved goes into the player first, so the merge sees it.
    record_session_in_save();
    const std::vector<ScoreEntry> top10_before = m_Save.shared.top10;
    const CloudSyncResult result =
        sync_with_cloud(*m_Cloud, m_Save, [this] { return hex_id(m_IdSource()); }, *m_Tuning);
    m_CloudSyncing = false;

    if (result.outcome == CloudSyncOutcome::REFUSED)
    {
        qCWarning(lcGame) << "iCloud sync did nothing:"
                          << QString::fromUtf8(result.reason.data(), static_cast<qsizetype>(result.reason.size()));
    }
    else if (result.outcome == CloudSyncOutcome::SKIPPED_NEWER)
    {
        qCWarning(lcGame) << "The iCloud copy comes from a newer version of the game; it is left alone";
    }
    if (result.write_failed)
    {
        qCWarning(lcGame) << "iCloud did not take the players and scores; the next sync tries again";
    }
    if (!result.local_changed())
    {
        return;
    }

    // The merge changed the player's coins and win mark (and maybe who the current player is). The session takes them
    // now, here, at a moment it can: it must never hold an older wallet than the save, because the next save would book
    // the difference as coins spent.
    take_current_player_into_session();
    save_now();
    announce_wallet();
    if (m_HighlightRank != 0 && m_Save.shared.top10 != top10_before)
    {
        m_HighlightRank = 0; // the entry it marked may have moved
    }
    emit topScoresChanged();
    emit savedRunChanged();
    emit playersChanged();
}

void GameController::begin_gate(GatePurpose purpose)
{
    m_GatePurpose = purpose;
    m_GateChallenge = make_gate_challenge(m_GateSeedSource());
    emit gateChanged();
}

void GameController::end_gate()
{
    m_GatePurpose = GatePurpose::None;
    m_GateChallenge = {};
    m_GateUrl.clear();
    m_GatePlayer.clear();
    emit gateChanged();
}

void GameController::apply_settings()
{
    HBM_ZONE("controller.apply_settings");
    m_Services.audio->set_enabled(m_Save.device.sound, m_Save.device.music);
    m_Services.scores->set_enabled(m_Save.device.game_center && m_Services.scores->available());
    if (m_Services.haptics != nullptr)
    {
        m_Services.haptics->set_enabled(m_Save.device.haptics);
    }
}

void GameController::sync_haptics()
{
    // Nothing is felt while the app is in the background or the game is paused, and what was still to come is dropped.
    const bool quiet = !m_Active || m_Session.view().state == SessionState::PAUSED;
    if (quiet == m_HapticsQuiet)
    {
        return;
    }
    m_HapticsQuiet = quiet;
    if (m_Services.haptics == nullptr)
    {
        return;
    }
    if (quiet)
    {
        m_Services.haptics->suspend();
    }
    else
    {
        m_Services.haptics->resume();
    }
}

void GameController::play_haptic(HapticId id)
{
    if (m_Services.haptics != nullptr)
    {
        m_Services.haptics->play(id);
    }
}

GameController::State GameController::to_state(SessionState state) noexcept
{
    switch (state)
    {
    case SessionState::IDLE:
        return State::Idle;
    case SessionState::PLAYING:
        return State::Playing;
    case SessionState::PAUSED:
        return State::Paused;
    case SessionState::LEVEL_CLEAR:
        return State::LevelClear;
    case SessionState::WON:
        return State::Won;
    case SessionState::GAME_OVER:
        return State::GameOver;
    }
    return State::Idle;
}

// ---- Getters -----------------------------------------------------------------------------------------------------

GameController::State GameController::state() const
{
    return to_state(m_Session.view().state);
}

int GameController::level() const
{
    return m_Session.view().level;
}

bool GameController::endless() const
{
    return m_Session.view().endless;
}

int GameController::questions_per_level() const
{
    return m_Session.view().questions_per_level;
}

int GameController::ticks_total() const
{
    return m_Session.view().ticks_total;
}

int GameController::ticks_per_second() const
{
    return m_Tuning->ticks_per_second;
}

int GameController::alert_ticks() const
{
    const SessionView& v = m_Session.view();
    return m_Tuning->ticks(v.endless ? m_Tuning->endless.alert_seconds : m_Tuning->alert_seconds);
}

int GameController::seconds_per_right_answer() const
{
    return m_Session.view().seconds_per_right_answer;
}

int GameController::seconds_per_wrong_answer() const
{
    return m_Session.view().seconds_per_wrong_answer;
}

int GameController::correct_answers() const
{
    return m_Session.view().correct_answers;
}

int GameController::question_number() const
{
    return m_Session.view().question_number;
}

QStringList GameController::question_parts() const
{
    const SessionView& v = m_Session.view();
    return v.level == 0 ? QStringList{} : parts_for(v.question);
}

QString GameController::question_text() const
{
    return question_parts().join(QLatin1Char(' '));
}

QList<int> GameController::tiles() const
{
    const SessionView& v = m_Session.view();
    return v.level == 0 ? QList<int>{} : tiles_for(v.question);
}

int GameController::removed_tiles() const
{
    return m_Session.view().removed_tiles;
}

bool GameController::hint_used() const
{
    return m_Session.view().hint_used;
}

bool GameController::hint_available() const
{
    return m_Session.view().hint_available;
}

int GameController::hint_cost() const
{
    return m_Tuning->hint_cost_coins;
}

QVariantMap GameController::answered() const
{
    return m_Answered;
}

int GameController::ticks_left() const
{
    return m_Session.view().ticks_left;
}

int GameController::seconds_left() const
{
    return m_Session.view().seconds_left;
}

bool GameController::alert() const
{
    return m_Session.view().alert;
}

qint64 GameController::points() const
{
    return m_Session.view().points;
}

qint64 GameController::wallet() const
{
    return m_Session.view().wallet;
}

int GameController::run() const
{
    return m_Session.view().run;
}

int GameController::multiplier() const
{
    return m_Session.view().multiplier;
}

QVariantMap GameController::level_clear() const
{
    return m_LevelClear;
}

int GameController::win_bonus() const
{
    return m_WinBonus;
}

int GameController::campaign_levels() const
{
    return m_Tuning->campaign_levels;
}

int GameController::top10_size() const
{
    return m_Tuning->top10_size;
}

qint64 GameController::coins_earned() const
{
    return m_CoinsEarned;
}

bool GameController::coins_earned_known() const
{
    return m_CoinsEarnedKnown;
}

GameController::EndReason GameController::end_reason() const
{
    return m_EndReason;
}

qint64 GameController::final_score() const
{
    return m_FinalScore;
}

int GameController::final_level() const
{
    return m_FinalLevel;
}

bool GameController::new_best() const
{
    return m_NewBest;
}

bool GameController::name_entry_pending() const
{
    return m_NameRank != 0;
}

int GameController::name_entry_rank() const
{
    return m_NameRank;
}

int GameController::name_max_length() const
{
    return m_Tuning->name_max_length;
}

QVariantList GameController::top10() const
{
    QVariantList list;
    int rank = 1;
    for (const ScoreEntry& entry : m_Save.shared.top10)
    {
        list.append(QVariantMap{
            {QStringLiteral("rank"), rank},
            {QStringLiteral("name"), to_qstring(entry.name)},
            {QStringLiteral("score"), static_cast<qlonglong>(entry.score)},
        });
        ++rank;
    }
    return list;
}

qint64 GameController::best_score() const
{
    const Player* player = current_player(m_Save);
    return player != nullptr ? player->best : 0;
}

int GameController::highlight_rank() const
{
    return m_HighlightRank;
}

bool GameController::has_saved_run() const
{
    return run_is_current_players();
}

QVariantMap GameController::saved_run() const
{
    if (!run_is_current_players())
    {
        return {};
    }
    // Asking the core is the only way to read the snapshot without knowing its layout. A scratch session takes it, so
    // the one in play is untouched, and a run the core would refuse shows no caption.
    GameSession scratch(*m_Tuning, 0);
    if (!scratch.restore(*m_Save.device.run) || !is_resumable(scratch.view().state))
    {
        return {};
    }
    const SessionView& v = scratch.view();
    return {
        {QStringLiteral("level"), v.level},
        {QStringLiteral("secondsLeft"), v.seconds_left},
    };
}

QVariantList GameController::players() const
{
    QVariantList list;
    int place = 0;
    for (const Player& player : m_Save.shared.players)
    {
        if (player.removed)
        {
            continue;
        }
        ++place;
        const bool is_current = player.id == m_Save.device.current_player;
        const bool named = !player.name.empty();
        // The current player's coins are the session's, which are the same as the save's whenever the title shows.
        const std::int64_t coins = is_current ? m_Session.view().wallet : player_wallet(player);
        list.append(QVariantMap{
            {QStringLiteral("id"), to_qstring(player.id)},
            {QStringLiteral("name"), to_qstring(player.name)},
            {QStringLiteral("displayName"), named ? to_qstring(player.name) : QStringLiteral("PLAYER %1").arg(place)},
            {QStringLiteral("named"), named},
            {QStringLiteral("best"), static_cast<qlonglong>(player.best)},
            {QStringLiteral("coins"), static_cast<qlonglong>(coins)},
            {QStringLiteral("current"), is_current},
        });
    }
    return list;
}

QString GameController::player_name() const
{
    return display_name_of(m_Save.device.current_player);
}

QString GameController::player_id() const
{
    return to_qstring(m_Save.device.current_player);
}

int GameController::max_players() const
{
    return m_Tuning->max_players;
}

bool GameController::can_change_players() const
{
    const SessionState state = m_Session.view().state;
    return (state == SessionState::IDLE || state == SessionState::GAME_OVER) && m_NameRank == 0;
}

bool GameController::can_add_player() const
{
    return active_player_count(m_Save.shared) < m_Tuning->max_players &&
           m_Save.shared.players.size() < MAX_SAVED_PLAYERS;
}

bool GameController::can_remove_player() const
{
    return active_player_count(m_Save.shared) > 1;
}

QString GameController::gate_player_name() const
{
    return m_GatePurpose == GatePurpose::RemovePlayer ? display_name_of(m_GatePlayer) : QString();
}

int GameController::first_level_questions() const
{
    return m_Tuning->questions_per_level;
}

int GameController::first_level_seconds() const
{
    return m_Tuning->level_seconds;
}

bool GameController::sound_enabled() const
{
    return m_Save.device.sound;
}

bool GameController::music_enabled() const
{
    return m_Save.device.music;
}

bool GameController::game_center_enabled() const
{
    return m_Save.device.game_center && game_center_available();
}

bool GameController::game_center_available() const
{
    return m_Services.scores->available();
}

bool GameController::haptics_enabled() const
{
    return m_Save.device.haptics;
}

bool GameController::haptics_available() const
{
    return m_Services.haptics != nullptr && m_Services.haptics->available();
}

bool GameController::cloud_sync_enabled() const
{
    return cloud_sync_active();
}

bool GameController::cloud_sync_available() const
{
    return m_Cloud->available();
}

bool GameController::gate_active() const
{
    return m_GatePurpose != GatePurpose::None;
}

GameController::GatePurpose GameController::gate_purpose() const
{
    return m_GatePurpose;
}

QString GameController::gate_words() const
{
    return to_qstring(m_GateChallenge.words);
}

QString GameController::gate_url() const
{
    return to_qstring(m_GateUrl);
}

int GameController::gate_digits() const
{
    return GATE_NUMBER_DIGITS;
}

bool GameController::save_blocked() const
{
    return m_SaveBlocked;
}

QString GameController::version_text() const
{
    return to_qstring(m_BuildInfo.footer());
}

QVariantMap GameController::build_info() const
{
    return {
        {QStringLiteral("appName"), to_qstring(m_BuildInfo.app_name)},
        {QStringLiteral("version"), to_qstring(m_BuildInfo.version)},
        {QStringLiteral("buildNumber"), m_BuildInfo.build_number},
        {QStringLiteral("commit"), to_qstring(m_BuildInfo.commit)},
        {QStringLiteral("branch"), to_qstring(m_BuildInfo.branch)},
        {QStringLiteral("date"), to_qstring(m_BuildInfo.date)},
        {QStringLiteral("qtVersion"), to_qstring(m_BuildInfo.qt_version)},
    };
}

QString GameController::notices() const
{
    return notices_text();
}

QString GameController::privacy_policy_url() const
{
    return QString::fromLatin1(PRIVACY_POLICY_URL);
}

QString GameController::repository_url() const
{
    return QString::fromLatin1(REPOSITORY_URL);
}

} // namespace heart_boom_maths::game
