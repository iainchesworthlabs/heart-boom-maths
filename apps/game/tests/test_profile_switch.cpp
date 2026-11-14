// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_build.hpp"
#include "profiling.hpp"
#include "profiling/profile_switch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string_view>
#include <vector>

using namespace heart_boom_maths::game::profiling;

namespace
{

StartupDecision decide(bool default_on, const char* environment, std::vector<std::string_view> arguments = {})
{
    return decide_startup(default_on, environment, arguments);
}

} // namespace

TEST_CASE("The profiler follows the build's default when nothing is said", "[unit][profiling]")
{
    CHECK_FALSE(decide(false, nullptr).enabled);
    CHECK(decide(true, nullptr).enabled);
    CHECK(decide(true, nullptr).source == SwitchSource::BUILD_DEFAULT);
    CHECK_FALSE(decide(false, nullptr).wait);
}

TEST_CASE("The command line starts the profiler or stops it, and the last flag wins", "[unit][profiling]")
{
    const StartupDecision on = decide(false, nullptr, {"--profile"});
    CHECK(on.enabled);
    CHECK(on.source == SwitchSource::COMMAND_LINE);
    CHECK_FALSE(on.wait);

    CHECK_FALSE(decide(true, nullptr, {"--no-profile"}).enabled);
    CHECK(decide(true, nullptr, {"--no-profile", "--profile"}).enabled);
    CHECK_FALSE(decide(false, nullptr, {"--profile", "--no-profile"}).enabled);

    const StartupDecision wait = decide(false, nullptr, {"--profile-wait"});
    CHECK(wait.enabled);
    CHECK(wait.wait);
}

TEST_CASE("A scripted run does not start the profiler by itself", "[unit][profiling]")
{
    CHECK_FALSE(decide(false, nullptr, {"--profile-run", "90"}).enabled);
}

TEST_CASE("The environment starts the profiler with a word, in any case", "[unit][profiling]")
{
    for (const char* word : {"1", "true", "TRUE", "On", "yes"})
    {
        INFO(word);
        const StartupDecision decision = decide(false, word);
        CHECK(decision.enabled);
        CHECK(decision.source == SwitchSource::ENVIRONMENT);
        CHECK_FALSE(decision.wait);
    }
    for (const char* word : {"0", "false", "Off", "NO"})
    {
        INFO(word);
        CHECK_FALSE(decide(true, word).enabled);
    }
    const StartupDecision wait = decide(false, "wait");
    CHECK(wait.enabled);
    CHECK(wait.wait);
}

TEST_CASE("A word the environment does not mean changes nothing, so a typo never opens or closes a profiler",
          "[unit][profiling]")
{
    for (const char* word : {"", "2", "enable", "ye", "tru", "onn"})
    {
        INFO(word);
        CHECK_FALSE(decide(false, word).enabled);
        CHECK(decide(true, word).enabled);
        CHECK(decide(false, word).source == SwitchSource::BUILD_DEFAULT);
    }
}

TEST_CASE("The command line beats the environment", "[unit][profiling]")
{
    CHECK(decide(false, "0", {"--profile"}).enabled);
    CHECK_FALSE(decide(false, "1", {"--no-profile"}).enabled);
    CHECK(decide(false, "1", {"--no-profile"}).source == SwitchSource::COMMAND_LINE);
}

TEST_CASE("The marks of the app layer are real exactly when the build has Tracy", "[unit][profiling]")
{
    // CMake picks the directory of profiling.hpp; this fails if it picked the wrong one.
    CHECK(ENABLED == TRACY_BUILT);
    // A build that is not asked never starts a profiler: the marks do nothing until a Session has started one.
    CHECK_FALSE(heart_boom_maths::profile::active());
}

TEST_CASE("A session that is told not to profile starts nothing", "[unit][profiling]")
{
    const Session session(StartupDecision{});
    CHECK_FALSE(session.running());
    CHECK_FALSE(heart_boom_maths::profile::active());
    CHECK_FALSE(session.wait_for_profiler(std::chrono::milliseconds(1)));
}
