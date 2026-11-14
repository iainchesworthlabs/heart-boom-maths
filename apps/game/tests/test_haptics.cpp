// SPDX-License-Identifier: GPL-3.0-or-later
#include "event_haptics.hpp"
#include "fakes.hpp"
#include "services/haptic_maps.hpp"
#include "services/haptic_patterns.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <vector>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;

namespace
{

constexpr std::array<HapticId, 9> ALL_IDS = {
    HapticId::TAP,   HapticId::RIGHT,       HapticId::WRONG, HapticId::HINT, HapticId::MULTIPLIER_UP,
    HapticId::ALERT, HapticId::LEVEL_CLEAR, HapticId::WIN,   HapticId::BOOM,
};

Event make(EventKind kind)
{
    Event e;
    e.kind = kind;
    return e;
}

Event game_over(GameOverReason reason)
{
    Event e = make(EventKind::GAME_OVER);
    e.reason = reason;
    return e;
}

using Ids = std::vector<HapticId>;

} // namespace

// The event table

TEST_CASE("Events call for the haptics of the table", "[unit][haptics]")
{
    CHECK(haptic_for_event(make(EventKind::ANSWER_CORRECT)) == HapticId::RIGHT);

    Event streak = make(EventKind::ANSWER_CORRECT);
    streak.multiplier_rose = true;
    CHECK(haptic_for_event(streak) == HapticId::MULTIPLIER_UP);

    CHECK(haptic_for_event(make(EventKind::ANSWER_WRONG)) == HapticId::WRONG);
    CHECK(haptic_for_event(make(EventKind::HINT_USED)) == HapticId::HINT);
    CHECK(haptic_for_event(make(EventKind::ALERT_ENTERED)) == HapticId::ALERT);
    CHECK(haptic_for_event(make(EventKind::LEVEL_CLEARED)) == HapticId::LEVEL_CLEAR);
    CHECK(haptic_for_event(make(EventKind::GAME_WON)) == HapticId::WIN);
    CHECK(haptic_for_event(game_over(GameOverReason::BOMB)) == HapticId::BOOM);
}

TEST_CASE("Level starts, question starts, a quit and a finish are not felt", "[unit][haptics]")
{
    CHECK_FALSE(haptic_for_event(make(EventKind::LEVEL_STARTED)).has_value());
    CHECK_FALSE(haptic_for_event(make(EventKind::QUESTION_SHOWN)).has_value());
    CHECK_FALSE(haptic_for_event(game_over(GameOverReason::QUIT)).has_value());
    CHECK_FALSE(haptic_for_event(game_over(GameOverReason::FINISH)).has_value());
}

TEST_CASE("One command gives the strongest haptic of its events", "[unit][haptics]")
{
    CHECK_FALSE(haptic_for_events({}).has_value());

    // The tenth right answer: the answer and the level clear arrive together, and the clear is felt.
    const std::array level_end = {make(EventKind::ANSWER_CORRECT), make(EventKind::LEVEL_CLEARED),
                                  make(EventKind::LEVEL_STARTED)};
    CHECK(haptic_for_events(level_end) == HapticId::LEVEL_CLEAR);

    // The last level: the clear and the win arrive together, and the win is felt.
    const std::array win = {make(EventKind::ANSWER_CORRECT), make(EventKind::LEVEL_CLEARED), make(EventKind::GAME_WON)};
    CHECK(haptic_for_events(win) == HapticId::WIN);

    // A wrong answer that drops the timer into the alert zone: the wrong answer is the child's tap, so it is felt.
    const std::array wrong_into_alert = {make(EventKind::ANSWER_WRONG), make(EventKind::ALERT_ENTERED)};
    CHECK(haptic_for_events(wrong_into_alert) == HapticId::WRONG);

    // A streak step and the question after it.
    Event streak = make(EventKind::ANSWER_CORRECT);
    streak.multiplier_rose = true;
    const std::array rose = {streak, make(EventKind::QUESTION_SHOWN)};
    CHECK(haptic_for_events(rose) == HapticId::MULTIPLIER_UP);

    // The bomb, with or without the answer that set it off.
    const std::array boom = {make(EventKind::ANSWER_WRONG), game_over(GameOverReason::BOMB)};
    CHECK(haptic_for_events(boom) == HapticId::BOOM);

    // Nothing felt: the events of a level start.
    const std::array quiet = {make(EventKind::LEVEL_STARTED), make(EventKind::QUESTION_SHOWN)};
    CHECK_FALSE(haptic_for_events(quiet).has_value());
}

// Beats and priorities

TEST_CASE("Every haptic is one beat of its own, except the win, which is three small ones", "[unit][haptics]")
{
    for (const HapticId id : ALL_IDS)
    {
        DYNAMIC_SECTION("id " << static_cast<int>(id))
        {
            const auto beats = haptic_beats(id);
            REQUIRE_FALSE(beats.empty());
            CHECK(beats.front().delay_ms == 0);
            if (id == HapticId::WIN)
            {
                CHECK(beats.size() == 3);
            }
            else
            {
                REQUIRE(beats.size() == 1);
                CHECK(beats.front().id == id);
            }
            // A beat is never the win itself, so the service never hands a backend a run to split.
            for (const HapticBeat& beat : beats)
            {
                CHECK(beat.id != HapticId::WIN);
            }
            for (std::size_t i = 1; i < beats.size(); ++i)
            {
                CHECK(beats[i].delay_ms - beats[i - 1].delay_ms >= HAPTIC_MIN_GAP_MS);
            }
        }
    }
}

TEST_CASE("Priorities are all different, and the end of the game is the strongest", "[unit][haptics]")
{
    std::set<int> seen;
    for (const HapticId id : ALL_IDS)
    {
        CHECK(seen.insert(haptic_priority(id)).second);
    }
    CHECK(haptic_priority(HapticId::BOOM) > haptic_priority(HapticId::WIN));
    CHECK(haptic_priority(HapticId::WIN) > haptic_priority(HapticId::LEVEL_CLEAR));
    CHECK(haptic_priority(HapticId::LEVEL_CLEAR) > haptic_priority(HapticId::WRONG));
    CHECK(haptic_priority(HapticId::WRONG) > haptic_priority(HapticId::ALERT));
    CHECK(haptic_priority(HapticId::TAP) < haptic_priority(HapticId::HINT));
}

// The rules of the service

TEST_CASE("A haptic is made at once when nothing came before it", "[unit][haptics]")
{
    HapticRig rig;
    CHECK(rig.service.available());
    rig.service.play(HapticId::RIGHT);
    CHECK(rig.backend.fired == std::vector<FiredBeat>{{HapticId::RIGHT, 0}});
    CHECK_FALSE(rig.timer.armed());
}

TEST_CASE("A device that cannot make a haptic makes none", "[unit][haptics]")
{
    HapticRig rig(false);
    CHECK_FALSE(rig.service.available());
    rig.service.play(HapticId::RIGHT);
    rig.service.play(HapticId::TAP);
    rig.advance_ms(500);
    CHECK(rig.backend.fired.empty());
}

TEST_CASE("Nothing is made while the switch is off", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.set_enabled(false);
    rig.service.play(HapticId::RIGHT);
    rig.service.play(HapticId::BOOM);
    rig.advance_ms(500);
    rig.service.play(HapticId::TAP);
    CHECK(rig.backend.fired.empty());

    rig.service.set_enabled(true);
    rig.service.play(HapticId::RIGHT);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT});
}

TEST_CASE("Turning the switch off stops a win half way", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::WIN);
    REQUIRE(rig.backend.fired.size() == 1);
    REQUIRE(rig.timer.armed());
    rig.service.set_enabled(false);
    CHECK_FALSE(rig.timer.armed());
    CHECK(rig.backend.cancels == 1);
    rig.advance_ms(1000);
    CHECK(rig.backend.fired.size() == 1);
}

TEST_CASE("Nothing is made while suspended, and what was waiting is dropped", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::RIGHT);
    rig.advance_ms(10);
    rig.service.play(HapticId::WRONG); // inside the gap: waits
    REQUIRE(rig.timer.armed());

    rig.service.suspend();
    CHECK_FALSE(rig.timer.armed());
    CHECK(rig.backend.cancels == 1);
    rig.advance_ms(500);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT}); // the waiting wrong answer never came

    rig.service.play(HapticId::TAP);
    rig.service.play(HapticId::BOOM);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT});

    rig.service.resume();
    rig.service.play(HapticId::BOOM);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT, HapticId::BOOM});
}

TEST_CASE("Two haptics never come closer than the gap: the second waits", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::RIGHT);
    rig.advance_ms(10);
    rig.service.play(HapticId::WRONG);
    CHECK(rig.backend.fired.size() == 1);
    REQUIRE(rig.timer.armed());
    CHECK(rig.timer.due_ms() == HAPTIC_MIN_GAP_MS);

    rig.advance_ms(HAPTIC_MIN_GAP_MS - 10 - 1);
    CHECK(rig.backend.fired.size() == 1);
    rig.advance_ms(1);
    CHECK(rig.backend.fired == std::vector<FiredBeat>{{HapticId::RIGHT, 0}, {HapticId::WRONG, HAPTIC_MIN_GAP_MS}});
}

TEST_CASE("A stronger cue replaces one that is waiting, and a weaker or equal one is dropped", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::RIGHT);
    rig.advance_ms(5);
    rig.service.play(HapticId::HINT); // waits
    rig.advance_ms(5);
    rig.service.play(HapticId::LEVEL_CLEAR); // replaces the hint
    rig.advance_ms(5);
    rig.service.play(HapticId::ALERT);       // weaker than the level clear: dropped
    rig.service.play(HapticId::LEVEL_CLEAR); // equal: dropped
    rig.advance_ms(200);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT, HapticId::LEVEL_CLEAR});
    CHECK(rig.backend.fired[1].at_ms == HAPTIC_MIN_GAP_MS);
}

TEST_CASE("A tap is a light extra: dropped when anything is near", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::TAP);
    CHECK(rig.backend.ids() == Ids{HapticId::TAP});

    // Another tap too soon is dropped, one after the tap gap is made.
    rig.advance_ms(HAPTIC_TAP_GAP_MS - 1);
    rig.service.play(HapticId::TAP);
    CHECK(rig.backend.fired.size() == 1);
    rig.advance_ms(1);
    rig.service.play(HapticId::TAP);
    CHECK(rig.backend.fired.size() == 2);

    // A tap just after a right answer is dropped, not queued.
    rig.advance_ms(500);
    rig.service.play(HapticId::RIGHT);
    rig.advance_ms(HAPTIC_TAP_GAP_MS - 1);
    rig.service.play(HapticId::TAP);
    rig.advance_ms(500);
    CHECK(rig.backend.ids() == Ids{HapticId::TAP, HapticId::TAP, HapticId::RIGHT});
}

TEST_CASE("A tap does not delay or hide the cue that follows it", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::TAP);
    rig.advance_ms(5);
    rig.service.play(HapticId::HINT);
    rig.advance_ms(200);
    CHECK(rig.backend.fired == std::vector<FiredBeat>{{HapticId::TAP, 0}, {HapticId::HINT, HAPTIC_MIN_GAP_MS}});
}

TEST_CASE("A tap is dropped while a cue waits", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::RIGHT);
    rig.advance_ms(5);
    rig.service.play(HapticId::WRONG); // waits
    rig.service.play(HapticId::TAP);
    rig.advance_ms(500);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT, HapticId::WRONG});
}

TEST_CASE("The win is three beats, a tenth of a second apart", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::WIN);
    CHECK(rig.backend.fired.size() == 1);
    rig.advance_ms(1000);
    CHECK(rig.backend.fired ==
          std::vector<FiredBeat>{{HapticId::RIGHT, 0}, {HapticId::MULTIPLIER_UP, 110}, {HapticId::LEVEL_CLEAR, 220}});
    CHECK_FALSE(rig.timer.armed());
}

TEST_CASE("A win that follows a haptic closely waits out the gap first", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::LEVEL_CLEAR);
    rig.advance_ms(10);
    rig.service.play(HapticId::WIN);
    rig.advance_ms(1000);
    REQUIRE(rig.backend.fired.size() == 4);
    CHECK(rig.backend.fired[1] == FiredBeat{HapticId::RIGHT, HAPTIC_MIN_GAP_MS});
    CHECK(rig.backend.fired[3].at_ms == HAPTIC_MIN_GAP_MS + 220);
}

TEST_CASE("Nothing but the bomb breaks into a win", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::WIN);
    rig.advance_ms(50);
    rig.service.play(HapticId::TAP);
    rig.service.play(HapticId::HINT);
    rig.service.play(HapticId::WIN);
    rig.advance_ms(20);
    CHECK(rig.backend.fired.size() == 1);
    rig.advance_ms(100);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT, HapticId::MULTIPLIER_UP});

    rig.service.play(HapticId::BOOM); // 170 ms: the beat after the last was at 110, the gap is long over
    rig.advance_ms(1000);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT, HapticId::MULTIPLIER_UP, HapticId::BOOM});
}

TEST_CASE("Pausing in the middle of a win ends it", "[unit][haptics]")
{
    HapticRig rig;
    rig.service.play(HapticId::WIN);
    rig.advance_ms(120);
    rig.service.suspend();
    rig.advance_ms(1000);
    CHECK(rig.backend.ids() == Ids{HapticId::RIGHT, HapticId::MULTIPLIER_UP});
}

TEST_CASE("Whatever is asked, no two beats come closer than the gap", "[unit][haptics]")
{
    // A deterministic scramble of cues and pauses, including suspends and switch changes.
    HapticRig rig;
    std::uint32_t state = 12345;
    const auto next = [&state](std::uint32_t bound)
    {
        state = state * 1664525U + 1013904223U;
        return (state >> 16) % bound;
    };
    for (int step = 0; step < 4000; ++step)
    {
        switch (next(10))
        {
        case 0:
            rig.service.suspend();
            break;
        case 1:
            rig.service.resume();
            break;
        case 2:
            rig.service.set_enabled(next(2) == 0);
            break;
        default:
            rig.service.play(ALL_IDS[next(static_cast<std::uint32_t>(ALL_IDS.size()))]);
            break;
        }
        rig.advance_ms(next(60));
    }
    rig.advance_ms(2000);
    REQUIRE(rig.backend.fired.size() > 100);
    for (std::size_t i = 1; i < rig.backend.fired.size(); ++i)
    {
        INFO("beat " << i);
        CHECK(rig.backend.fired[i].at_ms - rig.backend.fired[i - 1].at_ms >= HAPTIC_MIN_GAP_MS);
        CHECK(rig.backend.fired[i].id != HapticId::WIN);
    }
}

TEST_CASE("The null service and the null backend can do nothing", "[unit][haptics]")
{
    NullHapticsService service;
    CHECK_FALSE(service.available());
    service.play(HapticId::BOOM);
    service.set_enabled(true);
    service.suspend();
    service.resume();

    NullHapticBackend backend;
    CHECK_FALSE(backend.available());
    backend.fire(HapticId::BOOM);
    backend.cancel();
}

// What each platform is asked for

TEST_CASE("Android is asked for a constant its level has", "[unit][haptics]")
{
    // android.view.HapticFeedbackConstants: LONG_PRESS 0, VIRTUAL_KEY 1, KEYBOARD_TAP 3, CLOCK_TICK 4, CONFIRM 16,
    // REJECT 17. CONFIRM and REJECT came with Android 11 (level 30).
    const std::set<int> before_11 = {0, 1, 3, 4};
    const std::set<int> from_11 = {0, 1, 3, 4, 16, 17};
    for (const HapticId id : ALL_IDS)
    {
        for (int sdk = ANDROID_MIN_SDK; sdk < ANDROID_SDK_WITH_CONFIRM_REJECT; ++sdk)
        {
            CHECK(before_11.contains(android_feedback_constant(id, sdk)));
        }
        for (const int sdk : {30, 33, 36})
        {
            CHECK(from_11.contains(android_feedback_constant(id, sdk)));
        }
    }
}

TEST_CASE("Android feels right and wrong answers apart, with and without CONFIRM and REJECT", "[unit][haptics]")
{
    CHECK(android_feedback_constant(HapticId::RIGHT, 36) == 16);
    CHECK(android_feedback_constant(HapticId::WRONG, 36) == 17);
    CHECK(android_feedback_constant(HapticId::RIGHT, 28) == 3);
    CHECK(android_feedback_constant(HapticId::WRONG, 28) == 0);
    CHECK(android_feedback_constant(HapticId::TAP, 28) == 4);
    CHECK(android_feedback_constant(HapticId::BOOM, 36) == 0);
    for (const int sdk : {28, 30, 36})
    {
        CHECK(android_feedback_constant(HapticId::RIGHT, sdk) != android_feedback_constant(HapticId::WRONG, sdk));
        CHECK(android_feedback_constant(HapticId::TAP, sdk) != android_feedback_constant(HapticId::BOOM, sdk));
    }
}

TEST_CASE("iOS is asked for the generators of the table", "[unit][haptics]")
{
    CHECK(ios_feedback(HapticId::TAP) == IosFeedback::SELECTION);
    CHECK(ios_feedback(HapticId::RIGHT) == IosFeedback::IMPACT_RIGID);
    CHECK(ios_feedback(HapticId::WRONG) == IosFeedback::NOTIFY_ERROR);
    CHECK(ios_feedback(HapticId::HINT) == IosFeedback::IMPACT_SOFT);
    CHECK(ios_feedback(HapticId::MULTIPLIER_UP) == IosFeedback::IMPACT_MEDIUM);
    CHECK(ios_feedback(HapticId::ALERT) == IosFeedback::NOTIFY_WARNING);
    CHECK(ios_feedback(HapticId::LEVEL_CLEAR) == IosFeedback::NOTIFY_SUCCESS);
    CHECK(ios_feedback(HapticId::WIN) == IosFeedback::NOTIFY_SUCCESS);
    CHECK(ios_feedback(HapticId::BOOM) == IosFeedback::IMPACT_HEAVY);
}

TEST_CASE("The web is asked for short patterns that navigator.vibrate takes", "[unit][haptics]")
{
    for (const HapticId id : ALL_IDS)
    {
        DYNAMIC_SECTION("id " << static_cast<int>(id))
        {
            const VibrationPattern p = web_vibration(id);
            CHECK(p[0] > 0);
            // The page builds [p0], [p0, p1] or [p0, p1, p2]: a zero ends the list, so nothing follows one.
            if (p[1] == 0)
            {
                CHECK(p[2] == 0);
            }
            // Short: a child's game, nothing that buzzes the hand.
            CHECK(p[0] + p[1] + p[2] <= 200);
        }
    }
    CHECK(web_vibration(HapticId::TAP)[0] < web_vibration(HapticId::BOOM)[0]);
}
