// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "heart_boom_maths/version.hpp"
#include "services/clock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <functional>
#include <memory>
#include <vector>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;
using State = GameController::State;

namespace
{

constexpr std::uint64_t FIRST_GATE_SEED = 11;
constexpr std::uint64_t SECOND_GATE_SEED = 12;
constexpr std::uint64_t FIRST_ID_BITS = 0x9F1C2A7B3D4E5F60ULL;
constexpr std::uint64_t SECOND_ID_BITS = 0x00000000000000FFULL;
constexpr std::string_view FIRST_ID = "9f1c2a7b3d4e5f60";

/// One controller with every service recorded. The save store is passed in so a second rig can read what the first
/// one wrote, as a restart does.
struct Rig
{
    explicit Rig(MemorySaveStore& memory, bool score_service_available = false, std::int64_t guard_ns = 0)
        : scores(score_service_available), controller(make_services(memory), make_options(guard_ns))
    {
    }

    GameServices make_services(MemorySaveStore& memory)
    {
        GameServices services;
        services.store = &memory;
        services.audio = &audio;
        services.scores = &scores;
        services.haptics = &haptics;
        services.links = &links;
        services.clock = &clock;
        return services;
    }

    GameOptions make_options(std::int64_t guard_ns)
    {
        GameOptions options;
        options.session_seed = 20261114;
        options.tile_guard_ns = guard_ns; // most tests tap straight after a level starts; the guard has its own tests
        options.gate_seed_source = [this] { return gate_seeds_used++ == 0 ? FIRST_GATE_SEED : SECOND_GATE_SEED; };
        // The first draw is the device's id; every later one is another id, so a test can add several players.
        options.id_source = [this]
        {
            if (id_override)
            {
                return id_override();
            }
            return id_draws++ == 0 ? FIRST_ID_BITS : SECOND_ID_BITS + static_cast<std::uint64_t>(id_draws - 2);
        };
        return options;
    }

    /// Plays real time in 50 ms frames, the length of one tick.
    void play_for_ms(int milliseconds)
    {
        for (int elapsed = 0; elapsed < milliseconds; elapsed += 50)
        {
            clock.advance_ms(50);
            controller.advance_time();
        }
    }

    void answer_right()
    {
        controller.answer(controller.session_view().question.correct_tile);
    }

    void answer_wrong()
    {
        const SessionView& v = controller.session_view();
        for (int tile = 0; tile < 4; ++tile)
        {
            if (tile != v.question.correct_tile && ((v.removed_tiles >> tile) & 1) == 0)
            {
                controller.answer(tile);
                return;
            }
        }
    }

    void play_level_right()
    {
        for (int i = 0; i < 10; ++i)
        {
            answer_right();
        }
    }

    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingHaptics haptics;
    RecordingLinks links;
    int gate_seeds_used = 0;
    int id_draws = 0;
    std::function<std::uint64_t()> id_override; // set by a test after the device id is drawn
    GameController controller;
};

SaveData saved(const MemorySaveStore& memory)
{
    REQUIRE(memory.text().has_value());
    const ParseResult parsed = parse_save(*memory.text());
    REQUIRE(parsed.status == LoadStatus::OK);
    return parsed.data;
}

} // namespace

TEST_CASE("A fresh controller shows the title screen on defaults", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    const GameController& c = rig.controller;
    CHECK(c.state() == State::Idle);
    CHECK(c.wallet() == 0);
    CHECK(c.points() == 0);
    CHECK(c.best_score() == 0);
    CHECK_FALSE(c.has_saved_run());
    CHECK(c.sound_enabled());
    CHECK(c.music_enabled());
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(c.game_center_available());
    CHECK_FALSE(c.gate_active());
    CHECK_FALSE(c.save_blocked());
    CHECK(c.question_text().isEmpty());
    CHECK(c.tiles().isEmpty());
    CHECK(c.top10().isEmpty());
    CHECK(c.hint_cost() == Tuning::defaults().hint_cost_coins);
    CHECK(c.name_max_length() == Tuning::defaults().name_max_length);
    CHECK(c.version_text().startsWith(QStringLiteral("Heart Boom Maths ")));
    CHECK(c.build_info().value(QStringLiteral("version")).toString().toStdString() ==
          std::string(heart_boom_maths::VERSION_STRING));
    CHECK_FALSE(c.notices().isEmpty());
    CHECK(rig.audio.music == std::vector<MusicId>{MusicId::MENU});
    CHECK_FALSE(memory.text().has_value()); // nothing is written before there is something to keep
}

TEST_CASE("The settings and the wallet in the save reach the game and the services", "[integration][controller]")
{
    SaveData data;
    give_wallet(data, 40);
    data.shared.players[0].won_once = true;
    data.device.sound = false;
    data.device.music = false;
    data.device.game_center = true;
    give_top10(data, {{"ADA", 900}, {"BEN", 500}});
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory, true);
    CHECK(rig.controller.wallet() == 40);
    CHECK(rig.controller.session_view().won_once);
    CHECK_FALSE(rig.controller.sound_enabled());
    CHECK_FALSE(rig.controller.music_enabled());
    CHECK_FALSE(rig.audio.effects_enabled);
    CHECK_FALSE(rig.audio.music_enabled);
    CHECK(rig.controller.game_center_enabled());
    CHECK(rig.scores.enabled);
    CHECK(rig.controller.best_score() == 900);
    CHECK(rig.controller.top10().size() == 2);
}

TEST_CASE("Starting a game shows the first question as the child reads it", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    int level_started = 0;
    int questions = 0;
    QObject::connect(&c, &GameController::levelStarted, [&](int level) { level_started += level; });
    QObject::connect(&c, &GameController::questionShown, [&](int) { ++questions; });
    QSignalSpy state_spy(&c, &GameController::stateChanged);

    c.startGame();
    CHECK(c.state() == State::Playing);
    CHECK(state_spy.count() == 1);
    CHECK(c.level() == 1);
    CHECK_FALSE(c.endless());
    CHECK(c.questions_per_level() == 10);
    CHECK(c.question_number() == 1);
    CHECK(c.ticks_left() == 1200);
    CHECK(c.ticks_total() == 1200);
    CHECK(c.seconds_left() == 60);
    CHECK(c.seconds_per_right_answer() == 7);
    CHECK(c.seconds_per_wrong_answer() == 3);
    CHECK(c.multiplier() == 1);
    CHECK(c.tiles().size() == 4);
    CHECK(level_started == 1);
    CHECK(questions == 1);
    CHECK(rig.audio.music.back() == MusicId::PLAY);

    // Level 1 is "a + b = ?" with the numbers from the question on screen.
    const Question& q = c.session_view().question;
    CHECK(c.question_text() == QStringLiteral("%1 + %2 = ?").arg(q.terms[0]).arg(q.terms[1]));
    CHECK(c.question_parts() == QStringList{QString::number(q.terms[0]), QStringLiteral("+"),
                                            QString::number(q.terms[1]), QStringLiteral("="), QStringLiteral("?")});
    CHECK(c.tiles()[q.correct_tile] == q.answer);

    c.startGame(); // ignored while a game is on
    CHECK(state_spy.count() == 1);
}

TEST_CASE("Question text covers every shape", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();

    // Levels 5 (missing addend), 8 (a + b - c) and 10 (four terms) by right answers; the timer never runs.
    for (int level = 1; level <= 10; ++level)
    {
        REQUIRE(c.level() == level);
        const Question& q = c.session_view().question;
        const QStringList parts = c.question_parts();
        switch (q.shape)
        {
        case QuestionShape::TWO_TERM:
            CHECK(parts.size() == 5);
            break;
        case QuestionShape::MISSING_ADDEND:
            CHECK(c.question_text() == QStringLiteral("%1 + ? = %2").arg(q.terms[0]).arg(q.terms[1]));
            break;
        case QuestionShape::THREE_TERM:
            CHECK(parts.size() == 7);
            break;
        case QuestionShape::FOUR_TERM:
            CHECK(parts.size() == 9);
            CHECK(parts.last() == QStringLiteral("?"));
            break;
        }
        rig.play_level_right();
        c.nextLevel();
    }
}

TEST_CASE("Real time becomes whole ticks", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    QSignalSpy time_spy(&c, &GameController::timeChanged);

    rig.clock.advance_ms(49);
    c.advance_time();
    CHECK(c.ticks_left() == 1200);
    rig.clock.advance_ms(1);
    c.advance_time();
    CHECK(c.ticks_left() == 1199);

    rig.play_for_ms(1000);
    CHECK(c.ticks_left() == 1179);
    CHECK(c.seconds_left() == 58);
    CHECK(time_spy.count() > 0);

    // A stall of ten seconds costs only the catch-up limit.
    rig.clock.advance_ms(10'000);
    c.advance_time();
    CHECK(c.ticks_left() == 1179 - MAX_CATCH_UP_TICKS);
}

TEST_CASE("Pausing stops time and resuming does not count the pause", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_for_ms(500);
    const int before = c.ticks_left();

    c.pause();
    CHECK(c.state() == State::Paused);
    rig.play_for_ms(30'000);
    CHECK(c.ticks_left() == before);
    CHECK(c.has_saved_run());

    c.resume();
    CHECK(c.state() == State::Playing);
    CHECK_FALSE(c.has_saved_run());
    rig.play_for_ms(100);
    CHECK(c.ticks_left() == before - 2);
    CHECK_FALSE(saved(memory).device.run.has_value());
}

TEST_CASE("A right and a wrong answer move the score, the timer and the sounds", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();

    int right_points = 0;
    int wrong_lost = 0;
    QObject::connect(&c, &GameController::answerCorrect,
                     [&](int, int points, int, int, int, int, bool, int) { right_points = points; });
    QObject::connect(&c, &GameController::answerWrong, [&](int, int, int lost, int) { wrong_lost = lost; });
    QSignalSpy points_spy(&c, &GameController::pointsChanged);
    QSignalSpy question_spy(&c, &GameController::questionChanged);

    rig.answer_right();
    CHECK(right_points == 10);
    CHECK(c.points() == 10);
    CHECK(c.wallet() == 1);
    CHECK(c.run() == 1);
    CHECK(c.correct_answers() == 1);
    CHECK(c.question_number() == 2);
    CHECK(points_spy.count() == 1);
    CHECK(question_spy.count() == 1);
    CHECK(rig.audio.count(SoundId::COIN_CHIME) == 1);

    rig.answer_wrong();
    CHECK(wrong_lost == 3);
    CHECK(c.run() == 0);
    CHECK(c.points() == 10);
    CHECK(c.question_number() == 3);
    CHECK(rig.audio.count(SoundId::SOFT_BUZZ) == 1);
    CHECK(c.ticks_left() == 1200 - 60 + 0); // the right answer was capped at the full timer, the wrong one cost 3 s
}

TEST_CASE("The rising tone plays with the chime when the multiplier rises", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    bool rose = false;
    QObject::connect(&c, &GameController::answerCorrect,
                     [&](int, int, int, int, int, int, bool multiplier_rose, int) { rose = rose || multiplier_rose; });
    rig.answer_right();
    rig.answer_right();
    CHECK(rig.audio.count(SoundId::RISING_TONE) == 0);
    rig.answer_right(); // a run of 3 lifts the step to x2
    CHECK(rose);
    CHECK(c.multiplier() == 2);
    CHECK(rig.audio.count(SoundId::RISING_TONE) == 1);
    CHECK(rig.audio.count(SoundId::COIN_CHIME) == 3);
}

TEST_CASE("A hint costs coins, removes tiles and whooshes", "[integration][controller]")
{
    SaveData data;
    give_wallet(data, 10);
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    CHECK(c.hint_available());
    int removed = 0;
    int cost = 0;
    QObject::connect(&c, &GameController::hintWasUsed,
                     [&](int removed_tiles, int price)
                     {
                         removed = removed_tiles;
                         cost = price;
                     });
    QSignalSpy removed_spy(&c, &GameController::removedTilesChanged);
    c.useHint();
    CHECK(c.wallet() == 8);
    CHECK(cost == 2);
    CHECK(c.hint_used());
    CHECK_FALSE(c.hint_available());
    CHECK(c.removed_tiles() == removed);
    CHECK(removed_spy.count() == 1);
    CHECK(rig.audio.count(SoundId::WHOOSH) == 1);
}

TEST_CASE("A cleared level shows its card and saves the wallet", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    int cleared = 0;
    QObject::connect(&c, &GameController::levelCleared, [&] { ++cleared; });
    QSignalSpy card_spy(&c, &GameController::levelClearChanged);

    rig.play_level_right();
    CHECK(c.state() == State::LevelClear);
    CHECK(cleared == 1);
    CHECK(card_spy.count() == 1);
    const QVariantMap card = c.level_clear();
    CHECK(card.value("level").toInt() == 1);
    CHECK(card.value("perfect").toBool());
    CHECK(card.value("secondsLeft").toInt() == 60);
    CHECK(card.value("timeBonusPoints").toInt() == 600);
    CHECK(card.value("perfectBonusPoints").toInt() == 100);
    CHECK(card.value("coins").toInt() == 5);
    CHECK(rig.audio.count(SoundId::FANFARE) == 1);

    // 10 right answers (10 coins and one streak bonus of 1 at the fifth and tenth) and 5 for the perfect level.
    CHECK(c.wallet() == wallet_of(saved(memory)));
    CHECK(c.wallet() == 10 + 2 + 5);

    rig.play_for_ms(5000); // the card has no clock
    CHECK(c.ticks_left() == 1200);
    c.nextLevel();
    CHECK(c.state() == State::Playing);
    CHECK(c.level() == 2);
}

TEST_CASE("Clearing level 12 wins, pays once and either keeps going or finishes", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    int bonus = -1;
    QObject::connect(&c, &GameController::gameWon, [&](int coins) { bonus = coins; });
    for (int level = 1; level <= 12; ++level)
    {
        rig.play_level_right();
        if (level < 12)
        {
            c.nextLevel();
        }
    }
    CHECK(c.state() == State::Won);
    CHECK(bonus == 1000);
    CHECK(rig.audio.count(SoundId::FANFARE) == 12); // the win sends GAME_WON as well and adds no second fanfare
    CHECK(current_player(saved(memory))->won_once);
    CHECK(wallet_of(saved(memory)) >= 1000);

    c.keepGoing();
    CHECK(c.state() == State::Playing);
    CHECK(c.level() == 13);
    CHECK(c.endless());

    // A second win elsewhere would pay nothing; here FINISH from level 12 is shown through a fresh rig.
    MemorySaveStore other;
    Rig again(other);
    again.controller.startGame();
    for (int level = 1; level <= 12; ++level)
    {
        again.play_level_right();
        if (level < 12)
        {
            again.controller.nextLevel();
        }
    }
    again.controller.finish();
    CHECK(again.controller.state() == State::GameOver);
    CHECK(again.controller.end_reason() == GameController::EndReason::Finish);
    CHECK(again.scores.submitted.size() == 1);
    CHECK(again.scores.submitted.front() == again.controller.final_score());
}

TEST_CASE("A whole scripted game runs to the bomb with the manual clock", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    rig.answer_right();
    int alerts = 0;
    int over_signals = 0;
    GameController::EndReason reason = GameController::EndReason::Quit;
    QObject::connect(&c, &GameController::alertEntered, [&] { ++alerts; });
    QObject::connect(&c, &GameController::gameOver,
                     [&](GameController::EndReason r, qint64, bool, int)
                     {
                         ++over_signals;
                         reason = r;
                     });

    int frames = 0;
    while (c.state() == State::Playing && frames < 2000)
    {
        rig.play_for_ms(50);
        ++frames;
    }
    CHECK(c.state() == State::GameOver);
    // 1200 ticks at one a frame, less the 7 s the first two answers could not add (the timer was full).
    CHECK(frames == 1200);
    CHECK(alerts == 1);
    CHECK(over_signals == 1);
    CHECK(reason == GameController::EndReason::Bomb);
    CHECK(c.ticks_left() == 0);
    CHECK(c.final_score() == 20);
    CHECK(c.final_level() == 1);
    CHECK(c.new_best());
    CHECK(c.name_entry_pending());
    CHECK(c.name_entry_rank() == 1);

    CHECK(rig.audio.count(SoundId::TICKING) == 1);
    CHECK(rig.audio.count(SoundId::CARTOON_POP) == 1);
    CHECK(rig.audio.stopped.size() == 1); // the ticking ends with the game
    CHECK(rig.audio.music.back() == MusicId::MENU);

    // Once, with the final score, and the driver is stopped for good.
    REQUIRE(rig.scores.submitted.size() == 1);
    CHECK(rig.scores.submitted.front() == 20);
    rig.play_for_ms(5000);
    CHECK(rig.scores.submitted.size() == 1);
    CHECK(c.ticks_left() == 0);
    CHECK(wallet_of(saved(memory)) == 2);
}

TEST_CASE("Leaving the alert zone stops the ticking, and a pause in it starts it again", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_for_ms(51'000);
    REQUIRE(c.alert());
    CHECK(rig.audio.count(SoundId::TICKING) == 1);

    c.pause();
    CHECK(rig.audio.stopped.size() == 1);
    c.resume();
    CHECK(rig.audio.count(SoundId::TICKING) == 2);

    rig.answer_right(); // +7 s lifts the timer out of the last 10
    CHECK_FALSE(c.alert());
    CHECK(rig.audio.stopped.size() == 2);
}

TEST_CASE("QUIT on the pause screen ends the game and offers the name entry once", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    CHECK(c.state() == State::GameOver);
    CHECK(c.end_reason() == GameController::EndReason::Quit);
    CHECK(c.final_score() == 10);
    CHECK_FALSE(c.has_saved_run());
    CHECK_FALSE(saved(memory).device.run.has_value());
    CHECK(rig.scores.submitted == std::vector<std::int64_t>{10});
    CHECK(rig.audio.count(SoundId::CARTOON_POP) == 1);

    REQUIRE(c.name_entry_pending());
    QSignalSpy top_spy(&c, &GameController::topScoresChanged);
    CHECK(c.normaliseName(QStringLiteral("ada lovelace 1!")) == QStringLiteral("ADA LOVELACE"));
    CHECK(c.saveName(QString()) == 0); // an empty name does not save
    CHECK(c.name_entry_pending());
    CHECK(c.saveName(QStringLiteral("ada lovelace 1!")) == 1);
    CHECK_FALSE(c.name_entry_pending());
    CHECK(top_spy.count() == 1);
    CHECK(c.highlight_rank() == 1);
    CHECK(c.best_score() == 10);
    CHECK(c.top10().first().toMap().value("name").toString() == QStringLiteral("ADA LOVELACE"));
    const SaveData on_disk = saved(memory);
    REQUIRE(on_disk.shared.top10.size() == 1);
    CHECK(on_disk.shared.top10[0] == ScoreEntry{"ADA LOVELACE", 10, std::string(FIRST_ID), 1});
    CHECK(on_disk.shared.players[0].best == 10);
    CHECK(on_disk.device.next_seq == 2);
    CHECK(c.saveName(QStringLiteral("again")) == 0); // already saved

    // Starting again clears the leftovers of the last game.
    c.startGame();
    CHECK(c.highlight_rank() == 0);
    CHECK_FALSE(c.name_entry_pending());
}

TEST_CASE("SKIP drops the entry, and a score of nothing never qualifies", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    REQUIRE(c.name_entry_pending());
    c.skipName();
    CHECK_FALSE(c.name_entry_pending());
    CHECK(c.top10().isEmpty());
    c.skipName(); // nothing pending: no effect

    c.startGame();
    c.pause();
    c.quit();
    CHECK(c.final_score() == 0);
    CHECK_FALSE(c.name_entry_pending());
    CHECK_FALSE(c.new_best());
    CHECK(rig.scores.submitted.size() == 2);
}

TEST_CASE("Leaving the foreground pauses, suspends audio and writes the save", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    rig.play_for_ms(500);
    const int before = c.ticks_left();
    const int stores = memory.store_count();

    c.on_application_state(false);
    CHECK(c.state() == State::Paused);
    CHECK(rig.audio.suspends == 1);
    CHECK(memory.store_count() > stores);
    CHECK(saved(memory).device.run.has_value());
    CHECK(wallet_of(saved(memory)) == 1);

    rig.play_for_ms(10'000);
    CHECK(c.ticks_left() == before);

    // Coming back resumes the sound only: the game stays on the pause screen with the clock stopped.
    c.on_application_state(true);
    CHECK(rig.audio.resumes == 1);
    CHECK(c.state() == State::Paused);
    rig.play_for_ms(10'000);
    CHECK(c.ticks_left() == before);

    c.resume();
    rig.play_for_ms(100);
    CHECK(c.ticks_left() == before - 2);
}

TEST_CASE("Leaving the foreground on the title screen or a card writes the save and pauses nothing",
          "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.on_application_state(false);
    CHECK(c.state() == State::Idle);
    CHECK(memory.text().has_value());
    c.on_application_state(true);

    c.startGame();
    rig.play_level_right();
    c.on_application_state(false);
    CHECK(c.state() == State::LevelClear);
    CHECK_FALSE(saved(memory).device.run.has_value());
    c.on_application_state(true);
    c.nextLevel();
    CHECK(c.state() == State::Playing);
}

TEST_CASE("CONTINUE restores the paused run once", "[integration][controller]")
{
    MemorySaveStore memory;
    std::int64_t points = 0;
    int ticks = 0;
    int level = 0;
    QStringList parts;
    QList<int> tiles;
    {
        Rig first(memory);
        first.controller.startGame();
        first.play_level_right();
        first.controller.nextLevel();
        first.answer_right();
        first.answer_wrong();
        first.play_for_ms(2000);
        first.controller.pause();
        points = first.controller.points();
        ticks = first.controller.ticks_left();
        level = first.controller.level();
        parts = first.controller.question_parts();
        tiles = first.controller.tiles();
    }
    REQUIRE(saved(memory).device.run.has_value());

    Rig second(memory);
    GameController& c = second.controller;
    CHECK(c.state() == State::Idle);
    CHECK(c.has_saved_run());
    QSignalSpy run_spy(&c, &GameController::savedRunChanged);

    REQUIRE(c.continueGame());
    CHECK(c.state() == State::Paused); // the pause screen: nothing runs until RESUME
    CHECK(c.points() == points);
    CHECK(c.ticks_left() == ticks);
    CHECK(c.level() == level);
    CHECK(c.question_parts() == parts);
    CHECK(c.tiles() == tiles);
    CHECK_FALSE(c.has_saved_run());
    CHECK(run_spy.count() == 1);
    CHECK_FALSE(saved(memory).device.run.has_value()); // deleted as soon as it loads
    second.play_for_ms(5000);
    CHECK(c.ticks_left() == ticks);

    CHECK_FALSE(c.continueGame()); // resumes once

    c.resume();
    second.play_for_ms(100);
    CHECK(c.ticks_left() == ticks - 2);
}

TEST_CASE("A paused run that is paused again after CONTINUE is saved again on leaving", "[integration][controller]")
{
    MemorySaveStore memory;
    {
        Rig first(memory);
        first.controller.startGame();
        first.controller.pause();
    }
    Rig second(memory);
    REQUIRE(second.controller.continueGame());
    second.controller.on_application_state(false);
    CHECK(saved(memory).device.run.has_value());
    CHECK(second.controller.has_saved_run());
}

TEST_CASE("A saved run that cannot be restored starts clean", "[integration][controller]")
{
    SaveData data;
    give_wallet(data, 3);
    give_run(data, {1, 2, 3});
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.has_saved_run());
    int failed = 0;
    QObject::connect(&c, &GameController::continueFailed, [&] { ++failed; });

    CHECK_FALSE(c.continueGame());
    CHECK(failed == 1);
    CHECK(c.state() == State::Idle);
    CHECK_FALSE(c.has_saved_run());
    CHECK(c.wallet() == 3);
    CHECK_FALSE(saved(memory).device.run.has_value());

    c.startGame(); // the game is fine
    CHECK(c.state() == State::Playing);
}

TEST_CASE("CONTINUE does nothing without a run, and NEW GAME discards one", "[integration][controller]")
{
    MemorySaveStore memory;
    {
        Rig first(memory);
        first.controller.startGame();
        first.controller.pause();
    }
    Rig rig(memory);
    REQUIRE(rig.controller.has_saved_run());
    rig.controller.startGame();
    CHECK_FALSE(rig.controller.has_saved_run());
    CHECK_FALSE(saved(memory).device.run.has_value());
    CHECK_FALSE(rig.controller.continueGame());
    CHECK(rig.controller.state() == State::Playing);
}

TEST_CASE("The settings save at once and reach the audio service", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    QSignalSpy spy(&c, &GameController::settingsChanged);

    c.setSoundEnabled(false);
    CHECK_FALSE(rig.audio.effects_enabled);
    CHECK(rig.audio.music_enabled);
    CHECK_FALSE(saved(memory).device.sound);
    c.setMusicEnabled(false);
    CHECK_FALSE(rig.audio.music_enabled);
    CHECK_FALSE(saved(memory).device.music);
    CHECK(spy.count() == 2);

    c.setSoundEnabled(false); // no change, no signal
    CHECK(spy.count() == 2);
}

TEST_CASE("A link opens only after the grown-up types the number back", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    QSignalSpy gate_spy(&c, &GameController::gateChanged);
    QSignalSpy failed_spy(&c, &GameController::gateFailed);
    QSignalSpy passed_spy(&c, &GameController::gatePassed);

    CHECK_FALSE(c.requestLink(QStringLiteral("http://example.org/plain")));
    CHECK_FALSE(c.requestLink(QStringLiteral("file:///etc/passwd")));
    CHECK_FALSE(c.gate_active());
    CHECK(c.submitGate(QStringLiteral("1")) == false); // no gate open

    REQUIRE(c.requestLink(QStringLiteral("https://example.org/privacy")));
    CHECK(c.gate_active());
    CHECK(c.gate_purpose() == GameController::GatePurpose::Link);
    const GateChallenge first = make_gate_challenge(FIRST_GATE_SEED);
    CHECK(c.gate_words().toStdString() == first.words);
    CHECK(rig.links.opened.empty());

    // A wrong answer opens nothing and deals a new number.
    CHECK_FALSE(c.submitGate(QString::number(first.number + 1)));
    CHECK(rig.links.opened.empty());
    CHECK(c.gate_active());
    CHECK(failed_spy.count() == 1);
    const GateChallenge second = make_gate_challenge(SECOND_GATE_SEED);
    CHECK(c.gate_words().toStdString() == second.words);
    CHECK_FALSE(c.submitGate(QString::number(first.number))); // the old number no longer works
    CHECK(rig.links.opened.empty());

    CHECK(c.submitGate(QString::number(second.number)));
    CHECK(rig.links.opened == std::vector<std::string>{"https://example.org/privacy"});
    CHECK_FALSE(c.gate_active());
    CHECK(c.gate_words().isEmpty());
    CHECK(passed_spy.count() == 1);
    CHECK(gate_spy.count() >= 3);

    // The gate is spent: the same answer opens nothing a second time.
    CHECK_FALSE(c.submitGate(QString::number(second.number)));
    CHECK(rig.links.opened.size() == 1);
}

TEST_CASE("Cancelling the gate opens nothing", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.requestLink(QStringLiteral("HTTPS://example.org")));
    c.cancelGate();
    CHECK_FALSE(c.gate_active());
    CHECK(rig.links.opened.empty());
    c.cancelGate(); // already closed
}

TEST_CASE("Game Center is unavailable without a leaderboard", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    rig.controller.requestGameCenter(true);
    CHECK_FALSE(rig.controller.gate_active());
    CHECK_FALSE(rig.controller.game_center_enabled());
    CHECK_FALSE(rig.scores.enabled);
}

TEST_CASE("Game Center turns on only behind the gate and off at once", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory, true);
    GameController& c = rig.controller;
    REQUIRE(c.game_center_available());
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(rig.scores.enabled);

    c.requestGameCenter(false); // already off: nothing happens
    CHECK_FALSE(c.gate_active());

    c.requestGameCenter(true);
    REQUIRE(c.gate_active());
    CHECK(c.gate_purpose() == GameController::GatePurpose::GameCenter);
    CHECK_FALSE(c.game_center_enabled());
    const GateChallenge challenge = make_gate_challenge(FIRST_GATE_SEED);

    CHECK_FALSE(c.submitGate(QString::number(challenge.number + 1)));
    CHECK_FALSE(c.game_center_enabled());
    c.cancelGate();
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(memory.text().has_value()); // nothing was turned on, so nothing was written

    c.requestGameCenter(true);
    CHECK(c.submitGate(QString::number(make_gate_challenge(SECOND_GATE_SEED).number)));
    CHECK(c.game_center_enabled());
    CHECK(rig.scores.enabled);
    CHECK(saved(memory).device.game_center);
    CHECK(rig.links.opened.empty());

    c.requestGameCenter(true); // already on: no gate
    CHECK_FALSE(c.gate_active());

    c.requestGameCenter(false);
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(rig.scores.enabled);
    CHECK_FALSE(saved(memory).device.game_center);
}

TEST_CASE("A store that cannot write is survived", "[integration][controller]")
{
    MemorySaveStore memory;
    memory.set_store_fails(true);
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    CHECK(c.state() == State::GameOver);
    CHECK(c.saveName(QStringLiteral("kit")) == 1); // the list is kept in memory for this session
    CHECK(c.top10().size() == 1);
}

TEST_CASE("Commands in the wrong state do nothing", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.answer(0);
    c.useHint();
    c.pause();
    c.resume();
    c.quit();
    c.nextLevel();
    c.keepGoing();
    c.finish();
    c.advance_time();
    CHECK(c.state() == State::Idle);
    CHECK_FALSE(memory.text().has_value());
    CHECK_FALSE(c.continueGame());
}

TEST_CASE("The controller registers its types and properties with Qt", "[integration][controller]")
{
    const QMetaObject& meta = GameController::staticMetaObject;
    for (const char* name : {"state",
                             "level",
                             "questionText",
                             "questionParts",
                             "tiles",
                             "removedTiles",
                             "secondsLeft",
                             "alert",
                             "points",
                             "wallet",
                             "run",
                             "multiplier",
                             "hintAvailable",
                             "top10",
                             "bestScore",
                             "hasSavedRun",
                             "gateActive",
                             "gateWords",
                             "versionText",
                             "noticesText",
                             "levelClear",
                             "savedRun",
                             "gateUrl",
                             "gateDigits",
                             "privacyPolicyUrl",
                             "repositoryUrl",
                             "firstLevelQuestions",
                             "firstLevelSeconds",
                             "hapticsEnabled",
                             "hapticsAvailable",
                             "players",
                             "playerName",
                             "playerId",
                             "maxPlayers",
                             "canChangePlayers",
                             "canAddPlayer",
                             "canRemovePlayer",
                             "gatePlayerName"})
    {
        INFO(name);
        CHECK(meta.indexOfProperty(name) >= 0);
    }
    for (const char* name : {"startGame()",
                             "continueGame()",
                             "answer(int)",
                             "useHint()",
                             "pause()",
                             "resume()",
                             "quit()",
                             "nextLevel()",
                             "keepGoing()",
                             "finish()",
                             "requestLink(QString)",
                             "submitGate(QString)",
                             "saveName(QString)",
                             "skipName()",
                             "gateMatches(QString)",
                             "cancelGate()",
                             "setHapticsEnabled(bool)",
                             "tap()",
                             "selectPlayer(QString)",
                             "addPlayer()",
                             "renamePlayer(QString,QString)",
                             "requestRemovePlayer(QString)"})
    {
        INFO(name);
        CHECK(meta.indexOfMethod(name) >= 0);
    }
}

TEST_CASE("The saved run says its level and the time left for the title screen", "[integration][controller]")
{
    MemorySaveStore memory;
    int level = 0;
    int seconds = 0;
    {
        Rig first(memory);
        CHECK(first.controller.saved_run().isEmpty());
        first.controller.startGame();
        first.play_level_right();
        first.controller.nextLevel();
        first.answer_right();
        first.play_for_ms(3000);
        first.controller.pause();
        level = first.controller.level();
        seconds = first.controller.seconds_left();
        const QVariantMap now = first.controller.saved_run();
        CHECK(now.value(QStringLiteral("level")).toInt() == level);
        CHECK(now.value(QStringLiteral("secondsLeft")).toInt() == seconds);
    }
    CHECK(level == 2);

    Rig second(memory);
    QSignalSpy spy(&second.controller, &GameController::savedRunChanged);
    const QVariantMap saved_run = second.controller.saved_run();
    CHECK(saved_run.value(QStringLiteral("level")).toInt() == level);
    CHECK(saved_run.value(QStringLiteral("secondsLeft")).toInt() == seconds);

    // Reading it changes nothing in the controller's own game.
    CHECK(second.controller.state() == State::Idle);
    CHECK(second.controller.has_saved_run());

    // A new game spends the run, and the caption goes with it.
    second.controller.startGame();
    CHECK(second.controller.saved_run().isEmpty());
    CHECK(spy.count() >= 1);
}

TEST_CASE("A saved run the core would refuse shows no caption", "[integration][controller]")
{
    SaveData data;
    give_run(data, {1, 2, 3});
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    CHECK(rig.controller.has_saved_run());
    CHECK(rig.controller.saved_run().isEmpty());
}

TEST_CASE("The gate says what it opens and checks the typed number without telling it", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK_FALSE(c.gateMatches(QStringLiteral("123"))); // no gate open
    CHECK(c.gate_url().isEmpty());
    CHECK(c.gate_digits() == 3);

    REQUIRE(c.requestLink(c.privacy_policy_url()));
    CHECK(c.gate_url() == c.privacy_policy_url());
    const GateChallenge first = make_gate_challenge(FIRST_GATE_SEED);
    const QString right = QString::number(first.number);
    CHECK(c.gateMatches(right));
    CHECK(c.gateMatches(QStringLiteral(" ") + right + QStringLiteral(" ")));
    CHECK_FALSE(c.gateMatches(QString::number(first.number + 1)));
    CHECK_FALSE(c.gateMatches(right.left(2)));
    CHECK_FALSE(c.gateMatches(QString()));
    CHECK(rig.links.opened.empty()); // asking never opens

    c.cancelGate();
    CHECK(c.gate_url().isEmpty());
    CHECK_FALSE(c.gateMatches(right));
}

TEST_CASE("The two addresses About opens are https and written once", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK(c.privacy_policy_url() ==
          QStringLiteral("https://iainchesworthlabs.github.io/heart-boom-maths/privacy-policy/"));
    CHECK(c.repository_url() == QStringLiteral("https://github.com/iainchesworthlabs/heart-boom-maths"));
    // Each is accepted by requestLink, which needs https.
    CHECK(c.requestLink(c.privacy_policy_url()));
    c.cancelGate();
    CHECK(c.requestLink(c.repository_url()));
    c.cancelGate();
    CHECK(rig.links.opened.empty());
}

TEST_CASE("The title tagline numbers come from the tuning", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    CHECK(rig.controller.first_level_questions() == Tuning::defaults().questions_per_level);
    CHECK(rig.controller.first_level_seconds() == Tuning::defaults().level_seconds);
}

TEST_CASE("The level-clear card adds up the answers of the level and reads the next level", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_level_right();
    REQUIRE(c.state() == State::LevelClear);

    // Ten right answers at x1, x1, x2 (from the third) and x3 (from the sixth): 10 + 10 + 20 * 3 + 30 * 5.
    const QVariantMap card = c.level_clear();
    CHECK(card.value("answerPoints").toInt() == 10 + 10 + 20 * 3 + 30 * 5);
    CHECK(card.value("timeBonusPerSecond").toInt() == 10);
    CHECK(card.value("answerCoins").toInt() == 10 + 2); // a coin each and the streak bonus at the fifth and tenth
    CHECK(card.value("streakCoins").toInt() == 2);
    CHECK(card.value("levelPoints").toInt() == card.value("answerPoints").toInt() + 600 + 100);
    CHECK(card.value("levelCoins").toInt() == 10 + 2 + 5);
    CHECK(card.value("levelPoints").toLongLong() == card.value("totalPoints").toLongLong());
    CHECK(card.value("wallet").toLongLong() == c.wallet());
    CHECK(card.value("complete").toBool());
    CHECK(card.value("nextUp").toString() == QStringLiteral("Next up: level 2, take away, up to 5"));

    // The next level starts its own count.
    c.nextLevel();
    rig.answer_right();
    rig.answer_wrong();
    for (int i = 0; i < 8; ++i)
    {
        rig.answer_right();
    }
    REQUIRE(c.state() == State::LevelClear);
    const QVariantMap second = c.level_clear();
    CHECK(second.value("level").toInt() == 2);
    CHECK_FALSE(second.value("perfect").toBool());
    CHECK(second.value("answerCoins").toInt() < 10 + 2);
    CHECK(second.value("levelCoins").toInt() == second.value("answerCoins").toInt());
    CHECK(second.value("levelPoints").toLongLong() + card.value("levelPoints").toLongLong() ==
          second.value("totalPoints").toLongLong());
}

TEST_CASE("A level brought back by CONTINUE part way leaves its sums out of the card", "[integration][controller]")
{
    MemorySaveStore memory;
    {
        Rig first(memory);
        first.controller.startGame();
        first.answer_right();
        first.answer_right();
        first.controller.pause();
    }
    Rig second(memory);
    GameController& c = second.controller;
    REQUIRE(c.continueGame());
    c.resume();
    for (int i = 0; i < 8; ++i)
    {
        second.answer_right();
    }
    REQUIRE(c.state() == State::LevelClear);
    const QVariantMap card = c.level_clear();
    CHECK_FALSE(card.value("complete").toBool());
    CHECK(card.value("levelPoints").toInt() == 0);
    CHECK(card.value("levelCoins").toInt() == 0);
    CHECK(card.value("totalPoints").toLongLong() == c.points()); // the numbers the core gave stay right
    CHECK(card.value("timeBonusPoints").toInt() > 0);

    // The next level counts again from its start.
    c.nextLevel();
    for (int i = 0; i < 10; ++i)
    {
        second.answer_right();
    }
    CHECK(c.level_clear().value("complete").toBool());
    CHECK(c.level_clear().value("levelPoints").toInt() > 0);
}

TEST_CASE("The win says what it paid, and nothing when the bonus was paid before", "[integration][controller]")
{
    const auto win = [](Rig& rig)
    {
        rig.controller.startGame();
        for (int level = 1; level <= 12; ++level)
        {
            rig.play_level_right();
            if (level < 12)
            {
                rig.controller.nextLevel();
            }
        }
    };
    MemorySaveStore memory;
    {
        Rig first(memory);
        QSignalSpy bonus_spy(&first.controller, &GameController::winBonusChanged);
        CHECK(first.controller.campaign_levels() == 12);
        win(first);
        REQUIRE(first.controller.state() == State::Won);
        CHECK(first.controller.win_bonus() == 1000);
        CHECK(bonus_spy.count() == 1);
        CHECK(first.controller.coins_earned() >= 1000);
        CHECK(first.controller.coins_earned_known());
    }
    Rig second(memory); // the save says the bonus was paid
    win(second);
    REQUIRE(second.controller.state() == State::Won);
    CHECK(second.controller.win_bonus() == 0);
    CHECK(second.controller.coins_earned() < 1000);
}

TEST_CASE("Coins earned counts what the game paid, not what a hint cost", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    CHECK(c.coins_earned() == 0);
    rig.answer_right();
    rig.answer_right();
    c.useHint();
    rig.answer_right();
    CHECK(c.wallet() == 1);
    c.pause();
    c.quit();
    CHECK(c.coins_earned() == 3);
    CHECK(c.coins_earned_known());

    // A new game starts the count again; a resumed game does not know its first part.
    c.startGame();
    CHECK(c.coins_earned() == 0);
    c.pause();
    {
        Rig again(memory);
        REQUIRE(again.controller.continueGame());
        CHECK_FALSE(again.controller.coins_earned_known());
        again.controller.quit();
        CHECK_FALSE(again.controller.coins_earned_known());
    }
}

TEST_CASE("SAVE needs a letter and SKIP drops the entry but keeps the player's best", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK(c.top10_size() == 10);
    CHECK(c.canSaveName(QStringLiteral("a")));
    CHECK(c.canSaveName(QStringLiteral(" a ")));
    CHECK(c.canSaveName(QStringLiteral("sparky 1!")));
    CHECK_FALSE(c.canSaveName(QString()));
    CHECK_FALSE(c.canSaveName(QStringLiteral("   ")));
    CHECK_FALSE(c.canSaveName(QStringLiteral("123 !?"))); // nothing but spaces is left of it

    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    REQUIRE(c.name_entry_pending());
    CHECK(c.new_best());
    QSignalSpy entry_spy(&c, &GameController::nameEntryChanged);
    c.skipName();
    // The entry is not in the list, but every finished game counts towards the player's best, so the score is still a
    // new best (SPEC, Players).
    CHECK(c.new_best());
    CHECK(entry_spy.count() == 1);
    CHECK(c.best_score() == 10);
    CHECK(c.top10().isEmpty());
}

// Journeys and lifecycle: what the controller does when the app is interrupted or the save is odd

namespace
{

/// A save whose run is the snapshot of a game that was brought to some state by `play`, as a hand-made file would hold.
std::string save_with_snapshot_after(const std::function<void(GameSession&)>& play)
{
    GameSession session(Tuning::defaults(), 20261114);
    session.start_game();
    play(session);
    SaveData data;
    give_wallet(data, 4);
    give_run(data, session.snapshot());
    return *serialise(data);
}

void answer_all_right(GameSession& session)
{
    for (int i = 0; i < 10; ++i)
    {
        session.answer(session.view().question.correct_tile);
    }
}

} // namespace

TEST_CASE("A saved run that is not a paused level is refused and spent", "[integration][controller][journey]")
{
    SECTION("a level-clear card")
    {
        MemorySaveStore memory(save_with_snapshot_after(answer_all_right));
        Rig rig(memory);
        CHECK(rig.controller.has_saved_run());
        CHECK(rig.controller.saved_run().isEmpty()); // no caption for a run CONTINUE would not bring back
        int failed = 0;
        QObject::connect(&rig.controller, &GameController::continueFailed, [&] { ++failed; });
        CHECK_FALSE(rig.controller.continueGame());
        CHECK(failed == 1);
        CHECK(rig.controller.state() == State::Idle);
        CHECK(rig.controller.level() == 0);
        CHECK_FALSE(saved(memory).device.run.has_value());
        CHECK(wallet_of(saved(memory)) == 4);
    }
    SECTION("a finished game")
    {
        MemorySaveStore memory(save_with_snapshot_after(
            [](GameSession& session)
            {
                session.pause();
                session.quit();
            }));
        Rig rig(memory);
        CHECK_FALSE(rig.controller.continueGame());
        CHECK(rig.controller.state() == State::Idle);
        CHECK_FALSE(rig.controller.has_saved_run());
    }
    SECTION("a paused level is still resumed")
    {
        MemorySaveStore memory(save_with_snapshot_after([](GameSession& session) { session.pause(); }));
        Rig rig(memory);
        CHECK(rig.controller.saved_run().value(QStringLiteral("level")).toInt() == 1);
        CHECK(rig.controller.continueGame());
        CHECK(rig.controller.state() == State::Paused);
    }
}

TEST_CASE("The parental gate is closed when the app leaves the foreground", "[integration][controller][journey]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;

    REQUIRE(c.requestLink(QStringLiteral("https://example.org/privacy")));
    REQUIRE(c.gate_active());
    c.on_application_state(false);
    CHECK_FALSE(c.gate_active());
    CHECK(c.gate_words().isEmpty());
    c.on_application_state(true);
    CHECK_FALSE(c.gate_active()); // it does not come back
    CHECK(rig.links.opened.empty());
    CHECK_FALSE(c.submitGate(QStringLiteral("123"))); // and no number opens anything

    // The same for the Game Center switch.
    MemorySaveStore apple_memory;
    Rig apple(apple_memory, true);
    apple.controller.requestGameCenter(true);
    REQUIRE(apple.controller.gate_active());
    apple.controller.on_application_state(false);
    CHECK_FALSE(apple.controller.gate_active());
    CHECK_FALSE(apple.controller.game_center_enabled());
}

TEST_CASE("Leaving the foreground in every state keeps what the save promises", "[integration][controller][journey]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;

    // Title: nothing to pause.
    c.on_application_state(false);
    c.on_application_state(true);
    CHECK(c.state() == State::Idle);

    // Playing: pauses, and the run is on disk.
    c.startGame();
    rig.answer_right();
    c.on_application_state(false);
    REQUIRE(c.state() == State::Paused);
    CHECK(saved(memory).device.run.has_value());
    // Paused: leaving again keeps the run.
    c.on_application_state(true);
    c.on_application_state(false);
    CHECK(saved(memory).device.run.has_value());
    c.on_application_state(true);
    c.resume();
    CHECK_FALSE(saved(memory).device.run.has_value());

    // Level clear: the wallet is on disk, there is no run to bring back.
    for (int i = 0; i < 9; ++i)
    {
        rig.answer_right();
    }
    REQUIRE(c.state() == State::LevelClear);
    const std::int64_t wallet = c.wallet();
    c.on_application_state(false);
    CHECK(c.state() == State::LevelClear);
    CHECK(wallet_of(saved(memory)) == wallet);
    CHECK_FALSE(saved(memory).device.run.has_value());
    c.on_application_state(true);

    // Bomb, then name entry: the wallet is on disk, the name is still waiting for the child.
    c.nextLevel();
    rig.play_for_ms(61'000);
    REQUIRE(c.state() == State::GameOver);
    REQUIRE(c.name_entry_pending());
    c.on_application_state(false);
    c.on_application_state(true);
    CHECK(c.name_entry_pending());
    CHECK(saved(memory).shared.top10.empty()); // not saved until SAVE: a kill here loses the entry
    CHECK(wallet_of(saved(memory)) == c.wallet());
    CHECK(c.saveName(QStringLiteral("zoe")) == 1);
    CHECK(saved(memory).shared.top10.size() == 1);
}

TEST_CASE("Taps on the tiles are ignored for a moment after a level starts and after RESUME",
          "[integration][controller][guard]")
{
    MemorySaveStore memory;
    Rig rig(memory, false, TILE_GUARD_NS);
    GameController& c = rig.controller;

    c.startGame();
    REQUIRE(c.session_view().state == SessionState::PLAYING);
    // The tap that started the level may arrive twice: the second one lands on a tile and must not count.
    rig.answer_right();
    CHECK(c.session_view().questions_answered == 0);
    CHECK(c.session_view().points == 0);

    rig.play_for_ms(300);
    rig.answer_right();
    CHECK(c.session_view().questions_answered == 1);

    // RESUME sits over the tiles of the play screen, so it arms the guard again. The clock does not run while paused.
    c.pause();
    REQUIRE(c.session_view().state == SessionState::PAUSED);
    c.resume();
    rig.answer_right();
    CHECK(c.session_view().questions_answered == 1);
    rig.play_for_ms(300);
    rig.answer_right();
    CHECK(c.session_view().questions_answered == 2);
}

TEST_CASE("Taps on the tiles are ignored for a moment after NEXT LEVEL and KEEP GOING",
          "[integration][controller][guard]")
{
    MemorySaveStore memory;
    Rig rig(memory, false, TILE_GUARD_NS);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_for_ms(300);
    for (int i = 0; i < Tuning::defaults().questions_per_level; ++i)
    {
        rig.answer_right();
    }
    REQUIRE(c.session_view().state == SessionState::LEVEL_CLEAR);

    c.nextLevel();
    REQUIRE(c.session_view().state == SessionState::PLAYING);
    rig.answer_right();
    CHECK(c.session_view().questions_answered == 0);
    rig.play_for_ms(300);
    rig.answer_right();
    CHECK(c.session_view().questions_answered == 1);
}

TEST_CASE("A guard of zero lets a tap through at once", "[integration][controller][guard]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    rig.controller.startGame();
    rig.answer_right();
    CHECK(rig.controller.session_view().questions_answered == 1);
}

// ---- Haptics ------------------------------------------------------------------------------------------------------

namespace
{

using Haptics = std::vector<HapticId>;

/// A controller over the real haptic rules and a recording device, to see what a phone would be asked to do. The clock
/// is the rules' too, so time moves the timer of the win and of the gap between two haptics.
struct HapticGame
{
    explicit HapticGame(MemorySaveStore& memory) : controller(make_services(memory), make_options()) {}

    GameServices make_services(MemorySaveStore& memory)
    {
        GameServices services;
        services.store = &memory;
        services.audio = &audio;
        services.scores = &scores;
        services.haptics = &device.service;
        services.links = &links;
        services.clock = &device.clock;
        return services;
    }

    static GameOptions make_options()
    {
        GameOptions options;
        options.session_seed = 20261114;
        options.tile_guard_ns = 0;
        return options;
    }

    void answer_right()
    {
        controller.answer(controller.session_view().question.correct_tile);
        device.advance_ms(300); // the next tap is a new moment
    }

    /// Plays levels 1 to 12 right to the win. With `settle_last` false the clock stands still after the last answer, so
    /// the later beats of the win are still to come.
    void play_to_win(bool settle_last)
    {
        controller.startGame();
        for (int level = 1; level <= 12; ++level)
        {
            for (int i = 0; i < 10; ++i)
            {
                controller.answer(controller.session_view().question.correct_tile);
                if (settle_last || level < 12 || i < 9)
                {
                    device.advance_ms(300);
                }
            }
            if (level < 12)
            {
                controller.nextLevel();
            }
        }
    }

    HapticRig device;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    GameController controller;
};

} // namespace

TEST_CASE("Haptics are on by default, reach the service and are available where the device can",
          "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    CHECK(rig.controller.haptics_enabled());
    CHECK(rig.controller.haptics_available());
    CHECK(rig.haptics.enabled);
    CHECK_FALSE(memory.text().has_value());
}

TEST_CASE("Without a haptics service the controller is silent and says haptics are unavailable",
          "[integration][haptics]")
{
    MemorySaveStore memory;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    GameServices services;
    services.store = &memory;
    services.audio = &audio;
    services.scores = &scores;
    services.links = &links;
    services.clock = &clock;
    GameOptions options;
    options.tile_guard_ns = 0;
    GameController c(services, options);
    CHECK_FALSE(c.haptics_available());
    c.startGame();
    c.tap();
    c.answer(c.session_view().question.correct_tile);
    c.pause();
    c.resume();
    c.on_application_state(false);
    c.on_application_state(true);
    c.setHapticsEnabled(false);
    CHECK_FALSE(c.haptics_enabled());
}

TEST_CASE("A device that cannot make haptics reports them unavailable but keeps the preference",
          "[integration][haptics]")
{
    MemorySaveStore memory;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    RecordingHaptics haptics(false);
    GameServices services;
    services.store = &memory;
    services.audio = &audio;
    services.scores = &scores;
    services.haptics = &haptics;
    services.links = &links;
    services.clock = &clock;
    GameController c(services);
    CHECK_FALSE(c.haptics_available());
    CHECK(c.haptics_enabled());
}

TEST_CASE("A save from before haptics turns them on, and one that turned them off keeps them off",
          "[integration][haptics]")
{
    MemorySaveStore older(R"({"version":1,"coins":5,"won_once":false,"sound":true,"music":true,"top10":[]})");
    Rig first(older);
    CHECK(first.controller.haptics_enabled());
    CHECK(first.haptics.enabled);
    CHECK(first.controller.wallet() == 5);

    SaveData data;
    data.device.haptics = false;
    MemorySaveStore off(*serialise(data));
    Rig second(off);
    CHECK_FALSE(second.controller.haptics_enabled());
    CHECK_FALSE(second.haptics.enabled);
}

TEST_CASE("The haptics switch saves at once and survives a restart", "[integration][haptics]")
{
    MemorySaveStore memory;
    {
        Rig rig(memory);
        QSignalSpy spy(&rig.controller, &GameController::settingsChanged);
        rig.controller.setHapticsEnabled(false);
        CHECK_FALSE(rig.controller.haptics_enabled());
        CHECK_FALSE(rig.haptics.enabled);
        CHECK_FALSE(saved(memory).device.haptics);
        CHECK(spy.count() == 1);
        rig.controller.setHapticsEnabled(false); // no change, no signal
        CHECK(spy.count() == 1);
        // The other switches are untouched.
        CHECK(saved(memory).device.sound);
        CHECK(saved(memory).device.music);
    }
    {
        Rig again(memory);
        CHECK_FALSE(again.controller.haptics_enabled());
        CHECK_FALSE(again.haptics.enabled);
        again.controller.setHapticsEnabled(true);
        CHECK(saved(memory).device.haptics);
    }
    Rig third(memory);
    CHECK(third.controller.haptics_enabled());
}

TEST_CASE("Each right answer is felt as the table says, and the last of a level as its clear", "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    rig.answer_right();
    rig.answer_right(); // a run of 3 lifts the multiplier
    CHECK(rig.haptics.played == Haptics{HapticId::RIGHT, HapticId::RIGHT, HapticId::MULTIPLIER_UP});

    for (int i = 0; i < 7; ++i)
    {
        rig.answer_right();
    }
    REQUIRE(c.state() == State::LevelClear);
    // One haptic per answer, whatever else the answer set off, and the tenth is the level clear alone.
    CHECK(rig.haptics.played.size() == 10);
    CHECK(rig.haptics.played.back() == HapticId::LEVEL_CLEAR);
    CHECK(rig.haptics.count(HapticId::LEVEL_CLEAR) == 1);
}

TEST_CASE("A wrong answer is felt as an error", "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    rig.controller.startGame();
    rig.answer_wrong();
    CHECK(rig.haptics.played == Haptics{HapticId::WRONG});
}

TEST_CASE("A hint is felt as a soft haptic", "[integration][haptics]")
{
    SaveData data;
    give_wallet(data, 10);
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    rig.controller.startGame();
    rig.controller.useHint();
    CHECK(rig.haptics.played == Haptics{HapticId::HINT});
}

TEST_CASE("The alert and the bomb are felt once each, and the ticking between them not at all",
          "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_for_ms(51'000);
    REQUIRE(c.alert());
    CHECK(rig.haptics.played == Haptics{HapticId::ALERT});

    // Pausing and coming back inside the alert zone does not warn a second time.
    c.pause();
    c.resume();
    CHECK(rig.haptics.played == Haptics{HapticId::ALERT});

    while (c.state() == State::Playing)
    {
        rig.play_for_ms(50);
    }
    CHECK(c.end_reason() == GameController::EndReason::Bomb);
    CHECK(rig.haptics.played == Haptics{HapticId::ALERT, HapticId::BOOM});
}

TEST_CASE("Quitting from the pause screen is not felt as a bomb", "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    rig.controller.startGame();
    rig.controller.pause();
    rig.controller.quit();
    REQUIRE(rig.controller.state() == State::GameOver);
    CHECK(rig.haptics.asked.empty());
}

TEST_CASE("Clearing level 12 is felt as the win and not as a level clear on top", "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    for (int level = 1; level <= 12; ++level)
    {
        rig.play_level_right();
        if (level < 12)
        {
            c.nextLevel();
        }
    }
    REQUIRE(c.state() == State::Won);
    CHECK(rig.haptics.count(HapticId::LEVEL_CLEAR) == 11);
    CHECK(rig.haptics.count(HapticId::WIN) == 1);
    CHECK(rig.haptics.played.back() == HapticId::WIN);
}

TEST_CASE("A tap is a light haptic, and none is felt on a paused game", "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;

    c.tap();
    CHECK(rig.haptics.played == Haptics{HapticId::TAP});

    c.startGame();
    c.pause();
    CHECK(rig.haptics.suspended);
    CHECK(rig.haptics.suspends == 1);
    c.tap();
    CHECK(rig.haptics.asked.back() == HapticId::TAP);
    CHECK(rig.haptics.played == Haptics{HapticId::TAP}); // only the one before the pause

    c.resume();
    CHECK_FALSE(rig.haptics.suspended);
    c.tap();
    CHECK(rig.haptics.played == Haptics{HapticId::TAP, HapticId::TAP});
}

TEST_CASE("Leaving the foreground silences haptics until the app is back and the game is not paused",
          "[integration][haptics]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;

    // On the title screen: quiet in the background, back when the app returns.
    c.on_application_state(false);
    CHECK(rig.haptics.suspended);
    c.tap();
    CHECK(rig.haptics.played.empty());
    c.on_application_state(true);
    CHECK_FALSE(rig.haptics.suspended);

    // In a game: the pause the app made on leaving keeps it quiet until RESUME.
    c.startGame();
    c.on_application_state(false);
    REQUIRE(c.state() == State::Paused);
    CHECK(rig.haptics.suspended);
    c.on_application_state(true);
    CHECK(rig.haptics.suspended);
    c.resume();
    CHECK_FALSE(rig.haptics.suspended);
    rig.answer_right();
    CHECK(rig.haptics.played == Haptics{HapticId::RIGHT});
}

TEST_CASE("With the switch off nothing reaches the device, and turning it on brings it back", "[integration][haptics]")
{
    MemorySaveStore memory;
    HapticGame game(memory);
    GameController& c = game.controller;
    CHECK(c.haptics_available());

    c.setHapticsEnabled(false);
    c.tap();
    c.startGame();
    game.answer_right();
    const int wrong_tile = c.session_view().question.correct_tile == 0 ? 1 : 0;
    c.answer(wrong_tile);
    game.device.advance_ms(300);
    c.useHint();
    c.pause();
    c.resume();
    game.device.advance_ms(1000);
    CHECK(game.device.backend.fired.empty());

    c.setHapticsEnabled(true);
    c.tap();
    CHECK(game.device.backend.ids() == Haptics{HapticId::TAP});
}

TEST_CASE("A right and a wrong answer reach the device as RIGHT and WRONG", "[integration][haptics]")
{
    MemorySaveStore memory;
    HapticGame game(memory);
    GameController& c = game.controller;
    c.startGame();
    game.answer_right();
    c.answer(c.session_view().question.correct_tile == 0 ? 1 : 0);
    game.device.advance_ms(300);
    CHECK(game.device.backend.ids() == Haptics{HapticId::RIGHT, HapticId::WRONG});
}

TEST_CASE("The win reaches the device as its three beats", "[integration][haptics]")
{
    MemorySaveStore memory;
    HapticGame game(memory);
    game.play_to_win(true);
    REQUIRE(game.controller.state() == State::Won);
    const Haptics fired = game.device.backend.ids();
    REQUIRE(fired.size() >= 3);
    CHECK(fired[fired.size() - 3] == HapticId::RIGHT);
    CHECK(fired[fired.size() - 2] == HapticId::MULTIPLIER_UP);
    CHECK(fired[fired.size() - 1] == HapticId::LEVEL_CLEAR);
}

TEST_CASE("Leaving the app in the middle of the win's beats cuts them short", "[integration][haptics]")
{
    MemorySaveStore memory;
    HapticGame game(memory);
    game.play_to_win(false);
    REQUIRE(game.controller.state() == State::Won);
    const std::size_t before = game.device.backend.fired.size();
    game.controller.on_application_state(false);
    game.device.advance_ms(1000);
    CHECK(game.device.backend.fired.size() == before);
}

// ---- Players and the device id (save version 2)
// ----------------------------------------------------------------------

TEST_CASE("A new device is given an id from the injected source, and keeps it", "[integration][controller][save]")
{
    MemorySaveStore memory;
    {
        Rig rig(memory);
        CHECK(rig.id_draws == 1); // the player `first` exists already, so only the device id is drawn
        rig.controller.setSoundEnabled(false);
        const SaveData first_write = saved(memory);
        CHECK(first_write.device.id == FIRST_ID);
        CHECK(first_write.device.current_player == "first");
        REQUIRE(first_write.shared.players.size() == 1);
        CHECK(first_write.shared.players[0].id == "first");
        CHECK(first_write.shared.players[0].name.empty());
    }
    // A restart reads the id back and draws nothing.
    Rig again(memory);
    CHECK(again.id_draws == 0);
    again.controller.setSoundEnabled(true);
    CHECK(saved(memory).device.id == FIRST_ID);
}

TEST_CASE("The wallet is the current player's, kept as what this device earned and spent",
          "[integration][controller][save]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    for (int i = 0; i < 3; ++i)
    {
        rig.answer_right();
    }
    c.pause();
    REQUIRE(c.wallet() == 3);
    const SaveData on_disk = saved(memory);
    REQUIRE(on_disk.shared.players.size() == 1);
    CHECK(on_disk.shared.players[0].coins == std::vector<CoinEntry>{{std::string(FIRST_ID), 3, 0}});
    CHECK(wallet_of(on_disk) == 3);
}

TEST_CASE("A hint is spent as `spent`, and earned coins are never taken back", "[integration][controller][save]")
{
    SaveData data;
    give_wallet(data, 10);
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    c.useHint();
    c.pause();
    const SaveData on_disk = saved(memory);
    // The 10 coins from the old save were given this device's id, then the hint cost 2.
    CHECK(on_disk.shared.players[0].coins == std::vector<CoinEntry>{{std::string(FIRST_ID), 10, 2}});
    CHECK(wallet_of(on_disk) == 8);
    CHECK(c.wallet() == 8);
}

TEST_CASE("A version 1 save becomes one player on this device", "[integration][controller][save]")
{
    MemorySaveStore memory(R"({"version":1,"coins":137,"won_once":true,"sound":true,"music":false,"top10":)"
                           R"([{"name":"ADA","score":900},{"name":"BEN","score":500}],"run":[1,2,3]})");
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK(c.wallet() == 137);
    CHECK(c.session_view().won_once);
    CHECK(c.best_score() == 900);
    CHECK(c.top10().size() == 2);
    CHECK(c.has_saved_run());
    CHECK_FALSE(c.music_enabled());

    c.setSoundEnabled(false); // any save writes version 2
    const SaveData on_disk = saved(memory);
    CHECK(on_disk.version == 2);
    CHECK(on_disk.device.id == FIRST_ID);
    CHECK(on_disk.device.run_player == "first");
    CHECK(on_disk.shared.players[0].coins == std::vector<CoinEntry>{{std::string(FIRST_ID), 137, 0}});
    CHECK(on_disk.shared.players[0].best == 900);
    CHECK(on_disk.shared.top10 ==
          std::vector<ScoreEntry>{{"ADA", 900, std::string(FIRST_ID), 1}, {"BEN", 500, std::string(FIRST_ID), 2}});
    CHECK(on_disk.device.next_seq == 3);
}

TEST_CASE("A saved run is kept with the player who paused it", "[integration][controller][save]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    SaveData on_disk = saved(memory);
    REQUIRE(on_disk.device.run.has_value());
    CHECK(on_disk.device.run_player == "first");

    c.resume(); // the run is spent when it resumes
    on_disk = saved(memory);
    CHECK_FALSE(on_disk.device.run.has_value());
    CHECK(on_disk.device.run_player.empty());
}

TEST_CASE("A save with no active player gets an unnamed one, as the current player", "[integration][controller][save]")
{
    SaveData data;
    data.shared.players.clear();
    give_wallet(data, 0);
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    CHECK(rig.id_draws == 2); // the device id, then the id of the new player
    rig.controller.setSoundEnabled(false);
    const SaveData on_disk = saved(memory);
    REQUIRE(on_disk.shared.players.size() == 1);
    // The creation counter is 1, the random part the low 48 bits of the draw.
    CHECK(on_disk.shared.players[0] == Player{.id = "00010000000000ff"});
    CHECK(on_disk.device.current_player == "00010000000000ff");
}

TEST_CASE("The best score on the title is the current player's", "[integration][controller][save]")
{
    SaveData data;
    data.shared.players[0].best = 700;
    data.shared.top10 = {{"ADA", 900, "", 1}};
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    CHECK(rig.controller.best_score() == 700);
    CHECK(rig.controller.top10().size() == 1);
}

// ---- WHO'S PLAYING: players on the title, the saved run, the top 10 and the best score of each player
// ----------------------------------------------------------------------------------------------------

namespace
{

constexpr std::string_view PLAYER_B = "00000000000000b2";
constexpr std::string_view PLAYER_C = "00000000000000c3";
constexpr std::string_view PLAYER_D = "00000000000000d4";

/// A family of two: `first` has no name, 40 coins, a best of 700 and the win bonus; BEN has 7 coins and a best of 300.
SaveData family_save()
{
    SaveData data;
    REQUIRE(add_player(data.shared, std::string(PLAYER_B)).has_value());
    REQUIRE(rename_player(data.shared, PLAYER_B, "BEN").has_value());
    Player& first = data.shared.players[0];
    Player& ben = data.shared.players[1];
    REQUIRE(record_wallet(first, "", 40).has_value());
    REQUIRE(record_wallet(ben, "", 7).has_value());
    first.best = 700;
    first.won_once = true;
    ben.best = 300;
    return data;
}

/// The challenge the gate asked for last. The rig deals the first number from one seed and every later one from
/// another.
GateChallenge latest_gate(const Rig& rig)
{
    return make_gate_challenge(rig.gate_seeds_used <= 1 ? FIRST_GATE_SEED : SECOND_GATE_SEED);
}

/// Removes a player the way a grown-up does: the gate, and the right number. False when the gate would not open.
bool remove_through_gate(Rig& rig, const QString& id)
{
    if (!rig.controller.requestRemovePlayer(id))
    {
        return false;
    }
    return rig.controller.submitGate(QString::number(latest_gate(rig).number));
}

/// A game of `rights` right answers that the player ends with QUIT. Leaves the controller in the game-over state.
void play_and_quit(Rig& rig, int rights)
{
    rig.controller.startGame();
    for (int i = 0; i < rights; ++i)
    {
        rig.answer_right();
    }
    rig.controller.pause();
    rig.controller.quit();
}

const Player& player_on_disk(const SaveData& data, std::string_view id)
{
    const Player* player = find_player(data.shared, id);
    REQUIRE(player != nullptr);
    return *player;
}

} // namespace

TEST_CASE("The players are listed by place, and an unnamed one is called PLAYER and its place",
          "[integration][controller][players]")
{
    SaveData data = family_save();
    REQUIRE(add_player(data.shared, std::string(PLAYER_C)).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_D)).has_value());
    REQUIRE(remove_player(data.shared, PLAYER_C).has_value()); // gone: it takes no place
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    const GameController& c = rig.controller;

    const QVariantList list = c.players();
    REQUIRE(list.size() == 3);
    const QVariantMap first = list[0].toMap();
    CHECK(first.value("id").toString() == QStringLiteral("first"));
    CHECK(first.value("name").toString().isEmpty());
    CHECK(first.value("displayName").toString() == QStringLiteral("PLAYER 1"));
    CHECK_FALSE(first.value("named").toBool());
    CHECK(first.value("best").toLongLong() == 700);
    CHECK(first.value("coins").toLongLong() == 40);
    CHECK(first.value("current").toBool());
    const QVariantMap ben = list[1].toMap();
    CHECK(ben.value("displayName").toString() == QStringLiteral("BEN"));
    CHECK(ben.value("named").toBool());
    CHECK(ben.value("coins").toLongLong() == 7);
    CHECK_FALSE(ben.value("current").toBool());
    CHECK(list[2].toMap().value("displayName").toString() == QStringLiteral("PLAYER 3")); // the third active player

    CHECK(c.player_name() == QStringLiteral("PLAYER 1"));
    CHECK(c.player_id() == QStringLiteral("first"));
    CHECK(c.max_players() == Tuning::defaults().max_players);
    CHECK(c.can_change_players());
    CHECK(c.can_add_player()); // three of four
    CHECK(c.can_remove_player());
}

TEST_CASE("Switching players records the old wallet and gives the session the new player's",
          "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.wallet() == 40);
    REQUIRE(c.session_view().won_once);
    REQUIRE(c.best_score() == 700);
    QSignalSpy wallet_spy(&c, &GameController::walletChanged);
    QSignalSpy best_spy(&c, &GameController::topScoresChanged);
    QSignalSpy players_spy(&c, &GameController::playersChanged);

    REQUIRE(c.selectPlayer(QString::fromLatin1(PLAYER_B)));
    CHECK(c.player_id() == QString::fromLatin1(PLAYER_B));
    CHECK(c.player_name() == QStringLiteral("BEN"));
    CHECK(c.wallet() == 7);
    CHECK_FALSE(c.session_view().won_once); // the win-bonus mark is the player's too
    CHECK(c.best_score() == 300);
    CHECK(wallet_spy.count() == 1);
    CHECK(best_spy.count() >= 1);
    CHECK(players_spy.count() >= 1);
    SaveData on_disk = saved(memory);
    CHECK(on_disk.device.current_player == PLAYER_B);
    CHECK(player_wallet(player_on_disk(on_disk, "first")) == 40);
    CHECK(player_wallet(player_on_disk(on_disk, PLAYER_B)) == 7);
    CHECK(player_on_disk(on_disk, "first").won_once);

    const auto changes = players_spy.count();
    CHECK(c.selectPlayer(QString::fromLatin1(PLAYER_B))); // already playing: true, and nothing happens
    CHECK(players_spy.count() == changes);
    CHECK_FALSE(c.selectPlayer(QStringLiteral("nobody")));
    CHECK_FALSE(c.selectPlayer(QString()));
    CHECK(c.player_id() == QString::fromLatin1(PLAYER_B));

    // BEN plays and earns, and a switch back leaves those coins with BEN.
    play_and_quit(rig, 2);
    CHECK(c.wallet() == 9);
    REQUIRE(c.selectPlayer(QStringLiteral("first")));
    CHECK(c.wallet() == 40);
    CHECK(c.session_view().won_once);
    CHECK(c.best_score() == 700);
    on_disk = saved(memory);
    CHECK(player_wallet(player_on_disk(on_disk, PLAYER_B)) == 9);
    CHECK(player_wallet(player_on_disk(on_disk, "first")) == 40);
    CHECK(on_disk.device.current_player == "first");

    // The game that follows is the first player's: it starts from 40 coins.
    c.startGame();
    rig.answer_right();
    CHECK(c.wallet() == 41);
}

TEST_CASE("Switching, adding and removing are refused while a game is played, paused, on a card or won",
          "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    Rig rig(memory);
    GameController& c = rig.controller;
    const QString ben = QString::fromLatin1(PLAYER_B);
    const auto check_refused = [&](const char* where)
    {
        INFO(where);
        CHECK_FALSE(c.can_change_players());
        CHECK_FALSE(c.selectPlayer(ben));
        CHECK(c.addPlayer().isEmpty());
        CHECK_FALSE(c.requestRemovePlayer(ben));
        CHECK_FALSE(c.gate_active());
        CHECK(c.player_id() == QStringLiteral("first"));
        CHECK(c.players().size() == 2);
    };

    CHECK(c.can_change_players()); // the title
    c.startGame();
    check_refused("playing");
    c.pause();
    check_refused("paused");
    c.resume();
    rig.play_level_right();
    REQUIRE(c.state() == State::LevelClear);
    check_refused("a level-clear card");
    for (int level = 2; level <= 12; ++level)
    {
        c.nextLevel();
        rig.play_level_right();
    }
    REQUIRE(c.state() == State::Won);
    check_refused("won");
    c.finish();
    REQUIRE(c.state() == State::GameOver);
    // The score belongs to the player who played it and is waiting for its name: not yet.
    REQUIRE(c.name_entry_pending());
    check_refused("the name screen");

    CHECK(c.saveName(QStringLiteral("ada")) == 1);
    CHECK(c.can_change_players()); // the game-over card, and the title after HOME
    CHECK(c.selectPlayer(ben));
    CHECK(c.player_id() == ben);
}

TEST_CASE("A new player is unnamed and current, and adding stops at the limit", "[integration][controller][players]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK(c.max_players() == 4);
    QSignalSpy players_spy(&c, &GameController::playersChanged);

    const QString second = c.addPlayer();
    REQUIRE_FALSE(second.isEmpty());
    CHECK(c.player_id() == second);
    CHECK(c.player_name() == QStringLiteral("PLAYER 2"));
    CHECK(c.wallet() == 0);
    CHECK(c.best_score() == 0);
    CHECK_FALSE(c.session_view().won_once);
    CHECK(c.players().size() == 2);
    CHECK(players_spy.count() >= 1);

    const QString third = c.addPlayer();
    const QString fourth = c.addPlayer();
    REQUIRE_FALSE(third.isEmpty());
    REQUIRE_FALSE(fourth.isEmpty());
    CHECK(c.players().size() == 4);
    CHECK_FALSE(c.can_add_player());
    CHECK(c.addPlayer().isEmpty());
    CHECK(c.players().size() == 4);
    CHECK(c.player_id() == fourth); // the refused add changed nothing

    SaveData on_disk = saved(memory);
    REQUIRE(on_disk.shared.players.size() == 4);
    CHECK(on_disk.device.current_player == fourth.toStdString());
    CHECK(second != third);
    CHECK(third != fourth);
    CHECK(second != QStringLiteral("first"));
    for (const Player& player : on_disk.shared.players)
    {
        CHECK(player.name.empty());
        CHECK_FALSE(player.removed);
    }

    // A removed player leaves room again.
    REQUIRE(remove_through_gate(rig, third));
    CHECK(c.players().size() == 3);
    CHECK(c.can_add_player());
    CHECK_FALSE(c.addPlayer().isEmpty());
    CHECK(c.players().size() == 4);
}

TEST_CASE("Whatever the random draw, a new player is last among the active players and nobody else is renumbered",
          "[integration][controller][players]")
{
    // A 64 bit linear congruential step: each trial draws a different run of values, high and low alike.
    for (std::uint64_t trial = 0; trial < 60; ++trial)
    {
        CAPTURE(trial);
        MemorySaveStore memory;
        Rig rig(memory);
        GameController& c = rig.controller;
        std::uint64_t state = trial * 0x9E3779B97F4A7C15ULL + 1;
        rig.id_override = [&state]
        {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            return state;
        };

        std::vector<QString> names_before;
        for (int added = 0; added < 3; ++added)
        {
            const QString id = c.addPlayer();
            REQUIRE_FALSE(id.isEmpty());
            const QVariantList list = c.players();
            REQUIRE(list.size() == added + 2);
            CHECK(list.back().toMap().value("id").toString() == id);
            CHECK(list.back().toMap().value("displayName").toString() == QStringLiteral("PLAYER %1").arg(added + 2));
            for (int i = 0; i < static_cast<int>(names_before.size()); ++i)
            {
                CHECK(list[i].toMap().value("displayName").toString() == names_before[static_cast<std::size_t>(i)]);
            }
            names_before.clear();
            for (const QVariant& entry : list)
            {
                names_before.push_back(entry.toMap().value("displayName").toString());
            }
        }

        // A removed player still counts for the counter, so the next one is again last.
        const QString second = c.players()[1].toMap().value("id").toString();
        REQUIRE(remove_through_gate(rig, second));
        const QString replacement = c.addPlayer();
        REQUIRE_FALSE(replacement.isEmpty());
        CHECK(c.players().back().toMap().value("id").toString() == replacement);
        CHECK(replacement != second);
    }
}

TEST_CASE("A player is named and renamed on the name screen's rules, in any state",
          "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    Rig rig(memory);
    GameController& c = rig.controller;

    CHECK(c.renamePlayer(QStringLiteral("first"), QStringLiteral("  ada lovelace 7! ")));
    CHECK(c.player_name() == QStringLiteral("ADA LOVELA")); // twelve characters at most, counted before the spaces go
    SaveData on_disk = saved(memory);
    CHECK(player_on_disk(on_disk, "first").name == "ADA LOVELA");
    CHECK(player_on_disk(on_disk, "first").name_rev == 1);

    CHECK(c.renamePlayer(QString::fromLatin1(PLAYER_B), QStringLiteral("ben the great")));
    CHECK(c.players()[1].toMap().value("displayName").toString() == QStringLiteral("BEN THE GREA"));

    // The same name again is nothing to change, and nothing to count.
    CHECK(c.renamePlayer(QStringLiteral("first"), QStringLiteral("Ada Lovela")));
    CHECK(player_on_disk(saved(memory), "first").name_rev == 1);

    // No letter, an unknown player, or a player who is gone: refused, and the names are as they were.
    CHECK_FALSE(c.renamePlayer(QStringLiteral("first"), QString()));
    CHECK_FALSE(c.renamePlayer(QStringLiteral("first"), QStringLiteral("   ")));
    CHECK_FALSE(c.renamePlayer(QStringLiteral("first"), QStringLiteral("123 !?")));
    CHECK_FALSE(c.renamePlayer(QStringLiteral("nobody"), QStringLiteral("ZED")));
    CHECK(c.player_name() == QStringLiteral("ADA LOVELA"));

    // Renaming is not a switch: it works while a game is played.
    c.startGame();
    CHECK(c.renamePlayer(QStringLiteral("first"), QStringLiteral("ada")));
    CHECK(c.player_name() == QStringLiteral("ADA"));
    CHECK(c.state() == State::Playing);
}

TEST_CASE("Removing a player needs the gate, and is refused for the last one", "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    Rig rig(memory);
    GameController& c = rig.controller;
    const QString ben = QString::fromLatin1(PLAYER_B);
    QSignalSpy passed_spy(&c, &GameController::gatePassed);

    CHECK_FALSE(c.requestRemovePlayer(QStringLiteral("nobody")));
    CHECK_FALSE(c.requestRemovePlayer(QString()));
    CHECK_FALSE(c.gate_active());

    REQUIRE(c.requestRemovePlayer(ben));
    CHECK(c.gate_active());
    CHECK(c.gate_purpose() == GameController::GatePurpose::RemovePlayer);
    CHECK(c.gate_player_name() == QStringLiteral("BEN"));
    CHECK(c.players().size() == 2);

    // A wrong number removes no one and deals a new one; CANCEL ends it and removes no one.
    CHECK_FALSE(c.submitGate(QString::number(latest_gate(rig).number + 1)));
    CHECK(c.gate_active());
    CHECK(c.gate_player_name() == QStringLiteral("BEN"));
    CHECK(c.players().size() == 2);
    c.cancelGate();
    CHECK_FALSE(c.gate_active());
    CHECK(c.gate_player_name().isEmpty());
    CHECK(c.players().size() == 2);
    CHECK(passed_spy.count() == 0);

    // Another gate is open: no second one over it.
    REQUIRE(c.requestLink(QStringLiteral("https://example.org")));
    CHECK_FALSE(c.requestRemovePlayer(ben));
    c.cancelGate();

    // The app leaving the foreground closes the gate with nobody removed.
    REQUIRE(c.requestRemovePlayer(ben));
    c.on_application_state(false);
    CHECK_FALSE(c.gate_active());
    CHECK(c.players().size() == 2);
    c.on_application_state(true);

    REQUIRE(c.requestRemovePlayer(ben));
    CHECK(c.submitGate(QString::number(latest_gate(rig).number)));
    CHECK_FALSE(c.gate_active());
    REQUIRE(passed_spy.count() == 1);
    CHECK(passed_spy.at(0).at(0).value<GameController::GatePurpose>() == GameController::GatePurpose::RemovePlayer);
    CHECK(c.players().size() == 1);
    CHECK(c.player_id() == QStringLiteral("first")); // BEN was not the player
    const SaveData on_disk = saved(memory);
    CHECK(player_on_disk(on_disk, PLAYER_B).removed);
    CHECK_FALSE(player_on_disk(on_disk, "first").removed);

    // One player is left, and the last one cannot go.
    CHECK_FALSE(c.can_remove_player());
    CHECK_FALSE(c.requestRemovePlayer(QStringLiteral("first")));
    CHECK_FALSE(c.gate_active());
}

TEST_CASE("Removing the player who is playing makes the first one left current, with their wallet",
          "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.selectPlayer(QString::fromLatin1(PLAYER_B)));
    REQUIRE(c.wallet() == 7);

    REQUIRE(remove_through_gate(rig, QString::fromLatin1(PLAYER_B)));
    CHECK(c.player_id() == QStringLiteral("first"));
    CHECK(c.wallet() == 40);
    CHECK(c.session_view().won_once);
    CHECK(c.best_score() == 700);
    const SaveData on_disk = saved(memory);
    CHECK(on_disk.device.current_player == "first");
    CHECK(player_wallet(player_on_disk(on_disk, "first")) == 40); // BEN's 7 did not land on the first player
    CHECK(player_on_disk(on_disk, PLAYER_B).removed);
}

TEST_CASE("The saved run stays with the player who paused it", "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    {
        Rig paused(memory);
        paused.controller.startGame();
        paused.answer_right();
        paused.controller.pause();
    }
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.has_saved_run());
    CHECK(c.saved_run().value("level").toInt() == 1);
    QSignalSpy run_spy(&c, &GameController::savedRunChanged);

    // BEN is playing now: no CONTINUE and no caption, though the run is still on the device.
    REQUIRE(c.selectPlayer(QString::fromLatin1(PLAYER_B)));
    CHECK_FALSE(c.has_saved_run());
    CHECK(c.saved_run().isEmpty());
    CHECK(run_spy.count() >= 1);
    CHECK_FALSE(c.continueGame());
    CHECK(c.state() == State::Idle);
    SaveData on_disk = saved(memory);
    REQUIRE(on_disk.device.run.has_value());
    CHECK(on_disk.device.run_player == "first");

    // BEN's new game, to the bomb, leaves it alone.
    c.startGame();
    rig.answer_right();
    rig.play_for_ms(61'000);
    REQUIRE(c.state() == State::GameOver);
    on_disk = saved(memory);
    REQUIRE(on_disk.device.run.has_value());
    CHECK(on_disk.device.run_player == "first");

    // Back to the first player, and CONTINUE is offered and works, with that player's own wallet.
    REQUIRE(c.selectPlayer(QStringLiteral("first")));
    CHECK(c.has_saved_run());
    CHECK(c.saved_run().value("level").toInt() == 1);
    REQUIRE(c.continueGame());
    CHECK(c.state() == State::Paused);
    CHECK(c.wallet() == 41);                           // 40 and the one right answer before the pause
    CHECK_FALSE(saved(memory).device.run.has_value()); // it resumes once
}

TEST_CASE("A new game as the player who owns the run drops it, and another player's pause replaces it",
          "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    {
        Rig paused(memory);
        paused.controller.startGame();
        paused.controller.pause();
    }

    SECTION("a new game by the owner drops the run")
    {
        Rig rig(memory);
        REQUIRE(rig.controller.has_saved_run());
        rig.controller.startGame();
        CHECK_FALSE(rig.controller.has_saved_run());
        CHECK_FALSE(saved(memory).device.run.has_value());
        CHECK(saved(memory).device.run_player.empty());
    }

    SECTION("pausing as another player replaces it")
    {
        {
            Rig rig(memory);
            REQUIRE(rig.controller.selectPlayer(QString::fromLatin1(PLAYER_B)));
            rig.controller.startGame();
            CHECK(saved(memory).device.run_player == "first"); // still the first player's while BEN plays
            rig.controller.pause();
            CHECK(rig.controller.has_saved_run());
            const SaveData on_disk = saved(memory);
            REQUIRE(on_disk.device.run.has_value());
            CHECK(on_disk.device.run_player == PLAYER_B);
        }
        Rig restarted(memory);
        CHECK(restarted.controller.player_id() == QString::fromLatin1(PLAYER_B)); // the choice is kept on the device
        CHECK(restarted.controller.has_saved_run());
        REQUIRE(restarted.controller.selectPlayer(QStringLiteral("first")));
        CHECK_FALSE(restarted.controller.has_saved_run()); // the first player's run is gone
    }

    SECTION("leaving the foreground as another player replaces it too")
    {
        Rig rig(memory);
        REQUIRE(rig.controller.selectPlayer(QString::fromLatin1(PLAYER_B)));
        rig.controller.startGame();
        rig.controller.on_application_state(false);
        const SaveData on_disk = saved(memory);
        REQUIRE(on_disk.device.run.has_value());
        CHECK(on_disk.device.run_player == PLAYER_B);
    }

    SECTION("QUIT is reached by pausing, so it replaces another player's run and then spends its own")
    {
        Rig rig(memory);
        REQUIRE(rig.controller.selectPlayer(QString::fromLatin1(PLAYER_B)));
        rig.controller.startGame();
        rig.controller.pause();
        CHECK(saved(memory).device.run_player == PLAYER_B);
        rig.controller.quit();
        CHECK_FALSE(saved(memory).device.run.has_value());
        REQUIRE(rig.controller.selectPlayer(QStringLiteral("first")));
        CHECK_FALSE(rig.controller.has_saved_run());
    }

    SECTION("another player's game over leaves the run, and the owner's new game drops it")
    {
        Rig rig(memory);
        REQUIRE(rig.controller.has_saved_run());
        REQUIRE(rig.controller.selectPlayer(QString::fromLatin1(PLAYER_B)));
        // To the bomb: QUIT is reached through the pause screen, and pausing saves a run for BEN, which replaces it.
        rig.controller.startGame();
        rig.answer_right();
        rig.play_for_ms(61'000);
        REQUIRE(rig.controller.state() == State::GameOver);
        CHECK(saved(memory).device.run.has_value());
        CHECK(saved(memory).device.run_player == "first");
        REQUIRE(rig.controller.selectPlayer(QStringLiteral("first")));
        rig.controller.startGame(); // the owner's new game drops it
        CHECK_FALSE(saved(memory).device.run.has_value());
    }
}

TEST_CASE("Removing a player drops that player's saved run and no one else's", "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    {
        Rig paused(memory);
        paused.controller.startGame();
        paused.controller.pause();
    }

    SECTION("the run's player is removed")
    {
        Rig rig(memory);
        REQUIRE(rig.controller.selectPlayer(QString::fromLatin1(PLAYER_B)));
        QSignalSpy run_spy(&rig.controller, &GameController::savedRunChanged);
        REQUIRE(remove_through_gate(rig, QStringLiteral("first")));
        const SaveData on_disk = saved(memory);
        CHECK_FALSE(on_disk.device.run.has_value());
        CHECK(on_disk.device.run_player.empty());
        CHECK(run_spy.count() >= 1);
        CHECK_FALSE(rig.controller.has_saved_run());
    }

    SECTION("another player is removed")
    {
        Rig rig(memory);
        REQUIRE(remove_through_gate(rig, QString::fromLatin1(PLAYER_B)));
        CHECK(rig.controller.has_saved_run());
        const SaveData on_disk = saved(memory);
        REQUIRE(on_disk.device.run.has_value());
        CHECK(on_disk.device.run_player == "first");
    }
}

TEST_CASE("A saved run whose player is gone is not offered", "[integration][controller][players]")
{
    SaveData data = family_save();
    REQUIRE(remove_player(data.shared, PLAYER_B).has_value());
    give_run(data, {1, 2, 3});
    data.device.run_player = std::string(PLAYER_B); // removed on another device and synced here
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    CHECK_FALSE(rig.controller.has_saved_run());
    rig.controller.setSoundEnabled(false); // any save
    const SaveData on_disk = saved(memory);
    CHECK_FALSE(on_disk.device.run.has_value());
    CHECK(on_disk.device.run_player.empty());
}

TEST_CASE("A resumed run takes the player's wallet as it is now, not as it was at the pause",
          "[integration][controller][players]")
{
    MemorySaveStore paused_memory;
    {
        Rig first(paused_memory);
        first.controller.startGame();
        for (int i = 0; i < 3; ++i)
        {
            first.answer_right();
        }
        first.controller.pause();
        REQUIRE(first.controller.wallet() == 3);
    }

    // While the run waited, the player's coins changed (earned on another device and synced, or spent there).
    const auto resumed_with = [&](std::int64_t wallet_now, bool won_once_now)
    {
        SaveData data = saved(paused_memory);
        Player* player = current_player(data);
        REQUIRE(player != nullptr);
        REQUIRE(record_wallet(*player, data.device.id, wallet_now).has_value());
        player->won_once = won_once_now;
        return std::make_unique<MemorySaveStore>(*serialise(data));
    };

    SECTION("more coins")
    {
        const auto memory = resumed_with(50, true);
        Rig rig(*memory);
        GameController& c = rig.controller;
        CHECK(c.wallet() == 50); // the title already shows the player's wallet
        REQUIRE(c.continueGame());
        CHECK(c.state() == State::Paused);
        CHECK(c.wallet() == 50);
        CHECK(c.session_view().wallet == 50);
        CHECK(c.session_view().won_once);
        c.resume();
        c.useHint();
        CHECK(c.wallet() == 48);
        c.pause();
        CHECK(wallet_of(saved(*memory)) == 48);
    }

    SECTION("fewer coins")
    {
        const auto memory = resumed_with(1, false);
        Rig rig(*memory);
        GameController& c = rig.controller;
        REQUIRE(c.continueGame());
        CHECK(c.wallet() == 1);
        c.resume();
        CHECK_FALSE(c.hint_available()); // one coin is short of a hint
        c.pause();
        CHECK(wallet_of(saved(*memory)) == 1);
    }

    SECTION("the same coins, and the rest of the run is as paused")
    {
        Rig rig(paused_memory);
        GameController& c = rig.controller;
        REQUIRE(c.continueGame());
        CHECK(c.wallet() == 3);
        CHECK(c.question_number() == 4);
        CHECK(c.level() == 1);
    }
}

TEST_CASE("A player with a name has a score saved at once, under that name, with no name screen",
          "[integration][controller][players]")
{
    MemorySaveStore memory(*serialise(family_save()));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.selectPlayer(QString::fromLatin1(PLAYER_B)));
    QSignalSpy over_spy(&c, &GameController::gameOver);
    QSignalSpy at_once_spy(&c, &GameController::scoreSavedAtOnce);
    QSignalSpy top_spy(&c, &GameController::topScoresChanged);

    play_and_quit(rig, 1);
    CHECK(c.state() == State::GameOver);
    CHECK_FALSE(c.name_entry_pending());
    CHECK(c.name_entry_rank() == 0);
    CHECK(c.highlight_rank() == 1);
    REQUIRE(c.top10().size() == 1);
    CHECK(c.top10().first().toMap().value("name").toString() == QStringLiteral("BEN"));
    CHECK(c.top10().first().toMap().value("score").toLongLong() == c.final_score());
    REQUIRE(at_once_spy.count() == 1);
    CHECK(at_once_spy.at(0).at(0).toInt() == 1);
    REQUIRE(over_spy.count() == 1);
    CHECK(over_spy.at(0).at(3).toInt() == 1); // the rank it took
    CHECK(top_spy.count() >= 1);

    // The flow after it is the one after SAVE: the best of the player, the card's NEW BEST!, a list on disk.
    CHECK(c.best_score() == 300); // 10 points: not a best for a player whose best is 300
    CHECK_FALSE(c.new_best());
    const SaveData on_disk = saved(memory);
    REQUIRE(on_disk.shared.top10.size() == 1);
    CHECK(on_disk.shared.top10[0].name == "BEN");
    CHECK(on_disk.shared.top10[0].seq == 1);
    CHECK(on_disk.device.next_seq == 2);
    CHECK(c.saveName(QStringLiteral("again")) == 0); // nothing is waiting

    // The next game clears the mark, and the next score takes its place in the list under the same name.
    c.startGame();
    CHECK(c.highlight_rank() == 0);
    rig.answer_right();
    rig.answer_right();
    c.pause();
    c.quit();
    CHECK(c.top10().size() == 2);
    CHECK(c.top10()[0].toMap().value("name").toString() == QStringLiteral("BEN"));
    CHECK(c.top10()[1].toMap().value("name").toString() == QStringLiteral("BEN"));
}

TEST_CASE("A score that does not earn a place gives a named player no name screen and no entry",
          "[integration][controller][players]")
{
    SaveData data = family_save();
    std::vector<ScoreEntry> entries;
    for (int i = 0; i < 10; ++i)
    {
        entries.push_back({"TOP", 1000 - 10 * i});
    }
    give_top10(data, std::move(entries));
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.selectPlayer(QString::fromLatin1(PLAYER_B)));
    QSignalSpy at_once_spy(&c, &GameController::scoreSavedAtOnce);

    play_and_quit(rig, 1);
    CHECK_FALSE(c.name_entry_pending());
    CHECK(c.highlight_rank() == 0);
    CHECK(at_once_spy.count() == 0);
    CHECK(c.top10().size() == 10);
    CHECK(c.top10()[9].toMap().value("score").toLongLong() == 910);
}

TEST_CASE("A player with no name gets the name screen: SAVE names the player, SKIP leaves them unnamed",
          "[integration][controller][players]")
{
    SECTION("SAVE")
    {
        MemorySaveStore memory;
        Rig rig(memory);
        GameController& c = rig.controller;
        QSignalSpy at_once_spy(&c, &GameController::scoreSavedAtOnce);
        play_and_quit(rig, 1);
        REQUIRE(c.name_entry_pending());
        CHECK(c.player_name() == QStringLiteral("PLAYER 1"));
        CHECK(c.saveName(QStringLiteral("  rocket ")) == 1);
        CHECK(at_once_spy.count() == 0); // the name screen's own flow
        CHECK(c.player_name() == QStringLiteral("ROCKET"));
        CHECK(c.top10().first().toMap().value("name").toString() == QStringLiteral("ROCKET"));
        SaveData on_disk = saved(memory);
        CHECK(player_on_disk(on_disk, "first").name == "ROCKET");
        CHECK(player_on_disk(on_disk, "first").name_rev == 1);
        CHECK(on_disk.shared.top10[0].name == "ROCKET");

        // The name is theirs from now on: the next game needs no name screen.
        play_and_quit(rig, 2);
        CHECK_FALSE(c.name_entry_pending());
        CHECK(c.highlight_rank() >= 1);
        CHECK(at_once_spy.count() == 1);
        CHECK(c.top10().size() == 2);
        CHECK(c.top10()[0].toMap().value("name").toString() == QStringLiteral("ROCKET"));
    }

    SECTION("SKIP")
    {
        MemorySaveStore memory;
        Rig rig(memory);
        GameController& c = rig.controller;
        play_and_quit(rig, 1);
        REQUIRE(c.name_entry_pending());
        c.skipName();
        CHECK_FALSE(c.name_entry_pending());
        CHECK(c.top10().isEmpty());
        CHECK(c.player_name() == QStringLiteral("PLAYER 1"));
        CHECK(player_on_disk(saved(memory), "first").name.empty());
        CHECK(player_on_disk(saved(memory), "first").name_rev == 0);

        // Still unnamed, so the next score asks again.
        play_and_quit(rig, 1);
        CHECK(c.name_entry_pending());
    }

    SECTION("a name that has no letter does not save")
    {
        MemorySaveStore memory;
        Rig rig(memory);
        GameController& c = rig.controller;
        play_and_quit(rig, 1);
        REQUIRE(c.name_entry_pending());
        CHECK(c.saveName(QStringLiteral("   ")) == 0);
        CHECK(c.saveName(QStringLiteral("12!")) == 0);
        CHECK(c.name_entry_pending());
        CHECK(player_on_disk(saved(memory), "first").name.empty());
    }
}

TEST_CASE("Every finished game counts towards the player's best, and NEW BEST! is that player's own",
          "[integration][controller][players]")
{
    // The list is full of other players' scores, all higher than anything this player will make; another player's best
    // is higher still. None of that is this player's best.
    SaveData data = family_save();
    std::vector<ScoreEntry> entries;
    for (int i = 0; i < 10; ++i)
    {
        entries.push_back({"BEN", 9000 - 100 * i});
    }
    give_top10(data, std::move(entries));
    data.shared.players[0].best = 0;
    data.shared.players[0].won_once = false;
    data.shared.players[1].best = 99'999;
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.best_score() == 0);

    play_and_quit(rig, 1);
    const std::int64_t first_score = c.final_score();
    REQUIRE(first_score > 0);
    CHECK_FALSE(c.name_entry_pending()); // it does not earn a place in the list...
    CHECK(c.top10().size() == 10);
    CHECK(c.new_best()); // ...but it is this player's best
    CHECK(c.best_score() == first_score);
    CHECK(player_on_disk(saved(memory), "first").best == first_score);

    // The same score again is not a new best; a lower one never is.
    play_and_quit(rig, 1);
    CHECK(c.final_score() == first_score);
    CHECK_FALSE(c.new_best());
    CHECK(c.best_score() == first_score);

    // A higher one is.
    play_and_quit(rig, 4);
    const std::int64_t higher = c.final_score();
    REQUIRE(higher > first_score);
    CHECK(c.new_best());
    CHECK(c.best_score() == higher);

    // A score of 0 is never a new best, and takes nothing away.
    play_and_quit(rig, 0);
    CHECK(c.final_score() == 0);
    CHECK_FALSE(c.new_best());
    CHECK(c.best_score() == higher);

    // The other player's best is theirs.
    REQUIRE(c.selectPlayer(QString::fromLatin1(PLAYER_B)));
    CHECK(c.best_score() == 99'999);
}

TEST_CASE("A score of 0 is never a new best, even for a player with no best at all",
          "[integration][controller][players]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    play_and_quit(rig, 0);
    CHECK(rig.controller.final_score() == 0);
    CHECK_FALSE(rig.controller.new_best());
    CHECK_FALSE(rig.controller.name_entry_pending());
    CHECK(rig.controller.best_score() == 0);
}
