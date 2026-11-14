// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/rng.hpp"

namespace heart_boom_maths
{

Rng::Rng(std::uint64_t seed) noexcept : m_State(seed) {}

std::uint64_t Rng::next_u64() noexcept
{
    m_State += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = m_State;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t Rng::below(std::uint64_t bound) noexcept
{
    // Reject the short first slice of the 64-bit range so every residue is equally likely.
    const std::uint64_t threshold = (0 - bound) % bound;
    for (;;)
    {
        const std::uint64_t r = next_u64();
        if (r >= threshold)
        {
            return r % bound;
        }
    }
}

int Rng::between(int low, int high) noexcept
{
    const auto span = static_cast<std::uint64_t>(static_cast<std::int64_t>(high) - low) + 1;
    return static_cast<int>(static_cast<std::int64_t>(low) + static_cast<std::int64_t>(below(span)));
}

} // namespace heart_boom_maths
