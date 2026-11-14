// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the application adds to the profiling marks, compiled out. CMake picks this directory (src/profiling/null) when
// the build has no Tracy; src/profiling/tracy has the same names over Tracy. A build that is not profiled has no Tracy
// code, no listener and no cost, and does not need Tracy installed.

#include "heart_boom_maths/profile.hpp"
#include "profiling/profile_switch.hpp"

#include <chrono>
#include <span>
#include <string_view>

class QQuickWindow;

namespace heart_boom_maths::game
{
class GameController;
}

namespace heart_boom_maths::game::profiling
{

inline constexpr bool ENABLED = profile::COMPILED_IN;

class Session
{
public:
    explicit Session(const StartupDecision& /*decision*/) noexcept {}

    [[nodiscard]] static constexpr bool running() noexcept
    {
        return false;
    }

    [[nodiscard]] static bool wait_for_profiler(std::chrono::milliseconds /*timeout*/)
    {
        return false;
    }
};

inline void note_main_started() noexcept {}

inline void prepare_scene_graph_log(std::span<const std::string_view> /*arguments*/) {}

inline void profile_window(QQuickWindow& /*window*/) {}

[[nodiscard]] inline bool start_profile_run(GameController& /*controller*/, QQuickWindow& /*window*/,
                                            std::span<const std::string_view> /*arguments*/)
{
    return false;
}

} // namespace heart_boom_maths::game::profiling
