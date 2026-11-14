// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// The parental gate asks for a number written in words, which an adult types back as digits. These bound the number:
/// three digits, so the words are long enough that a child who can only count cannot copy them across.
inline constexpr int GATE_NUMBER_MIN = 100;
inline constexpr int GATE_NUMBER_MAX = 999;

/// How many digits the number has. The keypad stops here, and a wrong number of this length is a wrong answer at once.
inline constexpr int GATE_NUMBER_DIGITS = 3;
static_assert(GATE_NUMBER_MIN == 100 && GATE_NUMBER_MAX == 999 && GATE_NUMBER_DIGITS == 3,
              "every number has three digits");

/// Longest answer read. Anything longer cannot be a valid answer.
inline constexpr std::size_t GATE_ANSWER_MAX_LENGTH = 8;

struct GateChallenge
{
    int number = 0;    // GATE_NUMBER_MIN to GATE_NUMBER_MAX
    std::string words; // "four hundred and seventy-two"
};

/// English words for 0 to 999.
[[nodiscard]] std::string number_in_words(int number);

/// The same seed always gives the same challenge, so tests are stable. It uses its own tiny generator (SplitMix64) and
/// nothing from the game core, whose random numbers belong to the game.
[[nodiscard]] GateChallenge make_gate_challenge(std::uint64_t seed);

/// True when the typed text is the challenge's number. Spaces around the digits and leading zeros are accepted, and
/// anything that is not a plain run of digits is not.
[[nodiscard]] bool gate_answer_matches(const GateChallenge& challenge, std::string_view typed) noexcept;

} // namespace heart_boom_maths::game
