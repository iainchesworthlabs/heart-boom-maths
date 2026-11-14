// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the application adds to the profiling marks of heart_boom_maths/profile.hpp when the build has Tracy: the
// Session that starts the client, the scene graph's zones and the scripted run. CMake picks this directory
// (src/profiling/tracy) when the build has Tracy and src/profiling/null otherwise; both expose this header and the same
// names, so no source asks which one it got.

#include "heart_boom_maths/profile.hpp"
#include "profiling/profile_switch.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
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

namespace detail
{

inline std::atomic<std::int64_t> g_MainStartedNs{0}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace detail

/// Marks where the start-up clock begins: the first thing main() does. The time to the first presented frame is
/// measured from here.
inline void note_main_started() noexcept
{
    detail::g_MainStartedNs.store(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count(),
        std::memory_order_relaxed);
}

/// Milliseconds since note_main_started(), or below zero if it was never called.
[[nodiscard]] inline double ms_since_main_started() noexcept
{
    const std::int64_t started = detail::g_MainStartedNs.load(std::memory_order_relaxed);
    if (started == 0)
    {
        return -1.0;
    }
    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    return static_cast<double>(now - started) / 1e6;
}

/// Holds the profiler for the life of the process: starts Tracy if the decision is to, and at the end stops the client.
/// Declare it first in main(), so it goes last, after every worker thread has finished. A build nobody asks to profile
/// constructs one that does nothing, and Tracy's client never exists.
class Session
{
public:
    explicit Session(const StartupDecision& decision) noexcept;
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    [[nodiscard]] bool running() const noexcept
    {
        return m_Running;
    }

    /// Blocks until a profiler has connected or `timeout` has passed, so a person is never left with a window that does
    /// not open. Returns whether one connected; false at once when the profiler is not running.
    [[nodiscard]] bool wait_for_profiler(std::chrono::milliseconds timeout) const;

private:
    bool m_Running = false;
};

/// Reads the program's arguments (--profile-sg-stats) and, when asked, switches the scene graph's logging on and takes
/// it over: its lines become plots and counts for the profiler and stay off the console. Also asks the RHI for GPU
/// timestamps (QSG_RHI_PROFILE=1) so that the GPU frame time can be plotted. Run it after the profiler has started and
/// before the first window exists; it does nothing unless the profiler runs.
void prepare_scene_graph_log(std::span<const std::string_view> arguments);

/// Hangs Tracy zones on the scene graph of `window`: the phases of every frame, a frame mark and the frame time, and,
/// where the graphics backend allows it, the GPU's side. Call it once for a window, only while the profiler runs.
void profile_window(QQuickWindow& window);

/// Starts the scripted run if the command line asks for one (--profile-run <seconds>, --profile-summary <file>): games
/// are played through the controller and the window for a fixed time, the program quits when they are done, and the
/// statistics of each phase go to the summary file. Returns whether a run was started. Needs profile_window() first.
[[nodiscard]] bool start_profile_run(GameController& controller, QQuickWindow& window,
                                     std::span<const std::string_view> arguments);

} // namespace heart_boom_maths::game::profiling
