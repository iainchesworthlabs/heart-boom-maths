// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace heart_boom_maths::game::profiling
{

/// Where the decision to profile came from, so the program can say so when it starts.
enum class SwitchSource : std::uint8_t
{
    BUILD_DEFAULT,
    ENVIRONMENT,
    COMMAND_LINE,
};

struct StartupDecision
{
    bool enabled = false;
    SwitchSource source = SwitchSource::BUILD_DEFAULT;
    /// Start, and then hold the program until a profiler has connected: Tracy records from the moment one connects, so
    /// what happens before it (start-up) is lost to a profiler attached by hand a second after launch.
    bool wait = false;

    friend bool operator==(const StartupDecision&, const StartupDecision&) = default;
};

inline constexpr std::string_view ENVIRONMENT_VARIABLE = "HBM_PROFILE";
inline constexpr std::string_view ENABLE_FLAG = "--profile";
inline constexpr std::string_view DISABLE_FLAG = "--no-profile";
inline constexpr std::string_view WAIT_FLAG = "--profile-wait";
/// A scripted run (profile_run.hpp) plays games and records frame times; it does not start the profiler by itself.
inline constexpr std::string_view RUN_FLAG = "--profile-run";

/// Whether this process starts the profiler. In order of priority:
///
///  1. the command line: --profile starts it, --no-profile does not, --profile-wait starts it and
///  waits for a profiler;
///     the last of them wins;
///  2. the environment: HBM_PROFILE is 1, true, on or yes to start, wait to start and wait, 0, false, off
///     or no not to (any case); anything else, or empty, is ignored, so a typo never opens or closes a profiler;
///  3. `default_on`, the build's own default (the profile presets turn it on; every other build has it off).
///
/// `environment` is the variable's value, or nullptr when it is unset. A pure function, so it is tested without
/// starting anything. In a build without Tracy the program never calls it.
[[nodiscard]] StartupDecision decide_startup(bool default_on, const char* environment,
                                             std::span<const std::string_view> arguments) noexcept;

} // namespace heart_boom_maths::game::profiling
