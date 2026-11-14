// SPDX-License-Identifier: GPL-3.0-or-later

// Properties that cross the generator, the session and the replay runner: the real generator seen through whole
// sessions at every level including endless, realistic timing, and determinism across runs and across a snapshot.
// The session's own command-sequence properties are in test_session_property.cpp.

#include "heart_boom_maths/question_generator.hpp"
#include "heart_boom_maths/replay.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/session.hpp"
#include "heart_boom_maths/state_hash.hpp"
#include "session_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace hbm_test;

namespace
{

const Tuning& tuning()
{
    return Tuning::defaults();
}

int largest_number(int level)
{
    return level <= tuning().campaign_levels ? tuning().levels[static_cast<std::size_t>(level - 1)].largest_number
                                             : tuning().endless.largest_number;
}

/// The value of the sentence worked out here, not by the generator.
int value_of(const Question& q)
{
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        return q.terms[1] - q.terms[0];
    }
    int value = q.terms[0];
    for (std::size_t i = 1; i < static_cast<std::size_t>(q.term_count); ++i)
    {
        value = q.operators[i - 1] == Operator::ADD ? value + q.terms[i] : value - q.terms[i];
    }
    return value;
}

/// What a question must satisfy on its own. Returns the first problem, or an empty string.
std::string problem_with(const Question& q, int level)
{
    const int largest = largest_number(level);
    const int minimum = tuning().operand_minimum;
    const bool campaign = level <= tuning().campaign_levels;

    if (campaign && q.shape != tuning().levels[static_cast<std::size_t>(level - 1)].shape)
    {
        return "shape does not match the level table";
    }
    const int terms = q.shape == QuestionShape::THREE_TERM ? 3 : (q.shape == QuestionShape::FOUR_TERM ? 4 : 2);
    if (q.term_count != terms)
    {
        return "term count does not match the shape";
    }
    for (int i = 0; i < q.term_count; ++i)
    {
        if (q.terms[static_cast<std::size_t>(i)] < minimum || q.terms[static_cast<std::size_t>(i)] > largest)
        {
            return "operand out of range";
        }
    }
    if (q.answer != value_of(q))
    {
        return "the answer is not the value of the sentence";
    }
    if (q.shape != QuestionShape::MISSING_ADDEND)
    {
        int running = q.terms[0];
        for (int i = 1; i < q.term_count; ++i)
        {
            const std::size_t at = static_cast<std::size_t>(i);
            running = q.operators[at - 1] == Operator::ADD ? running + q.terms[at] : running - q.terms[at];
            if (running < 0 || running > largest)
            {
                return "an intermediate result is out of range";
            }
        }
    }
    else if (q.answer < minimum || q.answer > largest)
    {
        return "the missing addend is out of range";
    }
    if (campaign)
    {
        const Operations operations = tuning().levels[static_cast<std::size_t>(level - 1)].operations;
        for (int i = 0; i + 1 < q.term_count && q.shape != QuestionShape::MISSING_ADDEND; ++i)
        {
            const Operator op = q.operators[static_cast<std::size_t>(i)];
            if ((operations == Operations::ADD && op != Operator::ADD) ||
                (operations == Operations::TAKE_AWAY && op != Operator::SUBTRACT))
            {
                return "an operator the level does not use";
            }
        }
    }
    if (q.correct_tile < 0 || q.correct_tile > 3 || q.tiles[static_cast<std::size_t>(q.correct_tile)] != q.answer)
    {
        return "the correct tile does not hold the answer";
    }
    if (std::count(q.tiles.begin(), q.tiles.end(), q.answer) != 1)
    {
        return "the answer is not on exactly one tile";
    }
    if (std::set<int>(q.tiles.begin(), q.tiles.end()).size() != 4)
    {
        return "the tiles are not distinct";
    }
    for (const int tile : q.tiles)
    {
        if (tile < 0 || tile > largest)
        {
            return "a tile is out of range";
        }
    }
    return {};
}

using Identity = std::tuple<int, int, std::array<int, 4>, std::array<int, 3>>;

Identity identity_of(const Question& q)
{
    std::array<int, 3> ops{};
    for (std::size_t i = 0; i < ops.size(); ++i)
    {
        ops[i] = static_cast<int>(q.operators[i]);
    }
    return {static_cast<int>(q.shape), q.term_count, q.terms, ops};
}

struct Params
{
    int wrong_percent = 0;
    int hint_percent = 0;
    int pause_percent = 0;
    int max_delay_ticks = 0;
    int target_level = 30;
    int max_commands = 6000;
    std::int64_t start_wallet = 0;
};

struct Stats
{
    int max_level = 0;
    int wins = 0;
    int bombs = 0;
    int alerts = 0;
    int levels_cleared = 0;
    int hints_bought = 0;
    int hints_refused = 0;
    int games = 0;
    int questions = 0;
    int endless_questions = 0;
};

/// Plays one seed like a child would, one command at a time, and checks every rule after every command. Stops when
/// `target_level` starts or after `max_commands`.
void drive(std::uint64_t seed, const Params& p, Stats& stats)
{
    GameSession s(tuning(), seed);
    const Tuning& t = tuning();
    QuestionGenerator probe(t);
    Rng rng(seed * 2654435761ULL + 17);
    s.set_wallet(p.start_wallet);

    // Stops the test with a message, but only when the condition fails, so a million checks stay cheap.
    const auto rule = [&](bool holds, std::string_view message)
    {
        if (!holds)
        {
            INFO("seed " << seed << ", level " << s.view().level << ": " << message);
            REQUIRE(holds);
        }
    };

    std::set<Identity> dealt; // the questions of the level on screen
    int dealt_count = 0;
    SessionView before = s.view();

    // Looks at what a command did. `command_is_start` is true for start_game, the only command that lowers points.
    const auto check = [&](bool command_is_start)
    {
        const SessionView& v = s.view();
        const std::vector<Event> events = s.take_events();
        if (!command_is_start)
        {
            rule(v.points >= before.points, "points went down");
            rule(v.level >= before.level, "the level went down");
        }
        rule(v.wallet >= 0, "the wallet went negative");
        rule(v.ticks_left >= 0, "the clock went negative");
        rule(v.ticks_left <= v.ticks_total, "the clock is above the level's cap");
        rule(v.state != SessionState::PLAYING || v.ticks_left > 0, "playing with no time left");
        rule(v.state != SessionState::GAME_OVER || v.game_over_reason != GameOverReason::BOMB || v.ticks_left == 0,
             "the bomb went off with time left");
        rule(v.alert == (v.state == SessionState::PLAYING || v.state == SessionState::PAUSED
                             ? v.ticks_left <= t.ticks(v.endless ? t.endless.alert_seconds : t.alert_seconds)
                             : false),
             "the alert flag does not match the clock");
        rule(std::count_if(events.begin(), events.end(),
                           [](const Event& e) { return e.kind == EventKind::QUESTION_SHOWN; }) <= 1,
             "two questions in one command");
        rule(std::count_if(events.begin(), events.end(),
                           [](const Event& e) { return e.kind == EventKind::GAME_OVER; }) <= 1,
             "two game-overs in one command");

        for (const Event& e : events)
        {
            switch (e.kind)
            {
            case EventKind::LEVEL_STARTED:
                dealt.clear();
                dealt_count = 0;
                rule(e.ticks_left == v.ticks_total, "a level did not start with its full fuse");
                break;
            case EventKind::QUESTION_SHOWN:
            {
                const std::string problem = problem_with(v.question, v.level);
                rule(problem.empty(), problem);
                rule(dealt.insert(identity_of(v.question)).second, "a question repeated within its level");
                ++dealt_count;
                rule(e.question_number == dealt_count, "the question number does not count the questions");
                rule(v.question_number == dealt_count, "the view's question number is wrong");
                ++stats.questions;
                stats.endless_questions += v.endless ? 1 : 0;
                break;
            }
            case EventKind::LEVEL_CLEARED:
                ++stats.levels_cleared;
                rule(static_cast<int>(dealt.size()) == t.questions_per_level, "a level did not deal ten");
                if (e.level == 1)
                {
                    // Level 1 has exactly ten questions, and a perfect pass deals each of them once.
                    rule(probe.possible_question_count(1) == t.questions_per_level,
                         "level 1 no longer has ten questions");
                }
                break;
            case EventKind::GAME_WON:
                ++stats.wins;
                break;
            case EventKind::GAME_OVER:
                stats.bombs += e.reason == GameOverReason::BOMB ? 1 : 0;
                break;
            case EventKind::ALERT_ENTERED:
                ++stats.alerts;
                break;
            default:
                break;
            }
        }
        stats.max_level = std::max(stats.max_level, v.level);
        before = v;
    };

    for (int command = 0; command < p.max_commands; ++command)
    {
        const SessionView& v = s.view();
        switch (v.state)
        {
        case SessionState::IDLE:
        case SessionState::GAME_OVER:
            s.start_game();
            ++stats.games;
            check(true);
            break;
        case SessionState::LEVEL_CLEAR:
            s.next_level();
            check(false);
            break;
        case SessionState::WON:
            s.keep_going();
            check(false);
            break;
        case SessionState::PAUSED:
            s.resume();
            check(false);
            break;
        case SessionState::PLAYING:
        {
            if (v.level >= p.target_level)
            {
                return;
            }
            const int delay = p.max_delay_ticks > 0 ? rng.between(0, p.max_delay_ticks) : 0;
            for (int i = 0; i < delay && s.view().state == SessionState::PLAYING; ++i)
            {
                s.tick();
                check(false);
            }
            if (s.view().state != SessionState::PLAYING)
            {
                break;
            }

            if (rng.between(0, 99) < p.pause_percent)
            {
                s.pause();
                check(false);
                const std::uint64_t paused_hash = s.state_hash();
                for (int i = 0; i < 40; ++i)
                {
                    s.tick();
                }
                rule(s.state_hash() == paused_hash, "time passed while paused");
                rule(s.take_events().empty(), "events arrived while paused");
                s.resume();
                check(false);
            }

            if (rng.between(0, 99) < p.hint_percent)
            {
                const SessionView now = s.view();
                const bool allowed = now.wallet >= t.hint_cost_coins && !now.hint_used;
                const std::uint64_t hash = s.state_hash();
                s.use_hint();
                if (allowed)
                {
                    rule(s.view().wallet == now.wallet - t.hint_cost_coins, "a hint cost the wrong price");
                    rule(s.view().hint_used, "a hint did not mark the question");
                    rule(std::popcount(static_cast<unsigned>(s.view().removed_tiles)) == t.hint_tiles_removed,
                         "a hint removed the wrong number of tiles");
                    rule(((s.view().removed_tiles >> now.question.correct_tile) & 1) == 0,
                         "a hint removed the right tile");
                    ++stats.hints_bought;
                }
                else
                {
                    rule(s.state_hash() == hash, "a refused hint changed the game");
                    ++stats.hints_refused;
                }
                check(false);
            }

            if (rng.between(0, 99) < p.wrong_percent && wrong_tile(s) >= 0)
            {
                answer_wrong(s);
            }
            else
            {
                answer_right(s);
            }
            check(false);
            break;
        }
        }
    }
}

} // namespace

TEST_CASE("Perfect play deals valid questions through every level into deep endless play", "[property][generator]")
{
    Stats stats;
    Params p;
    p.max_delay_ticks = 10;
    p.target_level = 40;
    p.max_commands = 12000;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        drive(seed, p, stats);
    }
    CHECK(stats.max_level >= 40);
    CHECK(stats.wins >= 40);
    CHECK(stats.endless_questions > 20 * 10);
    CHECK(stats.levels_cleared >= 40 * 39);
}

TEST_CASE("Careless play with hints, pauses and wrong answers keeps every rule", "[property][session]")
{
    Stats stats;
    Params p;
    p.wrong_percent = 18;
    p.hint_percent = 25;
    p.pause_percent = 10;
    p.max_delay_ticks = 140;
    p.start_wallet = 3;
    p.target_level = 30;
    p.max_commands = 4000;
    for (std::uint64_t seed = 1000; seed < 1080; ++seed)
    {
        drive(seed, p, stats);
    }
    // The run has to reach the places the rules are about, or it proves little.
    CHECK(stats.bombs > 0);
    CHECK(stats.alerts > 0);
    CHECK(stats.hints_bought > 0);
    CHECK(stats.hints_refused > 0);
    CHECK(stats.wins > 0);
    CHECK(stats.max_level > tuning().campaign_levels);
    CHECK(stats.games > 80); // games ended by the bomb and began again
}

TEST_CASE("Slow play drowns in the bomb on every level, and a wrong answer at the edge is a boom",
          "[property][session]")
{
    for (int level : {1, 5, 12, 13, 20, 40})
    {
        GameSession s(tuning(), 77);
        reach_level(s, level);
        REQUIRE(s.view().level == level);
        (void)s.take_events();
        const int fuse = s.view().ticks_left;
        // The clock runs out exactly when the fuse's last tick is spent, never before.
        tick_n(s, fuse - 1);
        REQUIRE(s.view().state == SessionState::PLAYING);
        REQUIRE(s.view().ticks_left == 1);
        s.tick();
        REQUIRE(s.view().state == SessionState::GAME_OVER);
        REQUIRE(s.view().ticks_left == 0);
        const std::vector<Event> events = s.take_events();
        REQUIRE(count_of(events, EventKind::GAME_OVER) == 1);
        REQUIRE(events.back().reason == GameOverReason::BOMB);
    }
}

TEST_CASE("Commands before a game and after it ends change nothing", "[property][session]")
{
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        GameSession s(tuning(), seed);
        const auto nothing_happens = [&]
        {
            const std::uint64_t hash = s.state_hash();
            s.tick();
            s.answer(0);
            s.answer(3);
            s.use_hint();
            s.pause();
            s.resume();
            s.quit();
            s.next_level();
            s.keep_going();
            s.finish();
            REQUIRE(s.state_hash() == hash);
            REQUIRE(s.take_events().empty());
        };
        nothing_happens(); // before the first game

        s.set_wallet(50);
        s.start_game();
        (void)s.take_events();
        tick_n(s, 100000); // the bomb
        REQUIRE(s.view().state == SessionState::GAME_OVER);
        (void)s.take_events();
        nothing_happens(); // after the bomb

        s.start_game();
        answer_right(s);
        s.pause();
        s.quit();
        REQUIRE(s.view().state == SessionState::GAME_OVER);
        (void)s.take_events();
        nothing_happens(); // after a quit
    }
}

TEST_CASE("The same seed and commands give the same events and hashes, with or without a snapshot in the middle",
          "[property][replay]")
{
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        const std::vector<Command> script = random_script(seed * 104729, 900);
        Rng rng(seed);
        const std::size_t cut = static_cast<std::size_t>(rng.between(0, static_cast<int>(script.size()) - 1));

        GameSession a(tuning(), seed);
        GameSession b(tuning(), seed);
        std::vector<std::int64_t> a_events;
        std::vector<std::int64_t> b_events;
        for (const Command& c : script)
        {
            apply(a, c);
            apply(b, c);
            for (const Event& e : a.take_events())
            {
                e.append_to(a_events);
            }
            for (const Event& e : b.take_events())
            {
                e.append_to(b_events);
            }
        }
        // Two runs of the same seed and commands cannot be told apart.
        REQUIRE(a_events == b_events);
        REQUIRE(a.state_hash() == b.state_hash());
        REQUIRE(fnv1a_64(a_events) == fnv1a_64(b_events));

        // A run that is saved at a random point and restored into a session made with another seed carries on the same.
        GameSession original(tuning(), seed);
        for (std::size_t i = 0; i < cut; ++i)
        {
            apply(original, script[i]);
        }
        (void)original.take_events();
        GameSession restored(tuning(), seed + 5000);
        REQUIRE(restored.restore(original.snapshot()));
        std::vector<std::int64_t> original_tail;
        std::vector<std::int64_t> restored_tail;
        for (std::size_t i = cut; i < script.size(); ++i)
        {
            apply(original, script[i]);
            apply(restored, script[i]);
            for (const Event& e : original.take_events())
            {
                e.append_to(original_tail);
            }
            for (const Event& e : restored.take_events())
            {
                e.append_to(restored_tail);
            }
            REQUIRE(original.state_hash() == restored.state_hash());
        }
        REQUIRE(original_tail == restored_tail);
        // And the whole run, original or restored, ends where the uninterrupted one ended.
        REQUIRE(restored.state_hash() == a.state_hash());
    }
}

TEST_CASE("A replay recorded from random play plays back identically", "[property][replay]")
{
    // Turn a random script into a replay (ticks become tick numbers), play it, and play the file text again.
    for (std::uint64_t seed = 1; seed <= 25; ++seed)
    {
        Replay replay;
        replay.seed = seed;
        replay.wallet = static_cast<std::int64_t>(seed % 5);
        int tick = 0;
        for (const Command& c : random_script(seed * 31, 600))
        {
            switch (c.kind)
            {
            case Cmd::TICK:
                ++tick;
                break;
            case Cmd::TICK_BURST:
                tick += c.arg;
                break;
            case Cmd::ANSWER_RIGHT:
            case Cmd::ANSWER_WRONG:
            case Cmd::ANSWER_ANY:
                replay.commands.push_back({tick, ReplayAction::ANSWER, c.arg % 6 - 1});
                break;
            case Cmd::HINT:
                replay.commands.push_back({tick, ReplayAction::USE_HINT, -1});
                break;
            case Cmd::PAUSE:
                replay.commands.push_back({tick, ReplayAction::PAUSE, -1});
                break;
            case Cmd::RESUME:
                replay.commands.push_back({tick, ReplayAction::RESUME, -1});
                break;
            case Cmd::QUIT:
                replay.commands.push_back({tick, ReplayAction::QUIT, -1});
                break;
            case Cmd::NEXT_LEVEL:
                replay.commands.push_back({tick, ReplayAction::NEXT_LEVEL, -1});
                break;
            case Cmd::KEEP_GOING:
                replay.commands.push_back({tick, ReplayAction::KEEP_GOING, -1});
                break;
            case Cmd::FINISH:
                replay.commands.push_back({tick, ReplayAction::FINISH, -1});
                break;
            case Cmd::START:
                replay.commands.push_back({tick, ReplayAction::START_GAME, -1});
                break;
            case Cmd::SET_WALLET:
                break;
            }
        }
        const auto first = run_replay(replay);
        REQUIRE(first.has_value());
        replay.expect = first->as_expect();

        const auto reread = parse_replay(write_replay(replay));
        REQUIRE(reread.has_value());
        REQUIRE(*reread == replay);
        const auto second = run_replay(*reread);
        REQUIRE(second.has_value());
        REQUIRE(second->as_expect() == *replay.expect);
        REQUIRE(second->event_count == first->event_count);
    }
}
