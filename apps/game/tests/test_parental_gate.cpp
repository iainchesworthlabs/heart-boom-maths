// SPDX-License-Identifier: GPL-3.0-or-later
#include "parental_gate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace heart_boom_maths::game;

TEST_CASE("Numbers are written in English words", "[unit][gate]")
{
    CHECK(number_in_words(0) == "zero");
    CHECK(number_in_words(7) == "seven");
    CHECK(number_in_words(13) == "thirteen");
    CHECK(number_in_words(20) == "twenty");
    CHECK(number_in_words(21) == "twenty-one");
    CHECK(number_in_words(99) == "ninety-nine");
    CHECK(number_in_words(100) == "one hundred");
    CHECK(number_in_words(101) == "one hundred and one");
    CHECK(number_in_words(472) == "four hundred and seventy-two");
    CHECK(number_in_words(910) == "nine hundred and ten");
    CHECK(number_in_words(999) == "nine hundred and ninety-nine");
    CHECK(number_in_words(-1).empty());
    CHECK(number_in_words(1000).empty());
}

TEST_CASE("The same seed gives the same challenge, and every challenge is in range", "[unit][gate]")
{
    CHECK(make_gate_challenge(5).number == make_gate_challenge(5).number);
    CHECK(make_gate_challenge(5).words == make_gate_challenge(5).words);

    std::set<int> seen;
    for (std::uint64_t seed = 0; seed < 2000; ++seed)
    {
        const GateChallenge challenge = make_gate_challenge(seed);
        REQUIRE(challenge.number >= GATE_NUMBER_MIN);
        REQUIRE(challenge.number <= GATE_NUMBER_MAX);
        REQUIRE(challenge.words == number_in_words(challenge.number));
        seen.insert(challenge.number);
    }
    CHECK(seen.size() > 500); // the seeds spread over the range
}

TEST_CASE("Only the number in digits passes", "[unit][gate]")
{
    const GateChallenge challenge{472, number_in_words(472)};
    CHECK(gate_answer_matches(challenge, "472"));
    CHECK(gate_answer_matches(challenge, "  472 "));
    CHECK(gate_answer_matches(challenge, "0472"));

    CHECK_FALSE(gate_answer_matches(challenge, ""));
    CHECK_FALSE(gate_answer_matches(challenge, "   "));
    CHECK_FALSE(gate_answer_matches(challenge, "473"));
    CHECK_FALSE(gate_answer_matches(challenge, "47 2"));
    CHECK_FALSE(gate_answer_matches(challenge, "472x"));
    CHECK_FALSE(gate_answer_matches(challenge, "-472"));
    CHECK_FALSE(gate_answer_matches(challenge, "four hundred and seventy-two"));
    CHECK_FALSE(gate_answer_matches(challenge, "000000472")); // longer than any answer is read
    CHECK_FALSE(gate_answer_matches(challenge, "99999999999999999999"));
}
