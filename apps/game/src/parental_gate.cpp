// SPDX-License-Identifier: GPL-3.0-or-later
#include "parental_gate.hpp"

#include <array>

namespace heart_boom_maths::game
{

namespace
{

constexpr std::array<std::string_view, 20> BELOW_TWENTY = {
    "zero", "one",    "two",    "three",    "four",     "five",    "six",     "seven",     "eight",    "nine",
    "ten",  "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};

constexpr std::array<std::string_view, 10> TENS = {"",      "",      "twenty",  "thirty", "forty",
                                                   "fifty", "sixty", "seventy", "eighty", "ninety"};

std::string below_hundred(int number)
{
    if (number < 20)
    {
        return std::string(BELOW_TWENTY[static_cast<std::size_t>(number)]);
    }
    std::string words(TENS[static_cast<std::size_t>(number / 10)]);
    if (number % 10 != 0)
    {
        words += '-';
        words += BELOW_TWENTY[static_cast<std::size_t>(number % 10)];
    }
    return words;
}

std::uint64_t splitmix64(std::uint64_t& state) noexcept
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

} // namespace

std::string number_in_words(int number)
{
    if (number < 0 || number > 999)
    {
        return {};
    }
    if (number < 100)
    {
        return below_hundred(number);
    }
    std::string words(BELOW_TWENTY[static_cast<std::size_t>(number / 100)]);
    words += " hundred";
    if (number % 100 != 0)
    {
        words += " and ";
        words += below_hundred(number % 100);
    }
    return words;
}

GateChallenge make_gate_challenge(std::uint64_t seed)
{
    std::uint64_t state = seed;
    // The modulo bias over a 64-bit value and a range of 900 is far too small to matter for a gate.
    constexpr std::uint64_t SPAN = GATE_NUMBER_MAX - GATE_NUMBER_MIN + 1;
    const int number = GATE_NUMBER_MIN + static_cast<int>(splitmix64(state) % SPAN);
    return GateChallenge{number, number_in_words(number)};
}

bool gate_answer_matches(const GateChallenge& challenge, std::string_view typed) noexcept
{
    while (!typed.empty() && typed.front() == ' ')
    {
        typed.remove_prefix(1);
    }
    while (!typed.empty() && typed.back() == ' ')
    {
        typed.remove_suffix(1);
    }
    if (typed.empty() || typed.size() > GATE_ANSWER_MAX_LENGTH)
    {
        return false;
    }
    int value = 0;
    for (const char c : typed)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    return value == challenge.number;
}

} // namespace heart_boom_maths::game
