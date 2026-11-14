// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstdint>

namespace heart_boom_maths::game
{

/// A monotonic clock in whole nanoseconds. The app layer is the only layer that may hold a clock, and it reads it
/// through this interface so a test can drive time by hand.
class Clock
{
public:
    virtual ~Clock() = default;

    [[nodiscard]] virtual std::int64_t now_ns() const = 0;
};

class SteadyClock final : public Clock
{
public:
    [[nodiscard]] std::int64_t now_ns() const override
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};

/// Time that moves only when told to, for tests.
class ManualClock final : public Clock
{
public:
    [[nodiscard]] std::int64_t now_ns() const override
    {
        return m_NowNs;
    }

    void advance_ns(std::int64_t nanoseconds) noexcept
    {
        m_NowNs += nanoseconds;
    }

    void advance_ms(std::int64_t milliseconds) noexcept
    {
        m_NowNs += milliseconds * 1'000'000;
    }

private:
    std::int64_t m_NowNs = 0;
};

} // namespace heart_boom_maths::game
