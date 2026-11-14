// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/tuning.hpp"
#include "prd_tables.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

// Tuning::defaults() is built from libs/core/data/tuning.json, and the PRD's own numbers are in
// libs/core/tests/data/prd-tables.json. These tests fail until the two files agree, so a change to one has to
// bring the other with it. Do not edit prd-tables.json to make a test pass: change the PRD first.

using namespace heart_boom_maths;

TEST_CASE("Tuning defaults match the PRD's scalar table", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    CHECK(t.level_seconds == prd::PRD_DEFAULTS_LEVEL_SECONDS);
    CHECK(t.alert_seconds == prd::PRD_DEFAULTS_ALERT_SECONDS);
    CHECK(t.right_answer_seconds == prd::PRD_DEFAULTS_RIGHT_ANSWER_SECONDS);
    CHECK(t.wrong_answer_seconds == prd::PRD_DEFAULTS_WRONG_ANSWER_SECONDS);
    CHECK(t.questions_per_level == prd::PRD_DEFAULTS_QUESTIONS_PER_LEVEL);
    CHECK(t.answer_tiles == prd::PRD_DEFAULTS_ANSWER_TILES);
    CHECK(t.base_points == prd::PRD_DEFAULTS_BASE_POINTS);
    CHECK(t.time_bonus_points_per_second == prd::PRD_DEFAULTS_TIME_BONUS_POINTS_PER_SECOND);
    CHECK(t.hint_cost_coins == prd::PRD_DEFAULTS_HINT_COST_COINS);
    CHECK(t.top10_size == prd::PRD_DEFAULTS_TOP10_SIZE);
    CHECK(t.name_max_length == prd::PRD_DEFAULTS_NAME_MAX_LENGTH);
}

TEST_CASE("Tuning defaults match the PRD's scoring and streak rules", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    CHECK(t.perfect_level_bonus_points == prd::PRD_SCORING_PERFECT_LEVEL_BONUS_POINTS);
    CHECK(t.perfect_level_coins == prd::PRD_SCORING_PERFECT_LEVEL_COINS);
    CHECK(t.time_bonus_points_per_second == prd::PRD_SCORING_TIME_LEFT_POINTS_PER_SECOND);
    CHECK(t.win_bonus_coins == prd::PRD_SCORING_WIN_BONUS_COINS_PAID_ONCE);
    CHECK(t.hint_cost_coins == prd::PRD_SCORING_HINT_COST_COINS);
    CHECK(t.streak_bonus_coin_every == 5); // "every 5th correct in a row"; the PRD key carries the 5 in its name

    CHECK(t.multiplier_steps[0].run == prd::PRD_STREAK_RUN_FOR_X2);
    CHECK(t.multiplier_steps[0].step == 2);
    CHECK(t.multiplier_steps[1].run == prd::PRD_STREAK_RUN_FOR_X3);
    CHECK(t.multiplier_steps[1].step == 3);
    CHECK(t.multiplier_steps.back().step == prd::PRD_STREAK_CAP);
}

TEST_CASE("Tuning defaults match the PRD's level table, row by row", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    REQUIRE(t.campaign_levels == static_cast<int>(prd::PRD_LEVELS.size()));
    REQUIRE(t.levels.size() == prd::PRD_LEVELS.size());
    for (std::size_t i = 0; i < prd::PRD_LEVELS.size(); ++i)
    {
        const prd::PrdLevel& row = prd::PRD_LEVELS[i];
        INFO("level " << row.level);
        CHECK(t.levels[i].level == row.level);
        CHECK(t.levels[i].shape == row.shape);
        CHECK(t.levels[i].operations == row.operations);
        CHECK(t.levels[i].largest_number == row.largest_number);
    }
}

TEST_CASE("The endless functions reproduce the PRD's anchor rows", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    for (const prd::PrdEndlessRow& row : prd::PRD_ENDLESS_ROWS)
    {
        INFO("level " << row.level);
        const int k = row.level - t.endless.first_level + 1;
        const int level_seconds = endless_level_seconds(t.endless, k);
        CHECK(level_seconds == row.level_seconds);
        CHECK(endless_bonus_seconds(t.endless, k) == row.bonus_seconds);
        CHECK(endless_multi_term_percent(t.endless, k) == row.multi_term_percent);
        CHECK(alert_share_percent(level_seconds, t.endless.alert_seconds) == row.alert_share_percent);
    }
}

TEST_CASE("Endless mode starts straight after the campaign and keeps its range", "[tuning]")
{
    const Tuning& t = Tuning::defaults();

    CHECK(t.endless.first_level == t.campaign_levels + 1);
    CHECK(t.endless.largest_number == t.levels.back().largest_number);
    CHECK(t.endless.wrong_answer_seconds == t.wrong_answer_seconds);
    CHECK(t.endless.alert_seconds == t.alert_seconds);
    CHECK(t.endless.level_seconds_start == t.level_seconds);
    CHECK(t.endless.bonus_seconds_start == t.right_answer_seconds);
}
