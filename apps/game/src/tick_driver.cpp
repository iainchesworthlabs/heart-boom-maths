// SPDX-License-Identifier: GPL-3.0-or-later
#include "tick_driver.hpp"

#include <algorithm>

namespace heart_boom_maths::game
{

TickDriver::TickDriver(int ticks_per_second, int max_catch_up_ticks) noexcept
    : m_TicksPerSecond(std::max(ticks_per_second, 1)), m_MaxCatchUp(std::max(max_catch_up_ticks, 1))
{
}

void TickDriver::start(std::int64_t now_ns) noexcept
{
    m_Running = true;
    m_LastNs = now_ns;
    m_Carry = 0;
}

void TickDriver::stop() noexcept
{
    m_Running = false;
    m_Carry = 0;
}

int TickDriver::ticks_due(std::int64_t now_ns) noexcept
{
    if (!m_Running)
    {
        return 0;
    }
    // A clock that steps back counts as no time passing.
    const std::int64_t gap = std::clamp<std::int64_t>(now_ns - m_LastNs, 0, MAX_FRAME_GAP_NS);
    m_LastNs = now_ns;
    m_Carry += gap * m_TicksPerSecond;
    const std::int64_t whole = m_Carry / NANOSECONDS_PER_SECOND;
    m_Carry %= NANOSECONDS_PER_SECOND;
    return static_cast<int>(std::min<std::int64_t>(whole, m_MaxCatchUp));
}

} // namespace heart_boom_maths::game
