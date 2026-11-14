// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiling.hpp"

#include <thread>

namespace heart_boom_maths::game::profiling
{

Session::Session(const StartupDecision& decision) noexcept
{
    if (!decision.enabled || profile::active())
    {
        return;
    }
    profile::start();
    m_Running = profile::active();
    HBM_THREAD_NAME("main");
}

Session::~Session()
{
    if (m_Running)
    {
        profile::stop();
    }
}

bool Session::wait_for_profiler(std::chrono::milliseconds timeout) const
{
    if (!m_Running)
    {
        return false;
    }
    using Clock = std::chrono::steady_clock;
    const Clock::time_point deadline = Clock::now() + timeout;
    while (!profile::connected())
    {
        if (Clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return true;
}

} // namespace heart_boom_maths::game::profiling
