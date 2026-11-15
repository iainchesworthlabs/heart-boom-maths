// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/session.hpp"
#include "session_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

using namespace hbm_test;

namespace
{

const Tuning& tuning()
{
    return Tuning::defaults();
}

struct Coverage
{
    int wins = 0;
    int endless_levels = 0;
    int bombs = 0;
    int quits = 0;
    int finishes = 0;
    int hints = 0;
    int alerts = 0;
    int refused_hints = 0;
};

/// Applies one command and checks every rule that must hold whatever the commands were.
void apply_and_check(GameSession& s, const Command& cmd, Coverage& cover)
{
    const Tuning& t = tuning();
    const SessionView before = s.view();
    const std::uint64_t hash_before = s.state_hash();
    (void)s.take_events(); // the previous command's events are not this command's

    // Work out whether the command should be accepted from the state it is sent in.
    const bool playing = before.state == SessionState::PLAYING;
    bool accepted = false;
    switch (cmd.kind)
    {
    case Cmd::TICK:
    case Cmd::TICK_BURST:
    case Cmd::PAUSE:
        accepted = playing;
        break;
    case Cmd::ANSWER_RIGHT:
        accepted = playing;
        break;
    case Cmd::ANSWER_WRONG:
        accepted = playing && wrong_tile(s) >= 0;
        break;
    case Cmd::ANSWER_ANY:
        accepted = playing && cmd.arg >= 0 && cmd.arg < 4 && ((before.removed_tiles >> cmd.arg) & 1) == 0;
        break;
    case Cmd::HINT:
        accepted = playing && !before.hint_used && before.wallet >= t.hint_cost_coins;
        break;
    case Cmd::RESUME:
    case Cmd::QUIT:
        accepted = before.state == SessionState::PAUSED;
        break;
    case Cmd::NEXT_LEVEL:
        accepted = before.state == SessionState::LEVEL_CLEAR;
        break;
    case Cmd::KEEP_GOING:
    case Cmd::FINISH:
        accepted = before.state == SessionState::WON;
        break;
    case Cmd::START:
        accepted = before.state == SessionState::IDLE || before.state == SessionState::GAME_OVER;
        break;
    case Cmd::SET_WALLET:
        accepted = before.state == SessionState::IDLE || before.state == SessionState::GAME_OVER ||
                   before.state == SessionState::PAUSED;
        break;
    }

    if (cmd.kind == Cmd::HINT && !accepted)
    {
        ++cover.refused_hints;
    }

    apply(s, cmd);
    const std::vector<Event> events = s.take_events();
    const SessionView& v = s.view();

    // A command the state does not accept changes nothing at all.
    if (!accepted)
    {
        REQUIRE(s.state_hash() == hash_before);
        REQUIRE(events.empty());
        return;
    }

    const bool new_game = cmd.kind == Cmd::START;

    // Time is never negative and never above the level's timer.
    REQUIRE(v.ticks_left >= 0);
    REQUIRE(v.ticks_left <= v.ticks_total);
    if (v.state == SessionState::PLAYING || v.state == SessionState::PAUSED)
    {
        REQUIRE(v.ticks_left > 0);
        REQUIRE(v.alert == (v.ticks_left <= t.ticks(v.endless ? t.endless.alert_seconds : t.alert_seconds)));
    }
    else
    {
        REQUIRE_FALSE(v.alert);
    }

    // Points and the level counter only grow inside one game.
    if (!new_game)
    {
        REQUIRE(v.points >= before.points);
        REQUIRE(v.level >= before.level);
    }
    REQUIRE(v.points >= 0);
    REQUIRE(v.questions_answered <= t.questions_per_level);
    REQUIRE(v.correct_answers <= v.questions_answered);
    REQUIRE(v.multiplier >= 1);
    REQUIRE(v.multiplier <= 3);
    REQUIRE(v.run >= 0);

    // The wallet is never negative and moves only by what the events say.
    REQUIRE(v.wallet >= 0);
    if (cmd.kind != Cmd::SET_WALLET)
    {
        std::int64_t expected = before.wallet;
        for (const Event& e : events)
        {
            expected += e.coins;
        }
        REQUIRE(v.wallet == expected);
    }

    // A hint is bought exactly when the wallet, the state and the question allow it, for exactly the price.
    if (cmd.kind == Cmd::HINT)
    {
        REQUIRE(count_of(events, EventKind::HINT_USED) == 1);
        REQUIRE(v.wallet == before.wallet - t.hint_cost_coins);
        REQUIRE(v.run == before.run);
        REQUIRE(v.multiplier == before.multiplier);
        ++cover.hints;
    }
    else
    {
        REQUIRE(count_of(events, EventKind::HINT_USED) == 0);
    }

    // The bomb goes off only at zero, and only the right commands end a game another way.
    for (const Event& e : events)
    {
        switch (e.kind)
        {
        case EventKind::GAME_OVER:
            REQUIRE(v.state == SessionState::GAME_OVER);
            if (e.reason == GameOverReason::BOMB)
            {
                REQUIRE(v.ticks_left == 0);
                REQUIRE((cmd.kind == Cmd::TICK || cmd.kind == Cmd::TICK_BURST || cmd.kind == Cmd::ANSWER_WRONG ||
                         cmd.kind == Cmd::ANSWER_ANY));
                ++cover.bombs;
            }
            else if (e.reason == GameOverReason::QUIT)
            {
                REQUIRE(cmd.kind == Cmd::QUIT);
                ++cover.quits;
            }
            else
            {
                REQUIRE(cmd.kind == Cmd::FINISH);
                ++cover.finishes;
            }
            break;
        case EventKind::GAME_WON:
            ++cover.wins;
            break;
        case EventKind::ALERT_ENTERED:
            ++cover.alerts;
            break;
        case EventKind::LEVEL_STARTED:
            if (e.level > t.campaign_levels)
            {
                ++cover.endless_levels;
            }
            break;
        default:
            break;
        }
    }
    if (v.state == SessionState::GAME_OVER && v.game_over_reason == GameOverReason::BOMB)
    {
        REQUIRE(v.ticks_left == 0);
    }

    // Whatever happened, the question on screen is consistent.
    if (v.level > 0)
    {
        REQUIRE(v.question.tiles[static_cast<std::size_t>(v.question.correct_tile)] == v.question.answer);
    }
}

} // namespace

TEST_CASE("Random command sequences never crash and never break a rule", "[property][session]")
{
    Coverage cover;
    for (std::uint64_t seed = 1; seed <= 150; ++seed)
    {
        GameSession s(tuning(), seed);
        const std::vector<Command> script = random_script(seed * 7919, 2500);
        int index = 0;
        for (const Command& cmd : script)
        {
            apply_and_check(s, cmd, cover);
            // Every so often a clone built from a snapshot must hash the same, whatever the state.
            if (++index % 53 == 0)
            {
                GameSession clone(tuning(), 0);
                REQUIRE(clone.restore(s.snapshot()));
                REQUIRE(clone.state_hash() == s.state_hash());
            }
        }
    }
    // The scripts must actually reach the interesting places, or the test proves little.
    REQUIRE(cover.wins > 0);
    REQUIRE(cover.endless_levels > 0);
    REQUIRE(cover.bombs > 0);
    REQUIRE(cover.quits > 0);
    REQUIRE(cover.finishes > 0);
    REQUIRE(cover.hints > 0);
    REQUIRE(cover.alerts > 0);
    REQUIRE(cover.refused_hints > 0);
}

TEST_CASE("Pause and resume at any tick change nothing but the clock", "[property][session]")
{
    // Two runs of the same commands, one of which pauses and resumes (and ticks while paused) between every command.
    const std::vector<Command> script = random_script(555, 1500);
    GameSession plain(tuning(), 77);
    GameSession paused(tuning(), 77);
    for (const Command& cmd : script)
    {
        if (cmd.kind == Cmd::PAUSE || cmd.kind == Cmd::RESUME || cmd.kind == Cmd::QUIT)
        {
            continue; // these would change the plain run too
        }
        apply(plain, cmd);
        paused.pause();
        tick_n(paused, 25);
        paused.answer(0);
        paused.use_hint();
        paused.resume();
        apply(paused, cmd);
        REQUIRE(plain.take_events() == paused.take_events());
        REQUIRE(plain.state_hash() == paused.state_hash());
    }
}

TEST_CASE("A restored session continues exactly like the original", "[unit][session]")
{
    const std::vector<Command> script = random_script(2026, 1800);
    // Snapshots at many points of one long script, so every state and every mid-question situation shows up.
    for (std::size_t cut = 0; cut < script.size(); cut += 17)
    {
        GameSession original(tuning(), 42);
        for (std::size_t i = 0; i < cut; ++i)
        {
            apply(original, script[i]);
        }
        (void)original.take_events();

        GameSession restored(tuning(), 999);
        REQUIRE(restored.restore(original.snapshot()));
        REQUIRE(restored.state_hash() == original.state_hash());

        // Follow with the next 120 commands, comparing events and hash at every step.
        const std::size_t end = std::min(script.size(), cut + 120);
        for (std::size_t i = cut; i < end; ++i)
        {
            apply(original, script[i]);
            apply(restored, script[i]);
            REQUIRE(original.take_events() == restored.take_events());
            REQUIRE(original.state_hash() == restored.state_hash());
        }
    }
}

TEST_CASE("A restore in the middle of a question after a hint continues identically", "[unit][session]")
{
    GameSession a(tuning(), 8);
    a.set_wallet(10);
    a.start_game();
    answer_right(a);
    tick_n(a, 77);
    a.use_hint();
    a.pause();
    (void)a.take_events();
    REQUIRE(a.view().hint_used);
    REQUIRE(a.view().removed_tiles != 0);

    GameSession b(tuning(), 1);
    REQUIRE(b.restore(a.snapshot()));
    REQUIRE(b.view().state == SessionState::PAUSED);
    REQUIRE(b.view().removed_tiles == a.view().removed_tiles);
    REQUIRE(b.view().hint_used);
    REQUIRE(b.view().question.tiles == a.view().question.tiles);
    REQUIRE(b.view().ticks_left == a.view().ticks_left);
    REQUIRE(b.view().wallet == a.view().wallet);

    a.resume();
    b.resume();
    for (int i = 0; i < 40; ++i)
    {
        a.tick();
        b.tick();
        answer_right(a);
        answer_right(b);
        a.use_hint();
        b.use_hint();
        REQUIRE(a.take_events() == b.take_events());
        REQUIRE(a.state_hash() == b.state_hash());
    }
}

TEST_CASE("A restore keeps the random generator so later hints pick the same tiles", "[unit][session]")
{
    GameSession a(tuning(), 3);
    a.set_wallet(100);
    a.start_game();
    GameSession b(tuning(), 3);
    REQUIRE(b.restore(a.snapshot()));
    for (int i = 0; i < 5; ++i)
    {
        a.use_hint();
        b.use_hint();
        REQUIRE(a.view().removed_tiles == b.view().removed_tiles);
        answer_right(a);
        answer_right(b);
    }
}

TEST_CASE("restore rejects garbage and leaves the session untouched", "[unit][session]")
{
    GameSession s(tuning(), 5);
    s.set_wallet(4);
    s.start_game();
    answer_right(s);
    tick_n(s, 30);
    s.use_hint();
    s.pause();
    const std::vector<std::int64_t> good = s.snapshot();
    const std::uint64_t hash = s.state_hash();
    const std::vector<Event> pending = [&]
    {
        GameSession probe = s;
        return probe.take_events();
    }();
    REQUIRE_FALSE(pending.empty());

    const auto rejects = [&](const std::vector<std::int64_t>& values)
    {
        const bool ok = s.restore(values);
        REQUIRE(s.state_hash() == hash);
        return !ok;
    };

    SECTION("wrong sizes")
    {
        REQUIRE(rejects({}));
        REQUIRE(rejects({1}));
        REQUIRE(rejects(std::vector<std::int64_t>(good.begin(), good.begin() + 20)));
        REQUIRE(rejects(std::vector<std::int64_t>(good.begin(), good.end() - 1)));
        std::vector<std::int64_t> longer = good;
        longer.push_back(0);
        REQUIRE(rejects(longer));
    }
    SECTION("wrong format number")
    {
        std::vector<std::int64_t> bad = good;
        bad[0] = 2;
        REQUIRE(rejects(bad));
    }
    SECTION("named fields that do not add up")
    {
        const auto with = [&](std::size_t index, std::int64_t value)
        {
            std::vector<std::int64_t> bad = good;
            bad.at(index) = value;
            return bad;
        };
        REQUIRE(rejects(with(1, 99)));    // state
        REQUIRE(rejects(with(2, -1)));    // level
        REQUIRE(rejects(with(2, 0)));     // level 0 only before a game
        REQUIRE(rejects(with(3, 10)));    // a live level has fewer than 10 answered
        REQUIRE(rejects(with(5, 0)));     // a live level has time left
        REQUIRE(rejects(with(5, 99999))); // more than the level's timer
        REQUIRE(rejects(with(6, -1)));    // points
        REQUIRE(rejects(with(7, -1)));    // wallet
        REQUIRE(rejects(with(9, 0)));     // step
        REQUIRE(rejects(with(9, 4)));
        REQUIRE(rejects(with(13, 15)));           // removed tiles that include the right one or too many
        REQUIRE(rejects(with(10, 0)));            // hint flag off with tiles removed
        REQUIRE(rejects(with(29, 4)));            // correct tile out of range
        REQUIRE(rejects(with(24, good[24] + 1))); // tiles[correct] != answer
        REQUIRE(rejects(with(30, 5)));            // generator length
        REQUIRE(rejects(with(15, 9)));            // question shape
        REQUIRE(rejects(with(16, 1)));            // term count
    }
    SECTION("any one value set to an extreme either restores cleanly or changes nothing")
    {
        int rejected = 0;
        for (std::size_t i = 0; i < good.size(); ++i)
        {
            for (const std::int64_t extreme :
                 {std::int64_t{-1}, std::int64_t{7}, std::int64_t{1} << 62, std::numeric_limits<std::int64_t>::min(),
                  std::numeric_limits<std::int64_t>::max()})
            {
                std::vector<std::int64_t> bad = good;
                bad[i] = extreme;
                GameSession probe(tuning(), 5);
                if (!probe.restore(bad))
                {
                    ++rejected;
                    // The probe was fresh, so a rejected restore left it exactly as a new session.
                    REQUIRE(probe.state_hash() == GameSession(tuning(), 5).state_hash());
                }
            }
        }
        REQUIRE(static_cast<std::size_t>(rejected) > good.size()); // most single-value corruptions are caught
    }

    // After every rejection the pending events are still there and the good snapshot still loads.
    REQUIRE(s.take_events() == pending);
    REQUIRE(s.restore(good));
    REQUIRE(s.state_hash() == hash);
}

TEST_CASE("restore accepts a snapshot from every state", "[unit][session]")
{
    const Tuning& t = tuning();
    GameSession s(t, 17);
    GameSession fresh(t, 1);
    REQUIRE(fresh.restore(s.snapshot())); // idle

    reach_level(s, 2); // playing at level 2
    REQUIRE(fresh.restore(s.snapshot()));
    s.pause();
    REQUIRE(fresh.restore(s.snapshot()));
    REQUIRE(fresh.view().state == SessionState::PAUSED);
    s.resume();

    GameSession u(t, 18);
    u.start_game();
    play_level_right(u);
    REQUIRE(u.view().state == SessionState::LEVEL_CLEAR);
    REQUIRE(fresh.restore(u.snapshot()));
    REQUIRE(fresh.view().state == SessionState::LEVEL_CLEAR);

    GameSession w(t, 19);
    reach_level(w, t.campaign_levels);
    play_level_right(w);
    REQUIRE(w.view().state == SessionState::WON);
    REQUIRE(fresh.restore(w.snapshot()));
    REQUIRE(fresh.view().state == SessionState::WON);
    fresh.keep_going();
    REQUIRE(fresh.view().level == 13);

    w.finish();
    REQUIRE(fresh.restore(w.snapshot()));
    REQUIRE(fresh.view().state == SessionState::GAME_OVER);
    REQUIRE(fresh.view().game_over_reason == GameOverReason::FINISH);

    GameSession x(t, 20);
    x.start_game();
    tick_n(x, t.ticks(t.level_seconds));
    REQUIRE(x.view().state == SessionState::GAME_OVER);
    REQUIRE(fresh.restore(x.snapshot()));
    REQUIRE(fresh.view().game_over_reason == GameOverReason::BOMB);
}
