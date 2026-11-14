// SPDX-License-Identifier: GPL-3.0-or-later
// The Windows answer to process_memory_bytes(): the bytes the process has committed. CMake picks this file on
// Windows and process_memory_proc.cpp elsewhere, so no source line depends on the platform.
#include "process_memory.hpp"

// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

namespace hbm_perf
{

std::int64_t process_memory_bytes()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof counters) == 0)
    {
        return -1;
    }
    return static_cast<std::int64_t>(counters.PrivateUsage);
}

} // namespace hbm_perf
