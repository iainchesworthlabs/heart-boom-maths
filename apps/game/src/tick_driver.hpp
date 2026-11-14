// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace heart_boom_maths::game
{

inline constexpr std::int64_t NANOSECONDS_PER_SECOND = 1'000'000'000;

/// After a stall (a window drag, a debugger, a slow frame) at most this many ticks are played in one go. The rest of
/// the stall is dropped, so a child never loses a level to a hiccup in the app. Four ticks is a fifth of a second at
/// 20 ticks a second.
inline constexpr int MAX_CATCH_UP_TICKS = 4;

/// A gap longer than this is read as this long, which keeps the integer arithmetic far from overflowing.
inline constexpr std::int64_t MAX_FRAME_GAP_NS = 3600 * NANOSECONDS_PER_SECOND;

/// Turns clock readings into whole ticks with integer arithmetic only. The remainder of each frame is carried in units
/// of one tick-nanosecond (a nanosecond times the ticks per second), so a rate that does not divide a second still
/// yields exactly that many ticks over any whole number of seconds.
class TickDriver
{
public:
    explicit TickDriver(int ticks_per_second, int max_catch_up_ticks = MAX_CATCH_UP_TICKS) noexcept;

    /// Begins counting from `now_ns`. Time before this call is not counted, and a leftover fraction is dropped.
    void start(std::int64_t now_ns) noexcept;
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept
    {
        return m_Running;
    }

    /// The whole ticks to play for the time since the last call (or start). Zero when stopped.
    [[nodiscard]] int ticks_due(std::int64_t now_ns) noexcept;

private:
    int m_TicksPerSecond;
    int m_MaxCatchUp;
    bool m_Running = false;
    std::int64_t m_LastNs = 0;
    std::int64_t m_Carry = 0; // tick-nanoseconds, always below NANOSECONDS_PER_SECOND between calls
};

} // namespace heart_boom_maths::game
