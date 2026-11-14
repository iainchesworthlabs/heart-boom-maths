// SPDX-License-Identifier: GPL-3.0-or-later
#include "golden_replays.hpp"
#include "heart_boom_maths/profile.hpp"
#include "heart_boom_maths/replay.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using namespace heart_boom_maths;

namespace
{

ReplayResult play(const GoldenReplay& golden)
{
    const auto replay = parse_replay(golden.text);
    REQUIRE(replay.has_value());
    const auto result = run_replay(*replay);
    REQUIRE(result.has_value());
    return *result;
}

std::vector<ReplayResult> play_all()
{
    std::vector<ReplayResult> results;
    for (const GoldenReplay& golden : GOLDEN_REPLAYS)
    {
        results.push_back(play(golden));
    }
    return results;
}

} // namespace

TEST_CASE("The profiler is not running unless something started it", "[unit][profile]")
{
    // Nothing in the core starts it, and no test before this one has: the client does not exist, so no thread runs and
    // no socket is open.
    CHECK_FALSE(profile::active());
    CHECK(profile::start_count() == 0);
}

TEST_CASE("The marks are real exactly when the build has Tracy", "[unit][profile]")
{
    // CMake picks the directory of profile.hpp from ICL_ENABLE_TRACY; the replays below are the same either way.
    CHECK(profile::COMPILED_IN == (HBM_TEST_EXPECT_TRACY != 0));
}

TEST_CASE("A replay gives the same events and hashes with the profiler started as with it not running",
          "[unit][profile]")
{
    // The marks only observe. Nothing in a session, a snapshot, an event or a hash reads them, so starting the profiler
    // must not change a single number (CLAUDE.md: the core's state is integers and nothing else). The golden replays
    // also run in every other test of this binary, with the marks compiled in or out as the build says.
    const std::vector<ReplayResult> before = play_all();

    if (profile::COMPILED_IN)
    {
        // Tracy's listener opens on the loopback interface, as in a profiling run.
        profile::start();
        REQUIRE(profile::active());
        CHECK(profile::start_count() == 1);
        profile::start(); // a second start does nothing
        CHECK(profile::start_count() == 1);
    }
    const std::vector<ReplayResult> during = play_all();
    if (profile::COMPILED_IN)
    {
        profile::stop();
        CHECK_FALSE(profile::active());
    }
    const std::vector<ReplayResult> after = play_all();

    REQUIRE(before.size() == during.size());
    REQUIRE(before.size() == after.size());
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        INFO(GOLDEN_REPLAYS[i].name);
        CHECK(during[i].events_hash == before[i].events_hash);
        CHECK(during[i].final_hash == before[i].final_hash);
        CHECK(during[i].score == before[i].score);
        CHECK(after[i].events_hash == before[i].events_hash);
        CHECK(after[i].final_hash == before[i].final_hash);
    }
}
