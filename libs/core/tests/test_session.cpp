// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/session.hpp"
#include "session_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace hbm_test;

namespace
{

const Tuning& tuning()
{
    return Tuning::defaults();
}

} // namespace

TEST_CASE("A new session is idle and ignores every command but start_game", "[unit][session]")
{
    GameSession s(tuning(), 1);
    const std::uint64_t hash = s.state_hash();
    REQUIRE(s.view().state == SessionState::IDLE);

    s.tick();
    s.answer(0);
    s.use_hint();
    s.pause();
    s.resume();
    s.quit();
    s.next_level();
    s.keep_going();
    s.finish();
    REQUIRE(s.state_hash() == hash);
    REQUIRE(s.take_events().empty());
}

TEST_CASE("start_game deals level 1 with a full timer", "[unit][session]")
{
    GameSession s(tuning(), 1);
    s.start_game();
    const SessionView& v = s.view();
    REQUIRE(v.state == SessionState::PLAYING);
    REQUIRE(v.level == 1);
    REQUIRE(v.ticks_left == tuning().level_seconds * tuning().ticks_per_second);
    REQUIRE(v.ticks_total == v.ticks_left);
    REQUIRE(v.question_number == 1);
    REQUIRE(v.question.tiles[static_cast<std::size_t>(v.question.correct_tile)] == v.question.answer);
    REQUIRE_FALSE(v.alert);
    REQUIRE_FALSE(v.endless);

    const std::vector<Event> events = s.take_events();
    REQUIRE(events.size() == 2);
    REQUIRE(events[0].kind == EventKind::LEVEL_STARTED);
    REQUIRE(events[0].level == 1);
    REQUIRE(events[0].ticks_left == v.ticks_total);
    REQUIRE(events[1].kind == EventKind::QUESTION_SHOWN);
    REQUIRE(events[1].question_number == 1);
    REQUIRE(s.take_events().empty());

    // A second start_game while playing is ignored.
    const std::uint64_t hash = s.state_hash();
    s.start_game();
    REQUIRE(s.state_hash() == hash);
}

TEST_CASE("A perfect level scores the multiplier, streak coin, time bonus and perfect bonus", "[unit][session]")
{
    GameSession s(tuning(), 7);
    s.start_game();
    (void)s.take_events();

    std::vector<Event> correct;
    for (int i = 0; i < 10; ++i)
    {
        answer_right(s);
        for (const Event& e : s.take_events())
        {
            if (e.kind == EventKind::ANSWER_CORRECT)
            {
                correct.push_back(e);
            }
            else if (e.kind == EventKind::LEVEL_CLEARED)
            {
                // Full timer: 60 whole seconds at 10 points each, plus the perfect bonus.
                REQUIRE(e.seconds_left == 60);
                REQUIRE(e.time_bonus_points == 600);
                REQUIRE(e.perfect);
                REQUIRE(e.perfect_bonus_points == 100);
                REQUIRE(e.points == 700);
                REQUIRE(e.coins == 5);
                REQUIRE(e.correct_answers == 10);
            }
        }
    }
    REQUIRE(correct.size() == 10);

    // x1 for answers 1 and 2, x2 from the third (the answer that completes the run scores at the new step, decision
    // G1), x3 from the sixth.
    const int expected_points[10] = {10, 10, 20, 20, 20, 30, 30, 30, 30, 30};
    const int expected_step[10] = {1, 1, 2, 2, 2, 3, 3, 3, 3, 3};
    for (std::size_t i = 0; i < 10; ++i)
    {
        REQUIRE(correct[i].points == expected_points[i]);
        REQUIRE(correct[i].multiplier == expected_step[i]);
        REQUIRE(correct[i].run == static_cast<int>(i) + 1);
        // Every fifth answer in a row pays a bonus coin.
        const int expected_coins = (i == 4 || i == 9) ? 2 : 1;
        REQUIRE(correct[i].coins == expected_coins);
        REQUIRE(correct[i].streak_bonus_coins == expected_coins - 1);
    }
    REQUIRE(correct[2].multiplier_rose);
    REQUIRE(correct[5].multiplier_rose);
    REQUIRE_FALSE(correct[3].multiplier_rose);

    const SessionView& v = s.view();
    REQUIRE(v.state == SessionState::LEVEL_CLEAR);
    REQUIRE(v.points == 230 + 700);
    REQUIRE(v.wallet == 12 + 5);
    // The timer is capped at the level time, so right answers on a full timer add nothing.
    REQUIRE(correct[0].ticks_delta == 0);

    s.next_level();
    REQUIRE(s.view().state == SessionState::PLAYING);
    REQUIRE(s.view().level == 2);
    REQUIRE(s.view().ticks_left == s.view().ticks_total);
    REQUIRE(s.view().run == 10); // the run carries across levels (decision G2)
    REQUIRE(s.view().multiplier == 3);
}

TEST_CASE("A wrong answer scores nothing, costs time and lowers the step", "[unit][session]")
{
    GameSession s(tuning(), 3);
    s.start_game();
    for (int i = 0; i < 6; ++i)
    {
        answer_right(s);
    }
    REQUIRE(s.view().multiplier == 3);
    const std::int64_t points = s.view().points;
    const std::int64_t wallet = s.view().wallet;
    tick_n(s, 100);
    (void)s.take_events();

    const int before = s.view().ticks_left;
    const int wrong = wrong_tile(s);
    REQUIRE(wrong >= 0);
    const int correct_tile = s.view().question.correct_tile;
    s.answer(wrong);
    const std::vector<Event> events = s.take_events();
    REQUIRE(events.size() == 2);
    REQUIRE(events[0].kind == EventKind::ANSWER_WRONG);
    REQUIRE(events[0].tile == wrong);
    REQUIRE(events[0].correct_tile == correct_tile);
    REQUIRE(events[0].ticks_delta == -60);
    REQUIRE(events[0].multiplier == 2);
    REQUIRE(events[0].run == 0);
    REQUIRE(events[1].kind == EventKind::QUESTION_SHOWN);
    REQUIRE(s.view().ticks_left == before - 60);
    REQUIRE(s.view().points == points);
    REQUIRE(s.view().wallet == wallet);
    REQUIRE(s.view().questions_answered == 7);
    REQUIRE(s.view().correct_answers == 6);

    // The ladder drops one step at a time and never below x1.
    answer_wrong(s);
    REQUIRE(s.view().multiplier == 1);
    answer_wrong(s);
    REQUIRE(s.view().multiplier == 1);
}

TEST_CASE("After a drop the ladder climbs again at 3 and 6 in a row", "[unit][session]")
{
    GameSession s(tuning(), 4);
    s.start_game();
    // The run carries across levels, and 13 answers pass one level end.
    const auto right = [&s]
    {
        if (s.view().state == SessionState::LEVEL_CLEAR)
        {
            s.next_level();
        }
        answer_right(s);
    };
    for (int i = 0; i < 6; ++i)
    {
        right();
    }
    answer_wrong(s); // x3 -> x2
    REQUIRE(s.view().multiplier == 2);
    right();
    right();
    REQUIRE(s.view().multiplier == 2);
    right(); // run of 3: at least x2, which is where it already is
    REQUIRE(s.view().multiplier == 2);
    right();
    right();
    REQUIRE(s.view().multiplier == 2);
    (void)s.take_events();
    right(); // run of 6: x3 again, on that same answer
    REQUIRE(s.view().multiplier == 3);
}

TEST_CASE("A right answer adds time but never takes the timer above the level time", "[unit][session]")
{
    GameSession s(tuning(), 5);
    s.start_game();
    tick_n(s, 100);
    (void)s.take_events();
    const int before = s.view().ticks_left;

    answer_right(s); // +140 would overshoot by 40
    const std::vector<Event> first = s.take_events();
    REQUIRE(first[0].kind == EventKind::ANSWER_CORRECT);
    REQUIRE(first[0].ticks_delta == 100);
    REQUIRE(s.view().ticks_left == before + 100);
    REQUIRE(s.view().ticks_left == s.view().ticks_total);

    tick_n(s, 300);
    (void)s.take_events();
    answer_right(s);
    REQUIRE(s.take_events()[0].ticks_delta == 140);
}

TEST_CASE("A hint removes two wrong tiles and costs two coins", "[unit][session]")
{
    GameSession s(tuning(), 11);
    s.start_game();
    (void)s.take_events();

    // An empty wallet cannot buy a hint.
    REQUIRE_FALSE(s.view().hint_available);
    const std::uint64_t hash = s.state_hash();
    s.use_hint();
    REQUIRE(s.state_hash() == hash);
    REQUIRE(s.take_events().empty());

    answer_right(s);
    answer_right(s);
    REQUIRE(s.view().wallet == 2);
    (void)s.take_events();
    const int run = s.view().run;
    const int step = s.view().multiplier;
    REQUIRE(s.view().hint_available);

    s.use_hint();
    const std::vector<Event> events = s.take_events();
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].kind == EventKind::HINT_USED);
    REQUIRE(events[0].coins == -2);
    REQUIRE(events[0].wallet == 0);
    const int mask = events[0].removed_tiles;
    REQUIRE(mask == s.view().removed_tiles);
    REQUIRE(std::popcount(static_cast<unsigned>(mask)) == tuning().hint_tiles_removed);
    REQUIRE(((mask >> s.view().question.correct_tile) & 1) == 0);
    REQUIRE(s.view().wallet == 0);
    REQUIRE(s.view().hint_used);
    REQUIRE_FALSE(s.view().hint_available);
    // A hint changes no streak and no multiplier (decision G8).
    REQUIRE(s.view().run == run);
    REQUIRE(s.view().multiplier == step);

    // One hint per question, even with coins to spare.
    s.set_wallet(5); // ignored while playing
    REQUIRE(s.view().wallet == 0);
    const std::uint64_t after_hint = s.state_hash();
    s.use_hint();
    REQUIRE(s.state_hash() == after_hint);

    // A removed tile cannot be answered.
    for (int i = 0; i < 4; ++i)
    {
        if (((mask >> i) & 1) != 0)
        {
            s.answer(i);
            REQUIRE(s.state_hash() == after_hint);
        }
    }

    // The next question can have its own hint once there are coins again.
    answer_right(s);
    REQUIRE(s.view().removed_tiles == 0);
    REQUIRE_FALSE(s.view().hint_used);
    REQUIRE(s.view().wallet == 1);
    REQUIRE_FALSE(s.view().hint_available);
}

TEST_CASE("A hint is refused with one coin and allowed with exactly the price", "[unit][session]")
{
    GameSession s(tuning(), 12);
    s.set_wallet(1);
    s.start_game();
    REQUIRE_FALSE(s.view().hint_available);
    s.use_hint();
    REQUIRE(s.view().wallet == 1);
    REQUIRE_FALSE(s.view().hint_used);
    answer_right(s); // wallet 2
    s.use_hint();
    REQUIRE(s.view().wallet == 0);
    REQUIRE(s.view().hint_used);
}

TEST_CASE("The alert zone starts at the last ten seconds and re-enters after a right answer lifts the timer out",
          "[unit][session]")
{
    GameSession s(tuning(), 2);
    s.start_game();
    (void)s.take_events();

    tick_n(s, 999); // 201 ticks left
    REQUIRE_FALSE(s.view().alert);
    REQUIRE(count_of(s.take_events(), EventKind::ALERT_ENTERED) == 0);
    s.tick(); // 200 ticks left: ten seconds
    REQUIRE(s.view().alert);
    const std::vector<Event> entered = s.take_events();
    REQUIRE(entered.size() == 1);
    REQUIRE(entered[0].kind == EventKind::ALERT_ENTERED);
    REQUIRE(entered[0].ticks_left == 200);

    tick_n(s, 50);
    REQUIRE(count_of(s.take_events(), EventKind::ALERT_ENTERED) == 0); // sent once, not on every tick

    answer_right(s); // 150 + 140 = 290, out of the zone
    REQUIRE_FALSE(s.view().alert);
    (void)s.take_events();
    tick_n(s, 90);
    REQUIRE(s.view().alert);
    REQUIRE(count_of(s.take_events(), EventKind::ALERT_ENTERED) == 1);
}

TEST_CASE("A wrong answer that drops the timer into the alert zone sends ALERT_ENTERED", "[unit][session]")
{
    GameSession s(tuning(), 2);
    s.start_game();
    tick_n(s, 900); // 300 ticks left
    (void)s.take_events();
    answer_wrong(s); // 240 ticks left, still outside
    REQUIRE_FALSE(s.view().alert);
    answer_wrong(s); // 180
    REQUIRE(s.view().alert);
    const std::vector<Event> events = s.take_events();
    REQUIRE(count_of(events, EventKind::ALERT_ENTERED) == 1);
    REQUIRE(events_of(events, EventKind::ALERT_ENTERED)[0].ticks_left == 180);
}

TEST_CASE("The bomb goes off when the timer reaches zero", "[unit][session]")
{
    GameSession s(tuning(), 9);
    s.set_wallet(7);
    s.start_game();
    answer_right(s);
    answer_right(s);
    const std::int64_t points = s.view().points;
    (void)s.take_events();

    tick_n(s, 1199);
    REQUIRE(s.view().state == SessionState::PLAYING);
    REQUIRE(s.view().ticks_left == 1);
    s.tick();
    REQUIRE(s.view().state == SessionState::GAME_OVER);
    REQUIRE(s.view().ticks_left == 0);
    REQUIRE(s.view().game_over_reason == GameOverReason::BOMB);

    const std::vector<Event> events = s.take_events();
    REQUIRE(count_of(events, EventKind::ALERT_ENTERED) == 1);
    REQUIRE(events.back().kind == EventKind::GAME_OVER);
    REQUIRE(events.back().reason == GameOverReason::BOMB);
    REQUIRE(events.back().total_points == points);
    REQUIRE(events.back().wallet == 9); // coins are kept when the bomb goes off
    REQUIRE(count_of(events, EventKind::GAME_OVER) == 1);

    // Nothing but start_game works now.
    const std::uint64_t hash = s.state_hash();
    s.tick();
    s.answer(0);
    s.use_hint();
    s.pause();
    s.resume();
    s.quit();
    s.next_level();
    s.keep_going();
    s.finish();
    REQUIRE(s.state_hash() == hash);
    REQUIRE(s.take_events().empty());

    // PLAY AGAIN starts level 1 with the score reset and the wallet kept.
    s.start_game();
    REQUIRE(s.view().state == SessionState::PLAYING);
    REQUIRE(s.view().level == 1);
    REQUIRE(s.view().points == 0);
    REQUIRE(s.view().run == 0);
    REQUIRE(s.view().multiplier == 1);
    REQUIRE(s.view().wallet == 9);
}

TEST_CASE("A wrong answer that takes the timer to zero ends the game at once without wrong-answer feedback",
          "[unit][session]")
{
    SECTION("exactly zero")
    {
        GameSession s(tuning(), 6);
        s.start_game();
        tick_n(s, 1140); // 60 ticks left: three seconds
        (void)s.take_events();
        answer_wrong(s);
        REQUIRE(s.view().state == SessionState::GAME_OVER);
        REQUIRE(s.view().ticks_left == 0);
        const std::vector<Event> events = s.take_events();
        REQUIRE(count_of(events, EventKind::ANSWER_WRONG) == 0);
        REQUIRE(events.back().kind == EventKind::GAME_OVER);
    }
    SECTION("below zero")
    {
        GameSession s(tuning(), 6);
        s.start_game();
        tick_n(s, 1180); // 20 ticks left
        (void)s.take_events();
        answer_wrong(s);
        REQUIRE(s.view().state == SessionState::GAME_OVER);
        REQUIRE(s.view().ticks_left == 0); // never negative
    }
    SECTION("one tick more and the player survives")
    {
        GameSession s(tuning(), 6);
        s.start_game();
        tick_n(s, 1139); // 61 ticks left
        (void)s.take_events();
        answer_wrong(s);
        REQUIRE(s.view().state == SessionState::PLAYING);
        REQUIRE(s.view().ticks_left == 1);
    }
}

TEST_CASE("Pause stops the clock, and only PLAYING can pause", "[unit][session]")
{
    GameSession s(tuning(), 8);
    s.pause(); // idle: ignored
    REQUIRE(s.view().state == SessionState::IDLE);
    s.start_game();
    tick_n(s, 10);
    const int ticks = s.view().ticks_left;

    s.pause();
    REQUIRE(s.view().state == SessionState::PAUSED);
    const std::uint64_t paused_hash = s.state_hash();
    tick_n(s, 50);
    s.answer(0);
    s.use_hint();
    s.next_level();
    s.keep_going();
    s.finish();
    s.start_game();
    s.pause();
    REQUIRE(s.state_hash() == paused_hash);
    REQUIRE(s.view().ticks_left == ticks);

    s.resume();
    REQUIRE(s.view().state == SessionState::PLAYING);
    s.resume(); // already playing: ignored
    s.tick();
    REQUIRE(s.view().ticks_left == ticks - 1);
}

TEST_CASE("QUIT on the pause screen is a game over that keeps the coins", "[unit][session]")
{
    GameSession s(tuning(), 8);
    s.start_game();
    answer_right(s);
    s.quit(); // not paused: ignored
    REQUIRE(s.view().state == SessionState::PLAYING);
    s.pause();
    (void)s.take_events();
    s.quit();
    REQUIRE(s.view().state == SessionState::GAME_OVER);
    REQUIRE(s.view().game_over_reason == GameOverReason::QUIT);
    REQUIRE(s.view().wallet == 1);
    const std::vector<Event> events = s.take_events();
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].kind == EventKind::GAME_OVER);
    REQUIRE(events[0].reason == GameOverReason::QUIT);
}

TEST_CASE("Clearing level 12 wins, pays 1000 coins once and offers endless play", "[unit][session]")
{
    const Tuning& t = tuning();
    GameSession s(t, 21);
    reach_level(s, t.campaign_levels);
    REQUIRE(s.view().level == t.campaign_levels);
    REQUIRE(s.view().state == SessionState::PLAYING);
    const std::int64_t wallet_before = s.view().wallet;
    (void)s.take_events();

    play_level_right(s);
    const std::vector<Event> events = s.take_events();
    REQUIRE(count_of(events, EventKind::LEVEL_CLEARED) == 1);
    REQUIRE(count_of(events, EventKind::GAME_WON) == 1);
    REQUIRE(events.back().kind == EventKind::GAME_WON);
    REQUIRE(events.back().coins == t.win_bonus_coins);
    REQUIRE(s.view().state == SessionState::WON);
    REQUIRE(s.view().won_once);
    REQUIRE(s.view().wallet == wallet_before + 10 + 2 + t.perfect_level_coins + t.win_bonus_coins);
    REQUIRE(events.back().wallet == s.view().wallet);

    // The win screen takes only KEEP GOING and FINISH.
    const std::uint64_t hash = s.state_hash();
    s.tick();
    s.answer(0);
    s.pause();
    s.next_level();
    s.start_game();
    REQUIRE(s.state_hash() == hash);

    SECTION("keep going enters endless level 13")
    {
        s.keep_going();
        REQUIRE(s.view().state == SessionState::PLAYING);
        REQUIRE(s.view().level == 13);
        REQUIRE(s.view().endless);
        const std::int64_t points = s.view().points;
        REQUIRE(points > 0);
        const std::vector<Event> started = s.take_events();
        REQUIRE(started[0].kind == EventKind::LEVEL_STARTED);
        REQUIRE(started[0].level == 13);
    }
    SECTION("finish ends the run like QUIT")
    {
        s.finish();
        REQUIRE(s.view().state == SessionState::GAME_OVER);
        REQUIRE(s.view().game_over_reason == GameOverReason::FINISH);
        REQUIRE(s.take_events().back().reason == GameOverReason::FINISH);

        // A second win in the same session, and a second game, pay nothing more.
        const std::int64_t wallet = s.view().wallet;
        s.start_game();
        reach_level(s, t.campaign_levels);
        (void)s.take_events();
        const std::int64_t before_second_win = s.view().wallet;
        play_level_right(s);
        const std::vector<Event> second = s.take_events();
        REQUIRE(second.back().kind == EventKind::GAME_WON);
        REQUIRE(second.back().coins == 0);
        REQUIRE(s.view().wallet == before_second_win + 10 + 2 + t.perfect_level_coins);
        REQUIRE(s.view().wallet > wallet);
    }
}

TEST_CASE("A win bonus paid in an earlier session is not paid again", "[unit][session]")
{
    GameSession s(tuning(), 21);
    s.set_won_once(true);
    reach_level(s, tuning().campaign_levels);
    (void)s.take_events();
    play_level_right(s);
    const std::vector<Event> events = s.take_events();
    REQUIRE(events.back().kind == EventKind::GAME_WON);
    REQUIRE(events.back().coins == 0);
}

TEST_CASE("Endless levels use the endless timer, bonus and wrong-answer numbers", "[unit][session]")
{
    const Tuning& t = tuning();
    GameSession s(t, 33);
    reach_level(s, 13);
    REQUIRE(s.view().ticks_total == t.ticks(58));
    REQUIRE(s.view().ticks_left == t.ticks(58));
    REQUIRE(s.view().seconds_per_right_answer == 7);
    REQUIRE(s.view().seconds_per_wrong_answer == 3);

    // The anchor rows of the SPEC table: level, timer seconds, seconds per right answer.
    const int anchors[][3] = {{16, 52, 6}, {20, 44, 5}, {25, 34, 3}, {27, 30, 3}, {40, 30, 3}};
    for (const auto& row : anchors)
    {
        reach_level(s, row[0]);
        REQUIRE(s.view().level == row[0]);
        REQUIRE(s.view().state == SessionState::PLAYING);
        REQUIRE(s.view().ticks_total == t.ticks(row[1]));
        REQUIRE(s.view().ticks_left == t.ticks(row[1]));
        REQUIRE(s.view().seconds_per_right_answer == row[2]);
        REQUIRE(s.view().seconds_per_wrong_answer == 3);
        REQUIRE(s.view().endless);
    }
}

TEST_CASE("In endless play a right answer adds the endless bonus and the alert zone is still ten seconds",
          "[unit][session]")
{
    const Tuning& t = tuning();
    GameSession s(t, 34);
    reach_level(s, 25);     // level time 34 s, +3 s per right answer
    tick_n(s, t.ticks(30)); // 4 s left, inside the alert zone
    REQUIRE(s.view().alert);
    (void)s.take_events();
    answer_right(s);
    const std::vector<Event> events = s.take_events();
    REQUIRE(events[0].ticks_delta == t.ticks(3));
    REQUIRE(s.view().ticks_left == t.ticks(4) + t.ticks(3));

    // From level 25 a right answer adds only what a wrong one removes.
    const int before = s.view().ticks_left;
    answer_wrong(s);
    answer_right(s);
    REQUIRE(s.view().ticks_left == before);
}

TEST_CASE("Endless level clears pay the same scoring and go on to the next level", "[unit][session]")
{
    const Tuning& t = tuning();
    GameSession s(t, 35);
    reach_level(s, 14);
    (void)s.take_events();
    const std::int64_t points = s.view().points;
    play_level_right(s);
    const std::vector<Event> events = s.take_events();
    REQUIRE(events.back().kind == EventKind::LEVEL_CLEARED);
    REQUIRE(events.back().level == 14);
    REQUIRE(events.back().seconds_left == endless_level_seconds(t.endless, 2));
    REQUIRE(s.view().state == SessionState::LEVEL_CLEAR);
    REQUIRE(s.view().points > points);
    s.next_level();
    REQUIRE(s.view().level == 15);
}

TEST_CASE("Leftover seconds convert to points rounded down", "[unit][session]")
{
    GameSession s(tuning(), 40);
    s.start_game();
    tick_n(s, 19); // 1181 ticks left: 59 whole seconds
    (void)s.take_events();
    play_level_right(s);
    const std::vector<Event> events = s.take_events();
    const std::vector<Event> cleared = events_of(events, EventKind::LEVEL_CLEARED);
    REQUIRE(cleared.size() == 1);
    // The right answers add time up to the cap, so the clear sees the full timer again.
    REQUIRE(cleared[0].seconds_left == 60);

    GameSession u(tuning(), 40);
    u.start_game();
    answer_wrong(u); // 1140 ticks
    tick_n(u, 19);   // 1121 ticks: 56 whole seconds and a bit
    (void)u.take_events();
    // Answer the remaining nine questions wrong so no time is added.
    for (int i = 0; i < 9; ++i)
    {
        answer_wrong(u);
        if (u.view().state != SessionState::PLAYING)
        {
            break;
        }
    }
    // 9 wrong answers cost 540 ticks: 581 left, 29 whole seconds.
    const std::vector<Event> end = events_of(u.take_events(), EventKind::LEVEL_CLEARED);
    REQUIRE(end.size() == 1);
    REQUIRE(end[0].seconds_left == 29);
    REQUIRE(end[0].time_bonus_points == 290);
    REQUIRE_FALSE(end[0].perfect);
    REQUIRE(end[0].perfect_bonus_points == 0);
    REQUIRE(end[0].coins == 0);
}

TEST_CASE("The same seed and commands give the same events and hash", "[unit][session]")
{
    const std::vector<Command> script = random_script(99, 3000);
    GameSession a(tuning(), 1234);
    GameSession b(tuning(), 1234);
    GameSession c(tuning(), 4321);
    for (const Command& cmd : script)
    {
        apply(a, cmd);
        apply(b, cmd);
        apply(c, cmd);
        REQUIRE(a.take_events() == b.take_events());
        REQUIRE(a.state_hash() == b.state_hash());
    }
    // The seed is part of the state, so another seed never hashes the same.
    REQUIRE(a.state_hash() != c.state_hash());
}

TEST_CASE("Event::append_to lists every field in a fixed order", "[unit][session]")
{
    Event e;
    e.kind = EventKind::GAME_OVER;
    e.level = 3;
    e.total_points = 99;
    std::vector<std::int64_t> out;
    e.append_to(out);
    REQUIRE(out.size() == 22);
    REQUIRE(out[0] == static_cast<std::int64_t>(EventKind::GAME_OVER));
    REQUIRE(out[1] == 3);
    REQUIRE(out[20] == 99);
}
