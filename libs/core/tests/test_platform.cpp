// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "heart_boom_maths/platform.hpp"

TEST_CASE("The platform layer reports a host name", "[platform]")
{
    CHECK_FALSE(heart_boom_maths::platform::host_name().empty());
}
