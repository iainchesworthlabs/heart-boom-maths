// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/question_generator.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/state_hash.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using namespace heart_boom_maths;

TEST_CASE("Rng repeats exactly for one seed and stays inside its bounds", "[unit][rng]")
{
    Rng a(42);
    Rng b(42);
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(a.next_u64() == b.next_u64());
    }
    for (int i = 0; i < 1000; ++i)
    {
        const int v = a.between(3, 9);
        REQUIRE(v >= 3);
        REQUIRE(v <= 9);
    }
}

TEST_CASE("Rng state can be saved and restored", "[unit][rng]")
{
    Rng a(7);
    (void)a.next_u64();
    Rng b(0);
    b.set_state(a.state());
    REQUIRE(a.next_u64() == b.next_u64());
}

TEST_CASE("fnv1a_64 of nothing is the offset basis", "[unit][hash]")
{
    REQUIRE(fnv1a_64({}) == 0xCBF29CE484222325ULL);
    const std::array<std::int64_t, 1> one{1};
    REQUIRE(fnv1a_64(one) != fnv1a_64({}));
}

TEST_CASE("The generator interface compiles and deals a question", "[unit][generator]")
{
    QuestionGenerator gen(Tuning::defaults());
    Rng rng(1);
    gen.begin_level(1, rng);
    const Question q = gen.next(rng);
    REQUIRE(q.tiles[static_cast<std::size_t>(q.correct_tile)] == q.answer);
}
