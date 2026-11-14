// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

namespace heart_boom_maths
{

/// 64-bit FNV-1a over a list of integers, each fed as eight bytes, least significant first. Replays compare these
/// hashes, so the definition never changes without regenerating every golden replay.
[[nodiscard]] constexpr std::uint64_t fnv1a_64(std::span<const std::int64_t> values) noexcept
{
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const std::int64_t value : values)
    {
        const auto bits = static_cast<std::uint64_t>(value);
        for (int shift = 0; shift < 64; shift += 8)
        {
            hash ^= (bits >> shift) & 0xFFU;
            hash *= 0x100000001B3ULL;
        }
    }
    return hash;
}

} // namespace heart_boom_maths
