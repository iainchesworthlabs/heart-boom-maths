// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiling/profile_switch.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace heart_boom_maths::game::profiling
{

namespace
{

bool equals_ignoring_case(std::string_view text, std::string_view word) noexcept
{
    return std::ranges::equal(
        text, word, [](char a, char b)
        { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}

bool is_one_of(std::string_view text, std::span<const std::string_view> words) noexcept
{
    return std::ranges::any_of(words, [text](std::string_view word) { return equals_ignoring_case(text, word); });
}

} // namespace

StartupDecision decide_startup(bool default_on, const char* environment,
                               std::span<const std::string_view> arguments) noexcept
{
    StartupDecision decision;
    decision.enabled = default_on;

    if (environment != nullptr)
    {
        static constexpr std::array<std::string_view, 4> ON_WORDS{"1", "true", "on", "yes"};
        static constexpr std::array<std::string_view, 4> OFF_WORDS{"0", "false", "off", "no"};
        const std::string_view value{environment};
        if (equals_ignoring_case(value, "wait"))
        {
            decision = {true, SwitchSource::ENVIRONMENT, true};
        }
        else if (is_one_of(value, ON_WORDS))
        {
            decision = {true, SwitchSource::ENVIRONMENT, false};
        }
        else if (is_one_of(value, OFF_WORDS))
        {
            decision = {false, SwitchSource::ENVIRONMENT, false};
        }
    }

    for (const std::string_view argument : arguments)
    {
        if (argument == WAIT_FLAG)
        {
            decision = {true, SwitchSource::COMMAND_LINE, true};
        }
        else if (argument == ENABLE_FLAG)
        {
            decision = {true, SwitchSource::COMMAND_LINE, false};
        }
        else if (argument == DISABLE_FLAG)
        {
            decision = {false, SwitchSource::COMMAND_LINE, false};
        }
    }
    return decision;
}

} // namespace heart_boom_maths::game::profiling
