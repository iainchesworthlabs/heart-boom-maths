// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace heart_boom_maths
{

/// The core's own random generator (SplitMix64). Its whole state is one integer, so a saved run can restore it exactly.
/// The standard distributions are not used anywhere in the core, because their results differ between standard
/// libraries and would break replays across platforms.
class Rng
{
public:
    explicit Rng(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;

    /// A value in [0, bound) with no modulo bias. `bound` must be at least 1.
    [[nodiscard]] std::uint64_t below(std::uint64_t bound) noexcept;

    /// A value in [low, high], both included. `low` must not exceed `high`.
    [[nodiscard]] int between(int low, int high) noexcept;

    /// Fisher-Yates, from the back of the range to the front.
    template <typename T>
    void shuffle(std::span<T> items) noexcept
    {
        for (std::size_t i = items.size(); i > 1; --i)
        {
            const auto j = static_cast<std::size_t>(below(i));
            std::swap(items[i - 1], items[j]);
        }
    }

    [[nodiscard]] std::uint64_t state() const noexcept
    {
        return m_State;
    }

    void set_state(std::uint64_t state) noexcept
    {
        m_State = state;
    }

private:
    std::uint64_t m_State;
};

} // namespace heart_boom_maths
