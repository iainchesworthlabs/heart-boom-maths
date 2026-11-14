// SPDX-License-Identifier: GPL-3.0-or-later
// The answer to process_memory_bytes() on a system with /proc (Linux, Android): the resident set from /proc/self/statm.
// Elsewhere, macOS for one, the file is missing and the answer is -1, which the tests read as "not measured".
#include "process_memory.hpp"

#include <cstdint>
#include <fstream>

namespace hbm_perf
{

std::int64_t process_memory_bytes()
{
    std::ifstream statm("/proc/self/statm");
    std::int64_t total_pages = 0;
    std::int64_t resident_pages = 0;
    if (!(statm >> total_pages >> resident_pages))
    {
        return -1;
    }
    constexpr std::int64_t PAGE_BYTES = 4096; // the page size of every system this file answers on
    return resident_pages * PAGE_BYTES;
}

} // namespace hbm_perf
