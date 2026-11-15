// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/session.hpp"

#include "heart_boom_maths/profile.hpp"
#include "heart_boom_maths/state_hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <utility>

namespace heart_boom_maths
{

namespace
{

// Version 1 of the snapshot layout. Bump it when the list of integers changes.
constexpr std::int64_t SNAPSHOT_FORMAT = 1;

// Index of the first value of the question generator's own state, after the length that precedes it.
constexpr std::size_t SNAPSHOT_FIXED_VALUES = 31;

// A sanity ceiling for a wallet read from a save file, so that adding coins can never overflow. It is not a game rule.
constexpr std::int64_t MAX_WALLET = std::int64_t{1} << 40;

// The same kind of ceiling for a restored level number: no real game gets near it.
constexpr int MAX_LEVEL = 1'000'000;

constexpr std::int64_t SMALL_LIMIT = 1'000'000'000;

// The wallet and the win-bonus mark come from the save. They are taken before a game starts and, since a saved run
// holds the wallet as it was at the pause, again on the pause screen after a restore, when the player's wallet has moved
// on (a sync, or the same player's coins earned on another device).
[[nodiscard]] bool accepts_save_values(SessionState state) noexcept
{
    return state == SessionState::IDLE || state == SessionState::GAME_OVER || state == SessionState::PAUSED;
}

[[nodiscard]] bool is_endless(const Tuning& t, int level) noexcept
{
    return level > t.campaign_levels;
}

[[nodiscard]] int endless_k(const Tuning& t, int level) noexcept
{
    return level - t.endless.first_level + 1;
}

[[nodiscard]] int level_seconds(const Tuning& t, int level) noexcept
{
    return is_endless(t, level) ? endless_level_seconds(t.endless, endless_k(t, level)) : t.level_seconds;
}

[[nodiscard]] int right_answer_seconds(const Tuning& t, int level) noexcept
{
    return is_endless(t, level) ? endless_bonus_seconds(t.endless, endless_k(t, level)) : t.right_answer_seconds;
}

[[nodiscard]] int wrong_answer_seconds(const Tuning& t, int level) noexcept
{
    return is_endless(t, level) ? t.endless.wrong_answer_seconds : t.wrong_answer_seconds;
}

[[nodiscard]] int alert_seconds(const Tuning& t, int level) noexcept
{
    return is_endless(t, level) ? t.endless.alert_seconds : t.alert_seconds;
}

/// The step a run of this length earns on its own, before any earlier step is taken into account.
[[nodiscard]] int step_for_run(const Tuning& t, int run) noexcept
{
    int step = 1;
    for (const MultiplierStep& entry : t.multiplier_steps)
    {
        if (run >= entry.run)
        {
            step = std::max(step, entry.step);
        }
    }
    return step;
}

[[nodiscard]] int top_step(const Tuning& t) noexcept
{
    int step = 1;
    for (const MultiplierStep& entry : t.multiplier_steps)
    {
        step = std::max(step, entry.step);
    }
    return step;
}

[[nodiscard]] bool in_range(std::int64_t value, std::int64_t low, std::int64_t high) noexcept
{
    return value >= low && value <= high;
}

[[nodiscard]] bool state_is_live(SessionState state) noexcept
{
    return state == SessionState::PLAYING || state == SessionState::PAUSED;
}

} // namespace

void Event::append_to(std::vector<std::int64_t>& out) const
{
    out.insert(out.end(), {static_cast<std::int64_t>(kind),
                           level,
                           question_number,
                           tile,
                           correct_tile,
                           removed_tiles,
                           points,
                           time_bonus_points,
                           perfect_bonus_points,
                           coins,
                           streak_bonus_coins,
                           run,
                           multiplier,
                           multiplier_rose ? 1 : 0,
                           ticks_delta,
                           ticks_left,
                           seconds_left,
                           correct_answers,
                           perfect ? 1 : 0,
                           static_cast<std::int64_t>(reason),
                           total_points,
                           wallet});
}

GameSession::GameSession(const Tuning& tuning, std::uint64_t seed) : m_Tuning(&tuning), m_Rng(seed), m_Generator(tuning)
{
    refresh_view();
}

Event GameSession::make_event(EventKind kind) const
{
    Event e;
    e.kind = kind;
    e.level = m_View.level;
    e.total_points = m_View.points;
    e.wallet = m_View.wallet;
    return e;
}

void GameSession::push(const Event& event)
{
    m_Events.push_back(event);
}

std::vector<Event> GameSession::take_events()
{
    return std::exchange(m_Events, {});
}

void GameSession::refresh_view()
{
    const Tuning& t = *m_Tuning;
    SessionView& v = m_View;
    const int level = std::max(v.level, 1);
    v.endless = is_endless(t, level);
    v.questions_per_level = t.questions_per_level;
    v.ticks_total = t.ticks(level_seconds(t, level));
    v.seconds_left = v.ticks_left / t.ticks_per_second;
    v.alert = state_is_live(v.state) && v.ticks_left <= t.ticks(alert_seconds(t, level));
    v.question_number = v.state == SessionState::PLAYING || v.state == SessionState::PAUSED ? v.questions_answered + 1
                                                                                            : v.questions_answered;
    v.seconds_per_right_answer = right_answer_seconds(t, level);
    v.seconds_per_wrong_answer = wrong_answer_seconds(t, level);
    v.hint_available = v.state == SessionState::PLAYING && !v.hint_used && v.wallet >= t.hint_cost_coins;
}

void GameSession::start_game()
{
    HBM_ZONE("session.start_game");
    if (m_View.state != SessionState::IDLE && m_View.state != SessionState::GAME_OVER)
    {
        return;
    }
    m_View.points = 0;
    m_View.run = 0;
    m_View.multiplier = 1;
    m_View.game_over_reason = GameOverReason::BOMB;
    begin_level(1);
}

void GameSession::begin_level(int level)
{
    HBM_ZONE("session.begin_level");
    const Tuning& t = *m_Tuning;
    m_View.level = level;
    m_View.questions_answered = 0;
    m_View.correct_answers = 0;
    m_View.ticks_left = t.ticks(level_seconds(t, level));
    m_View.state = SessionState::PLAYING;
    m_Generator.begin_level(level, m_Rng);

    Event started = make_event(EventKind::LEVEL_STARTED);
    started.ticks_left = m_View.ticks_left;
    push(started);
    deal_question();
}

void GameSession::deal_question()
{
    HBM_ZONE("session.deal_question");
    m_View.question = m_Generator.next(m_Rng);
    m_View.hint_used = false;
    m_View.removed_tiles = 0;
    refresh_view();

    Event shown = make_event(EventKind::QUESTION_SHOWN);
    shown.question_number = m_View.question_number;
    push(shown);
}

void GameSession::tick()
{
    HBM_ZONE("session.tick");
    if (m_View.state != SessionState::PLAYING)
    {
        return;
    }
    const Tuning& t = *m_Tuning;
    const int alert_ticks = t.ticks(alert_seconds(t, m_View.level));
    const bool was_alert = m_View.ticks_left <= alert_ticks;
    --m_View.ticks_left;
    if (m_View.ticks_left <= 0)
    {
        m_View.ticks_left = 0;
        end_game(GameOverReason::BOMB);
        return;
    }
    refresh_view();
    HBM_PLOT("session.ticks_left", m_View.ticks_left);
    if (!was_alert && m_View.alert)
    {
        HBM_MESSAGE("session.alert_entered");
        Event alert = make_event(EventKind::ALERT_ENTERED);
        alert.ticks_left = m_View.ticks_left;
        push(alert);
    }
}

void GameSession::answer(int tile_index)
{
    HBM_ZONE("session.answer");
    if (m_View.state != SessionState::PLAYING)
    {
        return;
    }
    const Tuning& t = *m_Tuning;
    const Question& q = m_View.question;
    if (tile_index < 0 || tile_index >= static_cast<int>(q.tiles.size()) ||
        ((m_View.removed_tiles >> tile_index) & 1) != 0)
    {
        return;
    }

    const int level_ticks = t.ticks(level_seconds(t, m_View.level));
    const int alert_ticks = t.ticks(alert_seconds(t, m_View.level));
    const bool was_alert = m_View.ticks_left <= alert_ticks;
    const int answered_number = m_View.questions_answered + 1;
    ++m_View.questions_answered;

    if (tile_index == q.correct_tile)
    {
        const int old_step = m_View.multiplier;
        ++m_View.run;
        ++m_View.correct_answers;
        // The answer that completes a run is scored at the new step (decision G1).
        m_View.multiplier = std::max(m_View.multiplier, step_for_run(t, m_View.run));

        const int points = t.base_points * m_View.multiplier;
        const int streak_coins = m_View.run % t.streak_bonus_coin_every == 0 ? 1 : 0;
        const int coins = 1 + streak_coins;
        m_View.points += points;
        m_View.wallet += coins;

        const int before = m_View.ticks_left;
        int after = before + t.ticks(right_answer_seconds(t, m_View.level));
        if (t.right_answer_time_capped_at_level_time)
        {
            after = std::min(after, std::max(level_ticks, before));
        }
        m_View.ticks_left = after;

        Event e = make_event(EventKind::ANSWER_CORRECT);
        e.question_number = answered_number;
        e.tile = tile_index;
        e.correct_tile = q.correct_tile;
        e.points = points;
        e.coins = coins;
        e.streak_bonus_coins = streak_coins;
        e.run = m_View.run;
        e.multiplier = m_View.multiplier;
        e.multiplier_rose = m_View.multiplier > old_step;
        e.ticks_delta = after - before;
        e.ticks_left = after;
        push(e);
    }
    else
    {
        m_View.run = 0;
        m_View.multiplier = std::max(1, m_View.multiplier - 1);

        const int before = m_View.ticks_left;
        const int after = before - t.ticks(wrong_answer_seconds(t, m_View.level));
        if (after <= 0)
        {
            // Decision G3: the bomb goes off at once and the wrong-answer feedback is skipped.
            m_View.ticks_left = 0;
            end_game(GameOverReason::BOMB);
            return;
        }
        m_View.ticks_left = after;

        Event e = make_event(EventKind::ANSWER_WRONG);
        e.question_number = answered_number;
        e.tile = tile_index;
        e.correct_tile = q.correct_tile;
        e.run = 0;
        e.multiplier = m_View.multiplier;
        e.ticks_delta = after - before;
        e.ticks_left = after;
        push(e);
    }

    refresh_view();
    if (!was_alert && m_View.alert)
    {
        Event alert = make_event(EventKind::ALERT_ENTERED);
        alert.ticks_left = m_View.ticks_left;
        push(alert);
    }

    if (m_View.questions_answered >= t.questions_per_level)
    {
        clear_level();
    }
    else
    {
        deal_question();
    }
}

void GameSession::clear_level()
{
    const Tuning& t = *m_Tuning;
    SessionView& v = m_View;

    const int seconds_left = v.ticks_left / t.ticks_per_second; // whole seconds, rounded down (decision G7)
    const int time_bonus = seconds_left * t.time_bonus_points_per_second;
    const bool perfect = v.correct_answers == t.questions_per_level;
    const int perfect_points = perfect ? t.perfect_level_bonus_points : 0;
    const int perfect_coins = perfect ? t.perfect_level_coins : 0;
    // Added in 64 bits: the two bonuses are ints, and their sum is added to a 64-bit score.
    v.points += std::int64_t{time_bonus} + perfect_points;
    v.wallet += perfect_coins;

    Event cleared = make_event(EventKind::LEVEL_CLEARED);
    cleared.correct_answers = v.correct_answers;
    cleared.seconds_left = seconds_left;
    cleared.ticks_left = v.ticks_left;
    cleared.time_bonus_points = time_bonus;
    cleared.perfect_bonus_points = perfect_points;
    cleared.perfect = perfect;
    cleared.points = time_bonus + perfect_points;
    cleared.coins = perfect_coins;

    if (v.level == t.campaign_levels)
    {
        v.state = SessionState::WON;
        const int bonus = v.won_once ? 0 : t.win_bonus_coins;
        v.wallet += bonus;
        v.won_once = true;
        refresh_view();
        push(cleared);

        Event won = make_event(EventKind::GAME_WON);
        won.coins = bonus;
        push(won);
        return;
    }
    v.state = SessionState::LEVEL_CLEAR;
    refresh_view();
    push(cleared);
}

void GameSession::end_game(GameOverReason reason)
{
    m_View.state = SessionState::GAME_OVER;
    m_View.game_over_reason = reason;
    refresh_view();
    Event over = make_event(EventKind::GAME_OVER);
    over.reason = reason;
    push(over);
}

void GameSession::use_hint()
{
    HBM_ZONE("session.use_hint");
    if (m_View.state != SessionState::PLAYING || m_View.hint_used)
    {
        return;
    }
    const Tuning& t = *m_Tuning;
    if (m_View.wallet < t.hint_cost_coins)
    {
        return;
    }

    std::array<int, 4> wrong{};
    std::size_t wrong_count = 0;
    for (int i = 0; i < static_cast<int>(m_View.question.tiles.size()); ++i)
    {
        if (i != m_View.question.correct_tile)
        {
            wrong[wrong_count++] = i;
        }
    }
    // The session's own generator picks which wrong tiles go, so a replay removes the same ones.
    m_Rng.shuffle(std::span<int>(wrong.data(), wrong_count));
    const auto removed = std::min(static_cast<std::size_t>(t.hint_tiles_removed), wrong_count);
    int mask = 0;
    for (std::size_t i = 0; i < removed; ++i)
    {
        mask |= 1 << wrong[i];
    }

    m_View.wallet -= t.hint_cost_coins;
    m_View.hint_used = true;
    m_View.removed_tiles = mask;
    refresh_view();

    Event e = make_event(EventKind::HINT_USED);
    e.question_number = m_View.question_number;
    e.removed_tiles = mask;
    e.coins = -t.hint_cost_coins;
    push(e);
}

void GameSession::pause()
{
    if (m_View.state == SessionState::PLAYING)
    {
        m_View.state = SessionState::PAUSED;
        refresh_view();
    }
}

void GameSession::resume()
{
    if (m_View.state == SessionState::PAUSED)
    {
        m_View.state = SessionState::PLAYING;
        refresh_view();
    }
}

void GameSession::quit()
{
    if (m_View.state == SessionState::PAUSED)
    {
        end_game(GameOverReason::QUIT);
    }
}

void GameSession::next_level()
{
    HBM_ZONE("session.next_level");
    if (m_View.state == SessionState::LEVEL_CLEAR)
    {
        begin_level(m_View.level + 1);
    }
}

void GameSession::keep_going()
{
    if (m_View.state == SessionState::WON)
    {
        begin_level(m_View.level + 1);
    }
}

void GameSession::finish()
{
    if (m_View.state == SessionState::WON)
    {
        end_game(GameOverReason::FINISH);
    }
}

void GameSession::set_wallet(std::int64_t coins)
{
    if (accepts_save_values(m_View.state))
    {
        m_View.wallet = std::clamp<std::int64_t>(coins, 0, MAX_WALLET);
        refresh_view();
    }
}

void GameSession::set_won_once(bool won_once)
{
    if (accepts_save_values(m_View.state))
    {
        m_View.won_once = won_once;
    }
}

std::vector<std::int64_t> GameSession::snapshot() const
{
    HBM_ZONE("session.snapshot");
    const SessionView& v = m_View;
    const Question& q = v.question;
    std::vector<std::int64_t> out;
    out.reserve(SNAPSHOT_FIXED_VALUES + 8);
    out.insert(out.end(),
               {SNAPSHOT_FORMAT, static_cast<std::int64_t>(v.state), v.level, v.questions_answered, v.correct_answers,
                v.ticks_left, v.points, v.wallet, v.run, v.multiplier, v.hint_used ? 1 : 0, v.won_once ? 1 : 0,
                static_cast<std::int64_t>(v.game_over_reason), v.removed_tiles,
                static_cast<std::int64_t>(m_Rng.state()), static_cast<std::int64_t>(q.shape), q.term_count});
    out.insert(out.end(), q.terms.begin(), q.terms.end());
    for (const Operator op : q.operators)
    {
        out.push_back(static_cast<std::int64_t>(op));
    }
    out.push_back(q.answer);
    out.insert(out.end(), q.tiles.begin(), q.tiles.end());
    out.push_back(q.correct_tile);

    const std::vector<std::int64_t> generator = m_Generator.snapshot();
    out.push_back(static_cast<std::int64_t>(generator.size()));
    out.insert(out.end(), generator.begin(), generator.end());
    return out;
}

bool GameSession::restore(std::span<const std::int64_t> values)
{
    HBM_ZONE("session.restore");
    const Tuning& t = *m_Tuning;
    if (values.size() < SNAPSHOT_FIXED_VALUES || values[0] != SNAPSHOT_FORMAT)
    {
        return false;
    }
    const std::int64_t generator_size = values[30];
    if (generator_size < 0 || static_cast<std::uint64_t>(generator_size) != values.size() - SNAPSHOT_FIXED_VALUES)
    {
        return false;
    }

    // Read every value into a candidate and check it first, so a bad snapshot changes nothing.
    SessionView c;
    if (!in_range(values[1], 0, static_cast<std::int64_t>(SessionState::GAME_OVER)) ||
        !in_range(values[2], 0, MAX_LEVEL) || !in_range(values[3], 0, t.questions_per_level) ||
        !in_range(values[4], 0, t.questions_per_level) || !in_range(values[5], 0, SMALL_LIMIT) ||
        !in_range(values[6], 0, std::numeric_limits<std::int64_t>::max() / 2) || !in_range(values[7], 0, MAX_WALLET) ||
        !in_range(values[8], 0, SMALL_LIMIT) || !in_range(values[9], 1, top_step(t)) || !in_range(values[10], 0, 1) ||
        !in_range(values[11], 0, 1) || !in_range(values[12], 0, static_cast<std::int64_t>(GameOverReason::FINISH)) ||
        !in_range(values[13], 0, (1 << c.question.tiles.size()) - 1))
    {
        return false;
    }
    c.state = static_cast<SessionState>(values[1]);
    c.level = static_cast<int>(values[2]);
    c.questions_answered = static_cast<int>(values[3]);
    c.correct_answers = static_cast<int>(values[4]);
    c.ticks_left = static_cast<int>(values[5]);
    c.points = values[6];
    c.wallet = values[7];
    c.run = static_cast<int>(values[8]);
    c.multiplier = static_cast<int>(values[9]);
    c.hint_used = values[10] == 1;
    c.won_once = values[11] == 1;
    c.game_over_reason = static_cast<GameOverReason>(values[12]);
    c.removed_tiles = static_cast<int>(values[13]);
    const auto rng_state = static_cast<std::uint64_t>(values[14]);

    Question& q = c.question;
    if (!in_range(values[15], 0, static_cast<std::int64_t>(QuestionShape::FOUR_TERM)) || !in_range(values[16], 2, 4))
    {
        return false;
    }
    q.shape = static_cast<QuestionShape>(values[15]);
    q.term_count = static_cast<int>(values[16]);
    for (std::size_t i = 0; i < q.terms.size(); ++i)
    {
        if (!in_range(values[17 + i], -SMALL_LIMIT, SMALL_LIMIT))
        {
            return false;
        }
        q.terms[i] = static_cast<int>(values[17 + i]);
    }
    for (std::size_t i = 0; i < q.operators.size(); ++i)
    {
        if (!in_range(values[21 + i], 0, static_cast<std::int64_t>(Operator::SUBTRACT)))
        {
            return false;
        }
        q.operators[i] = static_cast<Operator>(values[21 + i]);
    }
    if (!in_range(values[24], -SMALL_LIMIT, SMALL_LIMIT))
    {
        return false;
    }
    q.answer = static_cast<int>(values[24]);
    for (std::size_t i = 0; i < q.tiles.size(); ++i)
    {
        if (!in_range(values[25 + i], -SMALL_LIMIT, SMALL_LIMIT))
        {
            return false;
        }
        q.tiles[i] = static_cast<int>(values[25 + i]);
    }
    if (!in_range(values[29], 0, static_cast<std::int64_t>(q.tiles.size()) - 1))
    {
        return false;
    }
    q.correct_tile = static_cast<int>(values[29]);
    if (q.tiles[static_cast<std::size_t>(q.correct_tile)] != q.answer)
    {
        return false;
    }

    // The values must also agree with each other.
    const bool idle = c.state == SessionState::IDLE;
    if (idle != (c.level == 0) || c.correct_answers > c.questions_answered)
    {
        return false;
    }
    const int level_ticks = t.ticks(level_seconds(t, std::max(c.level, 1)));
    if (c.ticks_left > level_ticks || c.multiplier < step_for_run(t, c.run))
    {
        return false;
    }
    switch (c.state)
    {
    case SessionState::IDLE:
        if (c.questions_answered != 0 || c.ticks_left != 0 || c.run != 0 || c.points != 0)
        {
            return false;
        }
        break;
    case SessionState::PLAYING:
    case SessionState::PAUSED:
        if (c.questions_answered >= t.questions_per_level || c.ticks_left < 1)
        {
            return false;
        }
        break;
    case SessionState::LEVEL_CLEAR:
        if (c.questions_answered != t.questions_per_level || c.ticks_left < 1 || c.level == t.campaign_levels)
        {
            return false;
        }
        break;
    case SessionState::WON:
        if (c.questions_answered != t.questions_per_level || c.ticks_left < 1 || c.level != t.campaign_levels ||
            !c.won_once)
        {
            return false;
        }
        break;
    case SessionState::GAME_OVER:
        // A bomb means the timer is at zero, and only a bomb leaves it there.
        if ((c.game_over_reason == GameOverReason::BOMB) != (c.ticks_left == 0))
        {
            return false;
        }
        break;
    }
    if (c.state != SessionState::GAME_OVER && c.game_over_reason != GameOverReason::BOMB)
    {
        return false;
    }
    if (is_endless(t, c.level) && c.state != SessionState::GAME_OVER && !c.won_once)
    {
        return false;
    }

    // A hint removes the set number of wrong tiles and never the right one.
    const int wrong_tiles = static_cast<int>(q.tiles.size()) - 1;
    const int expected_removed = std::min(t.hint_tiles_removed, wrong_tiles);
    if (c.hint_used)
    {
        if (std::popcount(static_cast<unsigned>(c.removed_tiles)) != expected_removed ||
            ((c.removed_tiles >> q.correct_tile) & 1) != 0)
        {
            return false;
        }
    }
    else if (c.removed_tiles != 0)
    {
        return false;
    }

    QuestionGenerator generator = m_Generator;
    if (!generator.restore(values.subspan(SNAPSHOT_FIXED_VALUES)))
    {
        return false;
    }

    m_View = c;
    m_Rng.set_state(rng_state);
    m_Generator = std::move(generator);
    m_Events.clear();
    refresh_view();
    return true;
}

std::uint64_t GameSession::state_hash() const
{
    HBM_ZONE("session.state_hash");
    const std::vector<std::int64_t> values = snapshot();
    return fnv1a_64(values);
}

} // namespace heart_boom_maths
