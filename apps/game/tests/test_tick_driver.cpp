// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/clock.hpp"
#include "tick_driver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using namespace heart_boom_maths::game;

namespace
{
constexpr int TICKS_PER_SECOND = 20;
constexpr std::int64_t TICK_NS = NANOSECONDS_PER_SECOND / TICKS_PER_SECOND;
} // namespace

TEST_CASE("The driver turns exact tick lengths into whole ticks", "[unit][tick_driver]")
{
    TickDriver driver(TICKS_PER_SECOND);
    driver.start(1000);
    CHECK(driver.ticks_due(1000) == 0);
    CHECK(driver.ticks_due(1000 + TICK_NS) == 1);
    CHECK(driver.ticks_due(1000 + 2 * TICK_NS) == 1);
    CHECK(driver.ticks_due(1000 + 2 * TICK_NS) == 0);
}

TEST_CASE("The driver carries the part of a tick that has not finished", "[unit][tick_driver]")
{
    TickDriver driver(TICKS_PER_SECOND);
    driver.start(0);
    // Sixteen milliseconds a frame: 60 frames last 960 ms and must give 19 ticks, then 20 after 1000 ms.
    int total = 0;
    for (int frame = 1; frame <= 60; ++frame)
    {
        total += driver.ticks_due(frame * 16'000'000LL);
    }
    CHECK(total == 19);
    total += driver.ticks_due(1'000'000'000LL);
    CHECK(total == 20);
}

TEST_CASE("A rate that does not divide a second still gives that many ticks a second", "[unit][tick_driver]")
{
    TickDriver driver(30, 100);
    driver.start(0);
    int total = 0;
    // 7 ms steps for 7 000 ms: 30 ticks a second is 33.33 ms a tick.
    for (int step = 1; step <= 1000; ++step)
    {
        total += driver.ticks_due(step * 7'000'000LL);
    }
    CHECK(total == 210);
}

TEST_CASE("A stall plays at most the catch-up limit and drops the rest", "[unit][tick_driver]")
{
    TickDriver driver(TICKS_PER_SECOND);
    driver.start(0);
    CHECK(driver.ticks_due(10 * NANOSECONDS_PER_SECOND) == MAX_CATCH_UP_TICKS);
    // The dropped time is gone: the next frame counts from the stalled one.
    CHECK(driver.ticks_due(10 * NANOSECONDS_PER_SECOND + TICK_NS) == 1);
}

TEST_CASE("The catch-up limit is a parameter", "[unit][tick_driver]")
{
    TickDriver driver(TICKS_PER_SECOND, 2);
    driver.start(0);
    CHECK(driver.ticks_due(NANOSECONDS_PER_SECOND) == 2);
}

TEST_CASE("A huge gap and a clock that steps back are safe", "[unit][tick_driver]")
{
    TickDriver driver(TICKS_PER_SECOND);
    driver.start(0);
    CHECK(driver.ticks_due(INT64_MAX / 2) == MAX_CATCH_UP_TICKS);
    CHECK(driver.ticks_due(0) == 0); // went back: no time passed
    CHECK(driver.ticks_due(TICK_NS) == 1);
}

TEST_CASE("A stopped driver gives no ticks, and start forgets the time in between", "[unit][tick_driver]")
{
    TickDriver driver(TICKS_PER_SECOND);
    CHECK_FALSE(driver.running());
    CHECK(driver.ticks_due(NANOSECONDS_PER_SECOND) == 0);

    driver.start(0);
    CHECK(driver.running());
    CHECK(driver.ticks_due(TICK_NS / 2) == 0);
    driver.stop();
    CHECK_FALSE(driver.running());
    CHECK(driver.ticks_due(5 * NANOSECONDS_PER_SECOND) == 0);

    driver.start(5 * NANOSECONDS_PER_SECOND); // the half tick before the stop is not carried over
    CHECK(driver.ticks_due(5 * NANOSECONDS_PER_SECOND + TICK_NS / 2) == 0);
}

TEST_CASE("Nonsense settings are raised to the smallest working ones", "[unit][tick_driver]")
{
    TickDriver driver(0, 0);
    driver.start(0);
    CHECK(driver.ticks_due(3 * NANOSECONDS_PER_SECOND) == 1); // one tick a second, at most one a frame
}

TEST_CASE("The manual clock moves only when told", "[unit][tick_driver]")
{
    ManualClock clock;
    CHECK(clock.now_ns() == 0);
    clock.advance_ms(50);
    clock.advance_ns(5);
    CHECK(clock.now_ns() == 50'000'005);
}

TEST_CASE("The steady clock never goes back", "[unit][tick_driver]")
{
    SteadyClock clock;
    const std::int64_t first = clock.now_ns();
    CHECK(clock.now_ns() >= first);
}
