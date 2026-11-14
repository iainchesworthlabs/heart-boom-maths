// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

/// Heap counters for the performance test programs. perf_alloc.cpp replaces the global operator new and delete of the
/// program it is linked into, so a test can ask how many allocations a piece of work made and how much memory is still
/// held after it. Only the performance tests link it; the core and the game never do.
///
/// The rules library and its tests use only the standard library, so the replacement is safe there and the counters are
/// exact. The game's performance tests do not link it: Qt's libraries free memory the program allocated, and on Windows
/// they have their own operator delete, so a replacement they do not share would crash them.
namespace hbm_perf
{

struct AllocCounters
{
    std::int64_t allocations = 0; // calls to operator new since the program started
    std::int64_t live_allocations = 0;
    std::int64_t live_bytes = 0;
};

[[nodiscard]] AllocCounters alloc_counters() noexcept;

} // namespace hbm_perf
