// SPDX-License-Identifier: GPL-3.0-or-later
#include "level_text.hpp"

#include <algorithm>
#include <cstddef>

namespace heart_boom_maths::game
{

namespace
{

std::string operation_words(Operations operations)
{
    switch (operations)
    {
    case Operations::ADD:
        return "add";
    case Operations::TAKE_AWAY:
        return "take away";
    case Operations::MIXED:
        return "add and take away";
    }
    return "add";
}

} // namespace

std::string level_blurb(const Tuning& tuning, int level)
{
    if (level > tuning.campaign_levels)
    {
        return "endless, with less time and harder sums";
    }
    const LevelSpec& spec = tuning.levels[static_cast<std::size_t>(std::max(level, 1) - 1)];
    switch (spec.shape)
    {
    case QuestionShape::TWO_TERM:
        return operation_words(spec.operations) + ", up to " + std::to_string(spec.largest_number);
    case QuestionShape::MISSING_ADDEND:
        return "find the missing number, up to " + std::to_string(spec.largest_number);
    case QuestionShape::THREE_TERM:
        return operation_words(spec.operations) + " three numbers";
    case QuestionShape::FOUR_TERM:
        return operation_words(spec.operations) + " four numbers";
    }
    return {};
}

std::string next_up_text(const Tuning& tuning, int next_level)
{
    return "Next up: level " + std::to_string(next_level) + ", " + level_blurb(tuning, next_level);
}

} // namespace heart_boom_maths::game
