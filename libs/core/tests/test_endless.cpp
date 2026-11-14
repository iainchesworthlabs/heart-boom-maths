// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/tuning.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

using namespace heart_boom_maths;

TEST_CASE("Time is counted in whole ticks", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    CHECK(t.ticks_per_second == 20);
    CHECK(t.ticks(t.level_seconds) == 1200);
    CHECK(t.ticks(t.alert_seconds) == 200);
    CHECK(t.ticks(t.right_answer_seconds) == 140);
    CHECK(t.ticks(t.wrong_answer_seconds) == 60);
}

TEST_CASE("The campaign table is well formed", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    CHECK(t.operand_minimum >= 1);
    CHECK(t.alert_seconds < t.level_seconds);
    CHECK(t.hint_tiles_removed < t.answer_tiles - 1);

    int previous_largest = 0;
    for (std::size_t i = 0; i < t.levels.size(); ++i)
    {
        INFO("row " << i);
        CHECK(t.levels[i].level == static_cast<int>(i) + 1);
        CHECK(t.levels[i].largest_number >= previous_largest);
        CHECK(t.levels[i].largest_number > t.operand_minimum);
        previous_largest = t.levels[i].largest_number;
    }
}

TEST_CASE("Endless levels never get easier and always end", "[tuning]")
{
    const EndlessTuning& e = Tuning::defaults().endless;

    int previous_seconds = endless_level_seconds(e, 1);
    int previous_bonus = endless_bonus_seconds(e, 1);
    int previous_percent = endless_multi_term_percent(e, 1);
    for (int k = 2; k <= 200; ++k)
    {
        INFO("k " << k);
        const int seconds = endless_level_seconds(e, k);
        const int bonus = endless_bonus_seconds(e, k);
        const int percent = endless_multi_term_percent(e, k);

        CHECK(seconds <= previous_seconds);
        CHECK(bonus <= previous_bonus);
        CHECK(percent >= previous_percent);

        CHECK(seconds >= e.level_seconds_floor);
        CHECK(bonus >= e.bonus_seconds_floor);
        CHECK(percent <= e.multi_term_percent_end);

        previous_seconds = seconds;
        previous_bonus = bonus;
        previous_percent = percent;
    }

    // At the floor a right answer adds no more than a wrong one removes, so every game ends.
    CHECK(endless_bonus_seconds(e, 200) <= e.wrong_answer_seconds);
}

TEST_CASE("The alert share rounds to the nearest whole percent", "[tuning]")
{
    CHECK(alert_share_percent(60, 10) == 17);
    CHECK(alert_share_percent(30, 10) == 33);
    CHECK(alert_share_percent(44, 10) == 23);
    CHECK(alert_share_percent(10, 10) == 100);
}
