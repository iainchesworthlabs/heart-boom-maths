// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/question.hpp"
#include "heart_boom_maths/question_generator.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace heart_boom_maths
{

enum class SessionState
{
    IDLE,        // before the first game: the title screen
    PLAYING,     // the clock runs only here
    PAUSED,      // RESUME or QUIT
    LEVEL_CLEAR, // the level-clear card; next_level() moves on
    WON,         // level 12 is cleared; keep_going() or finish()
    GAME_OVER,   // start_game() plays again
};

enum class GameOverReason
{
    BOMB,   // the timer reached zero
    QUIT,   // QUIT on the pause screen
    FINISH, // FINISH on the win screen
};

enum class EventKind
{
    LEVEL_STARTED,
    QUESTION_SHOWN,
    ANSWER_CORRECT,
    ANSWER_WRONG,
    HINT_USED,
    ALERT_ENTERED,
    LEVEL_CLEARED,
    GAME_WON,
    GAME_OVER,
};

/// One thing that happened, with the numbers the screens and sounds need. It is a flat struct so it is cheap to copy,
/// compare and hash. A field a kind does not use stays at its default. Which fields each kind fills:
///
///  - LEVEL_STARTED:  level, ticks_left (the full timer).
///  - QUESTION_SHOWN: level, question_number (1 based). The question itself is in `view()`.
///  - ANSWER_CORRECT: level, question_number, tile, correct_tile, points (this answer), coins (this answer, including
///                    the streak bonus), streak_bonus_coins (0 or 1), run, multiplier, multiplier_rose, ticks_delta
///                    (time actually added, after the cap), ticks_left, total_points, wallet.
///  - ANSWER_WRONG:   level, question_number, tile, correct_tile, run (0), multiplier (after the drop), ticks_delta
///                    (negative), ticks_left, total_points, wallet. A wrong answer that takes the timer to zero sends
///                    GAME_OVER alone and no ANSWER_WRONG (decision G3: the boom replaces the wrong-answer feedback).
///  - HINT_USED:      level, question_number, removed_tiles (bit i set means tile i is gone), coins (negative: the
///                    price), wallet.
///  - ALERT_ENTERED:  level, ticks_left. Sent again if the timer climbs out and falls back in.
///  - LEVEL_CLEARED:  level, correct_answers, seconds_left (whole seconds), ticks_left, time_bonus_points,
///                    perfect_bonus_points, perfect, points (time bonus plus perfect bonus), coins (the perfect-level
///                    coins), total_points, wallet.
///  - GAME_WON:       level, coins (the win bonus, 0 when it was paid before), total_points, wallet.
///  - GAME_OVER:      level, reason, total_points, wallet.
struct Event
{
    EventKind kind = EventKind::LEVEL_STARTED;
    int level = 0;
    int question_number = 0;
    int tile = -1;
    int correct_tile = -1;
    int removed_tiles = 0;
    int points = 0;
    int time_bonus_points = 0;
    int perfect_bonus_points = 0;
    int coins = 0;
    int streak_bonus_coins = 0;
    int run = 0;
    int multiplier = 1;
    bool multiplier_rose = false;
    int ticks_delta = 0;
    int ticks_left = 0;
    int seconds_left = 0;
    int correct_answers = 0;
    bool perfect = false;
    GameOverReason reason = GameOverReason::BOMB;
    std::int64_t total_points = 0;
    std::int64_t wallet = 0;

    friend bool operator==(const Event&, const Event&) = default;

    /// The event as a fixed list of integers, appended to `out`. Replays hash these, so the order never changes
    /// without regenerating every golden replay.
    void append_to(std::vector<std::int64_t>& out) const;
};

/// Everything a screen draws, read every frame. The fields at the top are the saved state; the rest is derived from
/// them after each command so the app never recomputes a rule.
struct SessionView
{
    SessionState state = SessionState::IDLE;
    int level = 0; // 0 before the first game; 13 and above is endless
    int questions_answered = 0;
    int correct_answers = 0;
    int ticks_left = 0;
    std::int64_t points = 0;
    std::int64_t wallet = 0;
    int run = 0;            // correct answers in a row, carried across levels
    int multiplier = 1;     // the step: 1, 2 or 3
    bool hint_used = false; // for the question on screen
    bool won_once = false;
    GameOverReason game_over_reason = GameOverReason::BOMB;
    Question question{};   // the question on screen (the last one when no question is live)
    int removed_tiles = 0; // bit i set means a hint removed tile i

    // Derived.
    bool endless = false;
    int questions_per_level = 0;
    int question_number = 0; // 1 based number of the question on screen
    int ticks_total = 0;     // the level's full timer
    int seconds_left = 0;    // whole seconds, rounded down
    bool alert = false;
    bool hint_available = false; // false when used, when the wallet is short, and when the game is not being played
    int seconds_per_right_answer = 0;
    int seconds_per_wrong_answer = 0;
};

/// The whole rules engine. Commands go in, events come out, and one seed with one list of commands always gives one
/// run. It reads no clock: the app calls tick() 20 times a second of real time, and stops calling it to pause.
///
/// A command in a state that does not accept it is ignored: it changes nothing and sends no event.
///
///   IDLE or GAME_OVER:  start_game(), set_wallet(), set_won_once()
///   PLAYING:            tick(), answer(), use_hint(), pause()
///   PAUSED:             resume(), quit()
///   LEVEL_CLEAR:        next_level()
///   WON:                keep_going(), finish()
///
/// The tuning must outlive the session.
class GameSession
{
public:
    GameSession(const Tuning& tuning, std::uint64_t seed);

    /// Starts a new game at level 1. Points, run and step reset; the wallet and `won_once` carry over.
    void start_game();

    /// One twentieth of a second (Tuning::ticks_per_second) of play.
    void tick();

    /// Answers with the tile at `tile_index`. An index outside the tiles, or a tile a hint removed, is ignored.
    void answer(int tile_index);

    /// Spends the hint price and removes tiles. Refused (ignored) when the wallet is short or the question already
    /// had a hint. It does not change the run or the step (decision G8).
    void use_hint();

    void pause();
    void resume();

    /// QUIT on the pause screen: the game ends, the score can enter the top 10, the coins stay.
    void quit();

    /// Leaves the level-clear card for the next level with a fresh timer.
    void next_level();

    /// KEEP GOING on the win screen: level 13, the first endless level.
    void keep_going();

    /// FINISH on the win screen: the same as QUIT (decision G4).
    void finish();

    /// The wallet and the win-bonus flag come from the save file. Only accepted before a game starts (IDLE or
    /// GAME_OVER). A negative wallet becomes 0.
    void set_wallet(std::int64_t coins);
    void set_won_once(bool won_once);

    [[nodiscard]] const SessionView& view() const noexcept
    {
        return m_View;
    }

    /// Hands over the events since the last call, oldest first.
    [[nodiscard]] std::vector<Event> take_events();

    /// The whole state as integers: the view's saved fields, the random generator and the question generator. Events
    /// not yet taken are not part of it, so take them first.
    [[nodiscard]] std::vector<std::int64_t> snapshot() const;

    /// Replaces the state with a snapshot. Returns false and changes nothing when the values are not a state this
    /// session could have produced (wrong length, a value out of range, a question that does not add up). Pending
    /// events are dropped on success.
    [[nodiscard]] bool restore(std::span<const std::int64_t> values);

    /// FNV-1a over snapshot().
    [[nodiscard]] std::uint64_t state_hash() const;

private:
    void begin_level(int level);
    void deal_question();
    void clear_level();
    void end_game(GameOverReason reason);
    void refresh_view();
    void push(const Event& event);
    [[nodiscard]] Event make_event(EventKind kind) const;

    const Tuning* m_Tuning;
    Rng m_Rng;
    QuestionGenerator m_Generator;
    SessionView m_View;
    std::vector<Event> m_Events;
};

} // namespace heart_boom_maths
