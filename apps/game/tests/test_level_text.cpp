// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/tuning.hpp"
#include "level_text.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;

TEST_CASE("Every campaign level has words for what it asks", "[unit][level_text]")
{
    const Tuning& t = Tuning::defaults();
    CHECK(level_blurb(t, 1) == "add, up to 5");
    CHECK(level_blurb(t, 2) == "take away, up to 5");
    CHECK(level_blurb(t, 3) == "add and take away, up to 10");
    CHECK(level_blurb(t, 5) == "find the missing number, up to 20");
    CHECK(level_blurb(t, 6) == "add three numbers");
    CHECK(level_blurb(t, 8) == "add and take away three numbers");
    CHECK(level_blurb(t, 10) == "add and take away four numbers");
    CHECK(level_blurb(t, 12) == "add and take away, up to 1000");
    for (int level = 1; level <= t.campaign_levels; ++level)
    {
        INFO(level);
        CHECK_FALSE(level_blurb(t, level).empty());
    }
}

TEST_CASE("Levels past the campaign read as endless", "[unit][level_text]")
{
    const Tuning& t = Tuning::defaults();
    CHECK(level_blurb(t, t.campaign_levels + 1) == "endless, with less time and harder sums");
    CHECK(level_blurb(t, 40) == level_blurb(t, 13));
}

TEST_CASE("The next-up line names the level and what it asks", "[unit][level_text]")
{
    const Tuning& t = Tuning::defaults();
    CHECK(next_up_text(t, 6) == "Next up: level 6, add three numbers");
    CHECK(next_up_text(t, 13) == "Next up: level 13, endless, with less time and harder sums");
}

TEST_CASE("A level number below 1 is read as level 1", "[unit][level_text]")
{
    const Tuning& t = Tuning::defaults();
    CHECK(level_blurb(t, 0) == level_blurb(t, 1));
}
