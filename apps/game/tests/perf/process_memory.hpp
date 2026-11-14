// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace hbm_perf
{

/// The size of the process in memory (committed bytes on Windows, the resident set where /proc exists), or -1 when this
/// platform has no way to say. The game's performance tests use it to see that a loop does not grow the process; the
/// platform's file is chosen by CMake, so no source line depends on the platform.
[[nodiscard]] std::int64_t process_memory_bytes();

} // namespace hbm_perf
