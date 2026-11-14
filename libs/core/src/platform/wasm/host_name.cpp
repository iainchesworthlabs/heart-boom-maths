// SPDX-License-Identifier: GPL-3.0-or-later
#include <string>

#include "heart_boom_maths/platform.hpp"

namespace heart_boom_maths::platform
{

// A browser has no host name to report.
std::string host_name()
{
    return "browser";
}

}  // namespace heart_boom_maths::platform
