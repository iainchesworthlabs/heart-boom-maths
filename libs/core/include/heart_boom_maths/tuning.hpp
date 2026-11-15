// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>

namespace heart_boom_maths
{

enum class QuestionShape
{
    TWO_TERM,       // a + b, a - b
    MISSING_ADDEND, // a + ? = c
    THREE_TERM,     // a + b + c, a + b - c
    FOUR_TERM,      // a + b + c - d
};

enum class Operations
{
    ADD,
    TAKE_AWAY,
    MIXED,
};

/// One row of the campaign table (levels 1 to 12).
struct LevelSpec
{
    int level;
    QuestionShape shape;
    Operations operations;
    int largest_number;
};

/// A run of this many correct answers in a row lifts the multiplier step to at least `step`.
struct MultiplierStep
{
    int run;
    int step;
};

/// The numbers that shape endless mode (level 13 onward). Let k be the endless level number, so level 13 is k = 1.
struct EndlessTuning
{
    int first_level;
    int largest_number;
    int level_seconds_start;
    int level_seconds_per_k;
    int level_seconds_floor;
    int bonus_seconds_start;
    int bonus_seconds_drop_every_k;
    int bonus_seconds_floor;
    int multi_term_percent_start;
    int multi_term_percent_end;
    int multi_term_percent_span_k;
    int wrong_answer_seconds;
    int alert_seconds;
};

/// Every number of the game. The members are in the order of libs/core/data/tuning.json, and the generated header
/// builds the release values from that file, so a balance change is a data change. All of it is integers: nothing here
/// is saved or replayed as a floating-point number.
struct Tuning
{
    int ticks_per_second;
    int questions_per_level;
    int answer_tiles;
    int campaign_levels;
    int level_seconds;
    int alert_seconds;
    int right_answer_seconds;
    int wrong_answer_seconds;
    bool right_answer_time_capped_at_level_time;
    int base_points;
    std::array<MultiplierStep, 2> multiplier_steps;
    int streak_bonus_coin_every;
    int time_bonus_points_per_second;
    int perfect_level_bonus_points;
    int perfect_level_coins;
    int win_bonus_coins;
    int hint_cost_coins;
    int hint_tiles_removed;
    int top10_size;
    int max_players;
    int name_max_length;
    int operand_minimum;
    std::array<LevelSpec, 12> levels;
    EndlessTuning endless;

    /// The release values, from libs/core/data/tuning.json.
    [[nodiscard]] static const Tuning& defaults() noexcept;

    /// The core counts time in whole ticks, never in a clock reading.
    [[nodiscard]] constexpr int ticks(int seconds) const noexcept
    {
        return seconds * ticks_per_second;
    }
};

/// Seconds on the timer at the start of endless level k (k >= 1).
[[nodiscard]] constexpr int endless_level_seconds(const EndlessTuning& e, int k) noexcept
{
    return std::max(e.level_seconds_start - e.level_seconds_per_k * k, e.level_seconds_floor);
}

/// Seconds a right answer adds in endless level k (k >= 1).
[[nodiscard]] constexpr int endless_bonus_seconds(const EndlessTuning& e, int k) noexcept
{
    return std::max(e.bonus_seconds_start - (k - 1) / e.bonus_seconds_drop_every_k, e.bonus_seconds_floor);
}

/// Share of three- and four-term questions in endless level k (k >= 1), in whole percent.
[[nodiscard]] constexpr int endless_multi_term_percent(const EndlessTuning& e, int k) noexcept
{
    const int span = e.multi_term_percent_end - e.multi_term_percent_start;
    return std::min(e.multi_term_percent_start + (k - 1) * span / e.multi_term_percent_span_k,
                    e.multi_term_percent_end);
}

/// The alert zone as a share of the whole fuse, rounded to the nearest whole percent.
[[nodiscard]] constexpr int alert_share_percent(int level_seconds, int alert_seconds) noexcept
{
    return (alert_seconds * 100 + level_seconds / 2) / level_seconds;
}

} // namespace heart_boom_maths
