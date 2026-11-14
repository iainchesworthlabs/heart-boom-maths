// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace heart_boom_maths;

TEST_CASE("The version string is the three numbers joined by dots", "[unit][version]")
{
    const std::string expected =
        std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH);
    CHECK(std::string(VERSION_STRING) == expected);
}
