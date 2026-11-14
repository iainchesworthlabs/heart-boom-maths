// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The profiling marks, compiled out: CMake puts this directory on the include path when the build has no Tracy (the
// default, and every sanitizer, coverage, fuzz, web, Android and iOS build). Every mark is empty, so the build has no
// Tracy code, no listener and no cost, and does not need Tracy installed. Its sibling in profile-on has the same names
// over Tracy. The arguments of a mark are not evaluated: keep them free of side effects, and never make a mark the only
// use of a variable.

namespace heart_boom_maths::profile
{

/// False here, true in the profile-on sibling. A test pins it against the CMake option.
inline constexpr bool COMPILED_IN = false;

/// Always false: nothing can be started.
[[nodiscard]] constexpr bool active() noexcept
{
    return false;
}

/// Always zero.
[[nodiscard]] constexpr int start_count() noexcept
{
    return 0;
}

/// There is no profiler to start or stop, so a program written for both builds calls these and nothing happens.
inline void start() noexcept {}
inline void stop() noexcept {}

} // namespace heart_boom_maths::profile

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define HBM_ZONE(name)
#define HBM_ZONE_FN()
#define HBM_ZONE_VALUE(number)
#define HBM_PLOT(name, value)
#define HBM_MESSAGE(text)
#define HBM_MESSAGE_DYNAMIC(text)
#define HBM_FRAME_MARK(name)
#define HBM_THREAD_NAME(name)
#define HBM_INFO(text)
// NOLINTEND(cppcoreguidelines-macro-usage)
