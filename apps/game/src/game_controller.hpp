// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/save_manager.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "heart_boom_maths/session.hpp"
#include "heart_boom_maths/tuning.hpp"
#include "parental_gate.hpp"
#include "services/audio_service.hpp"
#include "services/build_info.hpp"
#include "services/clock.hpp"
#include "services/link_opener.hpp"
#include "services/save_quarantine.hpp"
#include "services/score_service.hpp"
#include "tick_driver.hpp"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace heart_boom_maths::game
{

/// How often the tick driver looks at the clock while a level plays: half a tick at 20 ticks a second. The driver, not
/// this interval, decides how many ticks that is.
inline constexpr int TICK_POLL_INTERVAL_MS = 25;

/// What the controller is handed. It uses them and owns none of them; they must outlive it.
struct GameServices
{
    SaveStore* store = nullptr;
    SaveQuarantine* quarantine = nullptr; // optional: a store that is not a file has nothing to move aside
    AudioService* audio = nullptr;
    ScoreService* scores = nullptr;
    LinkOpener* links = nullptr;
    Clock* clock = nullptr;
};

struct GameOptions
{
    const Tuning* tuning = &Tuning::defaults();
    std::uint64_t session_seed = 1;

    /// Where the parental gate gets the seed of each challenge. Empty means the system random generator. Tests set it
    /// to a fixed list so the number to type back is known.
    std::function<std::uint64_t()> gate_seed_source;
    BuildInfo build_info = BuildInfo::current();

    /// Options with a random session seed, for the real app.
    [[nodiscard]] static GameOptions with_random_seed();
};

/// The one object the screens talk to. It owns the game session, the save data and the tick driver, and it is the only
/// holder of the clock. A screen reads properties and calls the invokable commands; it never works out a rule. The
/// controller draws nothing and loads no QML.
///
/// Every property mirrors session.view() or the save data. Each has its own change signal, emitted only when the value
/// changed. A signal per core event tells a screen what just happened (for the visual twin of a sound).
class GameController final : public QObject
{
    Q_OBJECT

    // The core's session state.
    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)

    // The level on screen.
    Q_PROPERTY(int level READ level NOTIFY levelChanged FINAL)
    Q_PROPERTY(bool endless READ endless NOTIFY levelChanged FINAL)
    Q_PROPERTY(int questionsPerLevel READ questions_per_level NOTIFY levelChanged FINAL)
    Q_PROPERTY(int ticksTotal READ ticks_total NOTIFY levelChanged FINAL)
    Q_PROPERTY(int ticksPerSecond READ ticks_per_second CONSTANT FINAL)
    Q_PROPERTY(int alertTicks READ alert_ticks NOTIFY levelChanged FINAL)
    Q_PROPERTY(int secondsPerRightAnswer READ seconds_per_right_answer NOTIFY levelChanged FINAL)
    Q_PROPERTY(int secondsPerWrongAnswer READ seconds_per_wrong_answer NOTIFY levelChanged FINAL)
    Q_PROPERTY(int correctAnswers READ correct_answers NOTIFY correctAnswersChanged FINAL)

    // The question on screen, as the child reads it.
    Q_PROPERTY(int questionNumber READ question_number NOTIFY questionChanged FINAL)
    Q_PROPERTY(QString questionText READ question_text NOTIFY questionChanged FINAL)
    Q_PROPERTY(QStringList questionParts READ question_parts NOTIFY questionChanged FINAL)
    Q_PROPERTY(QList<int> tiles READ tiles NOTIFY questionChanged FINAL)
    Q_PROPERTY(int removedTiles READ removed_tiles NOTIFY removedTilesChanged FINAL)
    Q_PROPERTY(bool hintUsed READ hint_used NOTIFY hintAvailableChanged FINAL)
    Q_PROPERTY(bool hintAvailable READ hint_available NOTIFY hintAvailableChanged FINAL)
    Q_PROPERTY(int hintCost READ hint_cost CONSTANT FINAL)

    // The question the last answer was given to, as it stood when the child tapped: the core deals the next question at
    // once, so a screen that holds the right or wrong feedback for a moment draws this and not the properties above.
    // Keys: questionNumber, parts, tiles, removedTiles (bit i set means tile i was gone), tile, correctTile, correct.
    // Empty before the first answer. Set before answerCorrect and answerWrong are emitted.
    Q_PROPERTY(QVariantMap answered READ answered NOTIFY answeredChanged FINAL)

    // The timer.
    Q_PROPERTY(int ticksLeft READ ticks_left NOTIFY timeChanged FINAL)
    Q_PROPERTY(int secondsLeft READ seconds_left NOTIFY timeChanged FINAL)
    Q_PROPERTY(bool alert READ alert NOTIFY alertChanged FINAL)

    // The score.
    Q_PROPERTY(qint64 points READ points NOTIFY pointsChanged FINAL)
    Q_PROPERTY(qint64 wallet READ wallet NOTIFY walletChanged FINAL)
    Q_PROPERTY(int run READ run NOTIFY runChanged FINAL)
    Q_PROPERTY(int multiplier READ multiplier NOTIFY multiplierChanged FINAL)

    // The level-clear card: the numbers of the last LEVEL_CLEARED event.
    Q_PROPERTY(QVariantMap levelClear READ level_clear NOTIFY levelClearChanged FINAL)

    // The end of a game and the name entry.
    Q_PROPERTY(EndReason endReason READ end_reason NOTIFY nameEntryChanged FINAL)
    Q_PROPERTY(qint64 finalScore READ final_score NOTIFY nameEntryChanged FINAL)
    Q_PROPERTY(int finalLevel READ final_level NOTIFY nameEntryChanged FINAL)
    Q_PROPERTY(bool newBest READ new_best NOTIFY nameEntryChanged FINAL)
    Q_PROPERTY(bool nameEntryPending READ name_entry_pending NOTIFY nameEntryChanged FINAL)
    Q_PROPERTY(int nameEntryRank READ name_entry_rank NOTIFY nameEntryChanged FINAL)
    Q_PROPERTY(int nameMaxLength READ name_max_length CONSTANT FINAL)

    // The top 10 and the best score. highlightRank is the place of the entry saved last, 0 when none.
    Q_PROPERTY(QVariantList top10 READ top10 NOTIFY topScoresChanged FINAL)
    Q_PROPERTY(qint64 bestScore READ best_score NOTIFY topScoresChanged FINAL)
    Q_PROPERTY(int highlightRank READ highlight_rank NOTIFY topScoresChanged FINAL)

    // The saved run that CONTINUE offers on the title screen.
    Q_PROPERTY(bool hasSavedRun READ has_saved_run NOTIFY savedRunChanged FINAL)

    // Settings.
    Q_PROPERTY(bool soundEnabled READ sound_enabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool musicEnabled READ music_enabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool gameCenterEnabled READ game_center_enabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool gameCenterAvailable READ game_center_available CONSTANT FINAL)

    // The parental gate.
    Q_PROPERTY(bool gateActive READ gate_active NOTIFY gateChanged FINAL)
    Q_PROPERTY(GatePurpose gatePurpose READ gate_purpose NOTIFY gateChanged FINAL)
    Q_PROPERTY(QString gateWords READ gate_words NOTIFY gateChanged FINAL)

    // True when the save on disk came from a newer build: it is left alone, and nothing of this session is saved.
    Q_PROPERTY(bool saveBlocked READ save_blocked CONSTANT FINAL)

    // About and Settings.
    Q_PROPERTY(QString versionText READ version_text CONSTANT FINAL)
    Q_PROPERTY(QVariantMap buildInfo READ build_info CONSTANT FINAL)
    Q_PROPERTY(QString noticesText READ notices CONSTANT FINAL)

public:
    // The enumerators are PascalCase because QML requires a capital letter and these are the names it sees.
    enum class State
    {
        Idle,
        Playing,
        Paused,
        LevelClear,
        Won,
        GameOver,
    };
    Q_ENUM(State)

    enum class EndReason
    {
        Bomb,
        Quit,
        Finish,
    };
    Q_ENUM(EndReason)

    enum class GatePurpose
    {
        None,
        Link,
        GameCenter,
    };
    Q_ENUM(GatePurpose)

    /// Reads the save at once: the wallet, the settings and the top 10 are in the properties from the first read.
    /// Every pointer in `services` except `quarantine` must be set.
    GameController(GameServices services, GameOptions options = {}, QObject* parent = nullptr);

    // ---- Commands for QML ----------------------------------------------------------------------------------------

    Q_INVOKABLE void startGame();

    /// CONTINUE on the title screen: restores the saved run into the paused state. Returns false, and the game stays
    /// on the title screen without a saved run, when there is none or it cannot be restored.
    Q_INVOKABLE bool continueGame();

    Q_INVOKABLE void answer(int tile);
    Q_INVOKABLE void useHint();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void nextLevel();
    Q_INVOKABLE void keepGoing();
    Q_INVOKABLE void finish();

    /// Name entry. The typed text is normalised (capitals, A to Z and spaces, at most nameMaxLength). Returns the rank
    /// the entry took, or 0 when nothing was saved.
    Q_INVOKABLE int saveName(const QString& typed);
    Q_INVOKABLE void skipName();
    [[nodiscard]] Q_INVOKABLE QString normaliseName(const QString& typed) const;

    Q_INVOKABLE void setSoundEnabled(bool enabled);
    Q_INVOKABLE void setMusicEnabled(bool enabled);

    /// Turning Game Center off is immediate. Turning it on opens the parental gate. Ignored where the platform has no
    /// leaderboard.
    Q_INVOKABLE void requestGameCenter(bool enabled);

    /// Opens the parental gate for a web address. Only an https address is accepted: returns false for anything else.
    /// The address opens (through LinkOpener) only when submitGate() gets the right number.
    Q_INVOKABLE bool requestLink(const QString& url);

    /// The number the grown-up typed. Returns true when it was right: the pending action then runs. A wrong answer
    /// deals a new challenge, so one cannot be guessed at by trying again.
    Q_INVOKABLE bool submitGate(const QString& typed);
    Q_INVOKABLE void cancelGate();

    // ---- Hooks for the app and the tests ----------------------------------------------------------------------------

    /// Called when the application leaves or returns to the Active state (QGuiApplication::applicationStateChanged).
    /// Leaving pauses a running game, stops the tick driver, suspends audio and writes the save. Returning resumes
    /// audio only: a paused game stays on the pause screen until RESUME.
    void on_application_state(bool active);

    /// Plays the ticks the clock says are due. A QTimer calls it while a level plays; a test calls it after moving a
    /// ManualClock.
    void advance_time();

    [[nodiscard]] const SessionView& session_view() const noexcept
    {
        return m_Session.view();
    }

    // ---- Property getters
    // --------------------------------------------------------------------------------------------

    [[nodiscard]] State state() const;
    [[nodiscard]] int level() const;
    [[nodiscard]] bool endless() const;
    [[nodiscard]] int questions_per_level() const;
    [[nodiscard]] int ticks_total() const;
    [[nodiscard]] int ticks_per_second() const;
    [[nodiscard]] int alert_ticks() const;
    [[nodiscard]] int seconds_per_right_answer() const;
    [[nodiscard]] int seconds_per_wrong_answer() const;
    [[nodiscard]] int correct_answers() const;
    [[nodiscard]] int question_number() const;
    [[nodiscard]] QString question_text() const;
    [[nodiscard]] QStringList question_parts() const;
    [[nodiscard]] QList<int> tiles() const;
    [[nodiscard]] int removed_tiles() const;
    [[nodiscard]] bool hint_used() const;
    [[nodiscard]] bool hint_available() const;
    [[nodiscard]] int hint_cost() const;
    [[nodiscard]] QVariantMap answered() const;
    [[nodiscard]] int ticks_left() const;
    [[nodiscard]] int seconds_left() const;
    [[nodiscard]] bool alert() const;
    [[nodiscard]] qint64 points() const;
    [[nodiscard]] qint64 wallet() const;
    [[nodiscard]] int run() const;
    [[nodiscard]] int multiplier() const;
    [[nodiscard]] QVariantMap level_clear() const;
    [[nodiscard]] EndReason end_reason() const;
    [[nodiscard]] qint64 final_score() const;
    [[nodiscard]] int final_level() const;
    [[nodiscard]] bool new_best() const;
    [[nodiscard]] bool name_entry_pending() const;
    [[nodiscard]] int name_entry_rank() const;
    [[nodiscard]] int name_max_length() const;
    [[nodiscard]] QVariantList top10() const;
    [[nodiscard]] qint64 best_score() const;
    [[nodiscard]] int highlight_rank() const;
    [[nodiscard]] bool has_saved_run() const;
    [[nodiscard]] bool sound_enabled() const;
    [[nodiscard]] bool music_enabled() const;
    [[nodiscard]] bool game_center_enabled() const;
    [[nodiscard]] bool game_center_available() const;
    [[nodiscard]] bool gate_active() const;
    [[nodiscard]] GatePurpose gate_purpose() const;
    [[nodiscard]] QString gate_words() const;
    [[nodiscard]] bool save_blocked() const;
    [[nodiscard]] QString version_text() const;
    [[nodiscard]] QVariantMap build_info() const;
    [[nodiscard]] QString notices() const;

signals:
    // Property changes.
    void stateChanged();
    void levelChanged();
    void correctAnswersChanged();
    void questionChanged();
    void removedTilesChanged();
    void hintAvailableChanged();
    void answeredChanged();
    void timeChanged();
    void alertChanged();
    void pointsChanged();
    void walletChanged();
    void runChanged();
    void multiplierChanged();
    void levelClearChanged();
    void nameEntryChanged();
    void topScoresChanged();
    void savedRunChanged();
    void settingsChanged();
    void gateChanged();

    // One per core event, for the visual twin of each sound. Tile numbers are positions 0 to 3 in `tiles`.
    void levelStarted(int level);
    void questionShown(int questionNumber);
    void answerCorrect(int tile, int points, int coins, int streakBonusCoins, int run, int multiplier,
                       bool multiplierRose, int secondsAdded);
    void answerWrong(int tile, int correctTile, int secondsLost, int multiplier);
    void hintWasUsed(int removedTiles, int cost);
    void alertEntered();
    void levelCleared();
    void gameWon(int bonusCoins);
    void gameOver(EndReason reason, qint64 score, bool newBest, int rank);

    // The grown-up's answer to the gate was right or wrong.
    void gatePassed(GatePurpose purpose);
    void gateFailed();

    // CONTINUE found a run it could not restore.
    void continueFailed();

private:
    void process();
    void refresh_view();
    void handle_event(const Event& event);
    void remember_answer(const Event& event, bool correct);
    void finish_game(const Event& event);
    void sync_driver();
    void update_music();
    void persist();
    void begin_gate(GatePurpose purpose);
    void end_gate();
    void apply_settings();
    void remember_run();

    [[nodiscard]] static State to_state(SessionState state) noexcept;

    GameServices m_Services;
    const Tuning* m_Tuning;
    GameSession m_Session;
    SaveManager m_SaveManager;
    SaveData m_Save;
    bool m_SaveBlocked = false;

    TickDriver m_Driver;
    QTimer m_Timer;
    bool m_Active = true;

    SessionView m_Shown;  // what the signals last reported
    SessionView m_Before; // the view before the command being processed: the question an answer was given to
    QVariantMap m_Answered;
    bool m_TickingPlaying = false;
    MusicId m_Music = MusicId::NONE;

    QVariantMap m_LevelClear;
    EndReason m_EndReason = EndReason::Bomb;
    std::int64_t m_FinalScore = 0;
    int m_FinalLevel = 0;
    bool m_NewBest = false;
    int m_NameRank = 0; // 0 when the score does not qualify, or the name is dealt with
    int m_HighlightRank = 0;
    bool m_ScoreSubmitted = false;

    std::function<std::uint64_t()> m_GateSeedSource;
    GatePurpose m_GatePurpose = GatePurpose::None;
    GateChallenge m_GateChallenge;
    std::string m_GateUrl;

    BuildInfo m_BuildInfo;
};

} // namespace heart_boom_maths::game
