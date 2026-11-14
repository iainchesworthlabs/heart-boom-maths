// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/question_generator.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/state_hash.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <vector>

using namespace heart_boom_maths;

namespace
{

constexpr int SEEDS_PER_LEVEL = 300;

// Levels 1 to 12 from the campaign table, then endless levels near the start of the ramp, at its end and far past it.
constexpr std::array<int, 17> ALL_TEST_LEVELS{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 20, 40, 100};

const Tuning& tuning()
{
    return Tuning::defaults();
}

int largest_number(int level)
{
    return level <= tuning().campaign_levels ? tuning().levels[static_cast<std::size_t>(level - 1)].largest_number
                                             : tuning().endless.largest_number;
}

// The value of the sentence as a child would work it out, written separately from the generator's own arithmetic.
int answer_of(const Question& q)
{
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        return q.terms[1] - q.terms[0];
    }
    int value = q.terms[0];
    for (std::size_t i = 1; i < static_cast<std::size_t>(q.term_count); ++i)
    {
        value = q.operators[i - 1] == Operator::ADD ? value + q.terms[i] : value - q.terms[i];
    }
    return value;
}

std::string describe(const Question& q)
{
    std::string text = std::to_string(q.terms[0]);
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        text += " + ? = " + std::to_string(q.terms[1]);
    }
    else
    {
        for (std::size_t i = 1; i < static_cast<std::size_t>(q.term_count); ++i)
        {
            text += q.operators[i - 1] == Operator::ADD ? " + " : " - ";
            text += std::to_string(q.terms[i]);
        }
        text += " = ?";
    }
    text += " [";
    for (std::size_t i = 0; i < q.tiles.size(); ++i)
    {
        text += (i == 0 ? "" : " ") + std::to_string(q.tiles[i]);
    }
    text += "] " + std::to_string(q.correct_tile);
    return text;
}

// A question without its tiles, for repeat checks.
using Identity = std::tuple<int, int, std::array<int, 4>, std::array<int, 3>>;

Identity identity_of(const Question& q)
{
    std::array<int, 3> ops{};
    for (std::size_t i = 0; i < ops.size(); ++i)
    {
        ops[i] = static_cast<int>(q.operators[i]);
    }
    return {static_cast<int>(q.shape), q.term_count, q.terms, ops};
}

bool same_question(const Question& a, const Question& b)
{
    return identity_of(a) == identity_of(b) && a.answer == b.answer && a.tiles == b.tiles &&
           a.correct_tile == b.correct_tile;
}

// Checks everything a single question must satisfy on its own. Returns the first problem found, or an empty string.
std::string problem_with(const Question& q, int level)
{
    const int largest = largest_number(level);
    const int minimum = tuning().operand_minimum;

    const bool campaign = level <= tuning().campaign_levels;
    if (campaign && q.shape != tuning().levels[static_cast<std::size_t>(level - 1)].shape)
    {
        return "shape does not match the level table";
    }
    const int expected_terms = q.shape == QuestionShape::THREE_TERM ? 3 : (q.shape == QuestionShape::FOUR_TERM ? 4 : 2);
    if (q.term_count != expected_terms)
    {
        return "term count does not match the shape";
    }

    for (std::size_t i = 0; i < q.terms.size(); ++i)
    {
        if (static_cast<int>(i) < q.term_count)
        {
            if (q.terms[i] < minimum || q.terms[i] > largest)
            {
                return "operand out of range";
            }
        }
        else if (q.terms[i] != 0)
        {
            return "unused term is not zero";
        }
    }

    if (q.answer != answer_of(q))
    {
        return "answer is not the value of the sentence";
    }
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        if (q.answer < minimum || q.answer > largest)
        {
            return "missing addend out of range";
        }
        if (q.operators[0] != Operator::ADD)
        {
            return "missing addend must be an addition";
        }
    }
    else
    {
        int running = q.terms[0];
        for (std::size_t i = 1; i < static_cast<std::size_t>(q.term_count); ++i)
        {
            running = q.operators[i - 1] == Operator::ADD ? running + q.terms[i] : running - q.terms[i];
            if (running < 0 || running > largest)
            {
                return "intermediate result out of range";
            }
        }
    }

    if (campaign)
    {
        const Operations operations = tuning().levels[static_cast<std::size_t>(level - 1)].operations;
        for (std::size_t i = 0; i + 1 < static_cast<std::size_t>(q.term_count); ++i)
        {
            if (operations == Operations::ADD && q.operators[i] != Operator::ADD)
            {
                return "a subtraction on an adding level";
            }
            if (operations == Operations::TAKE_AWAY && q.operators[i] != Operator::SUBTRACT)
            {
                return "an addition on a take-away level";
            }
        }
    }

    if (q.correct_tile < 0 || q.correct_tile > 3)
    {
        return "correct tile index out of range";
    }
    if (q.tiles[static_cast<std::size_t>(q.correct_tile)] != q.answer)
    {
        return "the correct tile does not hold the answer";
    }
    if (std::count(q.tiles.begin(), q.tiles.end(), q.answer) != 1)
    {
        return "the answer is not on exactly one tile";
    }
    const std::set<int> distinct(q.tiles.begin(), q.tiles.end());
    if (distinct.size() != 4)
    {
        return "tiles are not distinct";
    }
    for (const int tile : q.tiles)
    {
        if (tile < 0 || tile > largest)
        {
            return "tile out of range";
        }
    }
    return {};
}

// Deals one level of `count` questions from a fresh generator.
std::vector<Question> deal_level(int level, std::uint64_t seed, int count)
{
    QuestionGenerator gen(tuning());
    Rng rng(seed);
    gen.begin_level(level, rng);
    std::vector<Question> questions;
    for (int i = 0; i < count; ++i)
    {
        questions.push_back(gen.next(rng));
    }
    return questions;
}

} // namespace

TEST_CASE("Generator deals valid questions on every level for many seeds", "[property][generator]")
{
    for (const int level : ALL_TEST_LEVELS)
    {
        for (int seed = 1; seed <= SEEDS_PER_LEVEL; ++seed)
        {
            const std::vector<Question> questions = deal_level(level, static_cast<std::uint64_t>(seed), 10);
            std::set<Identity> seen;
            for (const Question& q : questions)
            {
                const std::string problem = problem_with(q, level);
                INFO("level " << level << " seed " << seed << ": " << describe(q));
                REQUIRE(problem.empty());
                REQUIRE(seen.insert(identity_of(q)).second); // no repeat within the level
            }
        }
    }
}

TEST_CASE("Generator is repeatable for one seed", "[property][generator]")
{
    for (const int level : ALL_TEST_LEVELS)
    {
        const std::vector<Question> a = deal_level(level, 77, 10);
        const std::vector<Question> b = deal_level(level, 77, 10);
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            REQUIRE(same_question(a[i], b[i]));
        }
    }
}

TEST_CASE("Generator deals every level's own shape", "[unit][generator]")
{
    for (int level = 1; level <= 12; ++level)
    {
        const LevelSpec& spec = tuning().levels[static_cast<std::size_t>(level - 1)];
        std::set<int> operators_seen;
        for (std::uint64_t seed = 1; seed <= 50; ++seed)
        {
            for (const Question& q : deal_level(level, seed, 10))
            {
                REQUIRE(q.shape == spec.shape);
                for (std::size_t i = 0; i + 1 < static_cast<std::size_t>(q.term_count); ++i)
                {
                    operators_seen.insert(static_cast<int>(q.operators[i]));
                }
            }
        }
        if (spec.operations == Operations::MIXED && spec.shape == QuestionShape::TWO_TERM)
        {
            // Mixed two-term levels use both operations.
            REQUIRE(operators_seen.size() == 2);
        }
    }
}

TEST_CASE("Generator counts the possible questions of a level", "[unit][generator]")
{
    const QuestionGenerator gen(tuning());
    REQUIRE(gen.possible_question_count(1) == 10);
    REQUIRE(gen.possible_question_count(2) == 15);
    // From level 3 on there are more than 50, so the count stops at the cap.
    for (const int level : {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 100})
    {
        REQUIRE(gen.possible_question_count(level) == 51);
        REQUIRE(gen.possible_question_count(level, 60) == 60);
    }
}

TEST_CASE("Level 1 deals each of its ten possible questions once", "[property][generator]")
{
    // Every a + b with both operands at least 1 and a sum of at most 5.
    std::set<std::pair<int, int>> possible;
    for (int a = 1; a <= 5; ++a)
    {
        for (int b = 1; a + b <= 5; ++b)
        {
            possible.insert({a, b});
        }
    }
    REQUIRE(possible.size() == 10);

    for (std::uint64_t seed = 1; seed <= SEEDS_PER_LEVEL; ++seed)
    {
        std::set<std::pair<int, int>> dealt;
        for (const Question& q : deal_level(1, seed, 10))
        {
            REQUIRE(q.operators[0] == Operator::ADD);
            REQUIRE(dealt.insert({q.terms[0], q.terms[1]}).second);
        }
        REQUIRE(dealt == possible);
    }
}

TEST_CASE("Level 2 deals each of its fifteen possible questions once", "[property][generator]")
{
    // Every a - b with b at least 1, a at most 5 and a result of at least 0.
    std::set<std::pair<int, int>> possible;
    for (int a = 1; a <= 5; ++a)
    {
        for (int b = 1; b <= a; ++b)
        {
            possible.insert({a, b});
        }
    }
    REQUIRE(possible.size() == 15);

    for (std::uint64_t seed = 1; seed <= SEEDS_PER_LEVEL; ++seed)
    {
        // Fifteen deals show the whole list, and the first ten of them are what a level plays.
        std::set<std::pair<int, int>> dealt;
        for (const Question& q : deal_level(2, seed, 15))
        {
            REQUIRE(q.operators[0] == Operator::SUBTRACT);
            REQUIRE(dealt.insert({q.terms[0], q.terms[1]}).second);
        }
        REQUIRE(dealt == possible);
    }
}

TEST_CASE("Levels 1 and 2 shuffle their lists differently for different seeds", "[unit][generator]")
{
    for (const int level : {1, 2})
    {
        std::set<std::vector<Identity>> orders;
        for (std::uint64_t seed = 1; seed <= 20; ++seed)
        {
            std::vector<Identity> order;
            for (const Question& q : deal_level(level, seed, 10))
            {
                order.push_back(identity_of(q));
            }
            orders.insert(order);
        }
        REQUIRE(orders.size() > 15);
    }
}

TEST_CASE("Generator redraws a repeat when a level has a long list", "[unit][generator]")
{
    // Level 3 has 100 possible questions. Dealing 60 of them from one level must never repeat.
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        std::set<Identity> seen;
        for (const Question& q : deal_level(3, seed, 60))
        {
            REQUIRE(seen.insert(identity_of(q)).second);
        }
    }
}

TEST_CASE("Generator starts a new level with a clean memory", "[unit][generator]")
{
    QuestionGenerator gen(tuning());
    Rng rng(5);
    gen.begin_level(1, rng);
    for (int i = 0; i < 10; ++i)
    {
        (void)gen.next(rng);
    }
    // All ten level 1 questions are used. A new level 1 deals the whole list again.
    gen.begin_level(1, rng);
    std::set<Identity> seen;
    for (int i = 0; i < 10; ++i)
    {
        REQUIRE(seen.insert(identity_of(gen.next(rng))).second);
    }
    REQUIRE(seen.size() == 10);
}

TEST_CASE("Generator deals the list again when more questions are asked than a level has", "[unit][generator]")
{
    QuestionGenerator gen(tuning());
    Rng rng(9);
    gen.begin_level(1, rng);
    std::set<Identity> first;
    std::set<Identity> second;
    for (int i = 0; i < 10; ++i)
    {
        first.insert(identity_of(gen.next(rng)));
    }
    for (int i = 0; i < 10; ++i)
    {
        const Question q = gen.next(rng);
        REQUIRE(problem_with(q, 1).empty());
        second.insert(identity_of(q));
    }
    REQUIRE(first.size() == 10);
    REQUIRE(second == first);
}

TEST_CASE("Generator begins level 1 by itself when next is called first", "[unit][generator]")
{
    QuestionGenerator gen(tuning());
    Rng rng(3);
    const Question q = gen.next(rng);
    REQUIRE(problem_with(q, 1).empty());
}

TEST_CASE("Wrong tiles are near misses of the answer", "[property][generator]")
{
    // On a wide level most wrong tiles are within 1, 2 or 10 of the answer, or its two last digits swapped.
    int near_misses = 0;
    int wrong_tiles = 0;
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        for (const Question& q : deal_level(12, seed, 10))
        {
            const int tens = (q.answer / 10) % 10;
            const int units = q.answer % 10;
            const int swapped = q.answer - tens * 10 - units + units * 10 + tens;
            for (const int tile : q.tiles)
            {
                if (tile == q.answer)
                {
                    continue;
                }
                ++wrong_tiles;
                const int difference = std::abs(tile - q.answer);
                if (difference == 1 || difference == 2 || difference == 10 || (q.answer >= 10 && tile == swapped))
                {
                    ++near_misses;
                }
            }
        }
    }
    // Answers near 0 or 1,000 have too few near misses and take the nearest numbers instead, which are 1 or 2 away.
    REQUIRE(near_misses == wrong_tiles);
}

TEST_CASE("Tiles of a tiny answer stay in range and are topped up", "[unit][generator]")
{
    // On level 1 the answer is 2 to 5 and the range is 0 to 5, so only a few of the near misses qualify.
    for (std::uint64_t seed = 1; seed <= SEEDS_PER_LEVEL; ++seed)
    {
        for (const Question& q : deal_level(1, seed, 10))
        {
            const std::set<int> tiles(q.tiles.begin(), q.tiles.end());
            REQUIRE(tiles.size() == 4);
            REQUIRE(*tiles.begin() >= 0);
            REQUIRE(*tiles.rbegin() <= 5);
        }
    }
}

TEST_CASE("The correct tile lands on every position", "[property][generator]")
{
    std::array<int, 4> positions{};
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        for (const Question& q : deal_level(8, seed, 10))
        {
            ++positions[static_cast<std::size_t>(q.correct_tile)];
        }
    }
    // 2000 deals over four positions: about 500 each.
    for (const int count : positions)
    {
        REQUIRE(count > 400);
        REQUIRE(count < 600);
    }
}

TEST_CASE("Endless levels draw the multi-term share the ramp asks for", "[property][generator]")
{
    const auto multi_share = [](int level)
    {
        int multi = 0;
        int total = 0;
        for (std::uint64_t seed = 1; seed <= SEEDS_PER_LEVEL; ++seed)
        {
            for (const Question& q : deal_level(level, seed, 10))
            {
                multi += q.shape == QuestionShape::THREE_TERM || q.shape == QuestionShape::FOUR_TERM ? 1 : 0;
                ++total;
            }
        }
        return multi * 100 / total;
    };

    const int level_13 = multi_share(13); // 40 percent
    REQUIRE(level_13 >= 36);
    REQUIRE(level_13 <= 44);
    const int level_16 = multi_share(16); // 65 percent
    REQUIRE(level_16 >= 61);
    REQUIRE(level_16 <= 69);
    REQUIRE(multi_share(20) == 100);
    REQUIRE(multi_share(100) == 100);
}

TEST_CASE("Endless levels use every shape", "[property][generator]")
{
    std::set<int> shapes;
    std::set<int> three_term_patterns;
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        for (const Question& q : deal_level(16, seed, 10))
        {
            shapes.insert(static_cast<int>(q.shape));
            if (q.shape == QuestionShape::THREE_TERM)
            {
                three_term_patterns.insert(static_cast<int>(q.operators[1]));
            }
        }
    }
    REQUIRE(shapes.size() == 4);
    REQUIRE(three_term_patterns.size() == 2);
}

TEST_CASE("Generator survives a very high endless level", "[unit][generator]")
{
    for (const int level : {1000, 1000000, 2147483647})
    {
        QuestionGenerator gen(tuning());
        Rng rng(1);
        gen.begin_level(level, rng);
        const Question q = gen.next(rng);
        REQUIRE(problem_with(q, level).empty());
        QuestionGenerator copy(tuning());
        REQUIRE(copy.restore(gen.snapshot()));
        REQUIRE(copy.snapshot() == gen.snapshot());
    }
}

TEST_CASE("A restored generator deals the same next question", "[unit][generator]")
{
    for (const int level : ALL_TEST_LEVELS)
    {
        for (const int dealt_before_save : {0, 1, 4, 9, 10})
        {
            QuestionGenerator original(tuning());
            Rng rng(1000 + static_cast<std::uint64_t>(level));
            original.begin_level(level, rng);
            for (int i = 0; i < dealt_before_save; ++i)
            {
                (void)original.next(rng);
            }

            const std::vector<std::int64_t> saved = original.snapshot();
            const std::uint64_t saved_rng = rng.state();

            QuestionGenerator restored(tuning());
            REQUIRE(restored.restore(saved));
            REQUIRE(restored.snapshot() == saved);
            Rng restored_rng(0);
            restored_rng.set_state(saved_rng);

            // Carry on past ten, which covers the end of a list and a fresh deal.
            for (int i = 0; i < 12; ++i)
            {
                const Question a = original.next(rng);
                const Question b = restored.next(restored_rng);
                INFO("level " << level << " saved after " << dealt_before_save << " questions, next " << i);
                REQUIRE(same_question(a, b));
            }
            REQUIRE(original.snapshot() == restored.snapshot());
        }
    }
}

TEST_CASE("A snapshot with the same hash comes from the same state", "[unit][generator]")
{
    QuestionGenerator a(tuning());
    QuestionGenerator b(tuning());
    Rng rng_a(8);
    Rng rng_b(8);
    a.begin_level(6, rng_a);
    b.begin_level(6, rng_b);
    for (int i = 0; i < 5; ++i)
    {
        (void)a.next(rng_a);
        (void)b.next(rng_b);
    }
    REQUIRE(fnv1a_64(a.snapshot()) == fnv1a_64(b.snapshot()));
    (void)a.next(rng_a);
    REQUIRE(fnv1a_64(a.snapshot()) != fnv1a_64(b.snapshot()));
}

TEST_CASE("A generator that has not begun a level snapshots and restores", "[unit][generator]")
{
    const QuestionGenerator fresh(tuning());
    QuestionGenerator target(tuning());
    REQUIRE(target.restore(fresh.snapshot()));
    REQUIRE(target.snapshot() == fresh.snapshot());
}

TEST_CASE("Restore rejects garbage and changes nothing", "[unit][generator]")
{
    QuestionGenerator gen(tuning());
    Rng rng(21);
    gen.begin_level(1, rng);
    for (int i = 0; i < 3; ++i)
    {
        (void)gen.next(rng);
    }
    const std::vector<std::int64_t> good = gen.snapshot();
    REQUIRE(good.size() == 3 + 7); // header and the seven level 1 questions still to deal

    QuestionGenerator redraw(tuning());
    Rng redraw_rng(22);
    redraw.begin_level(6, redraw_rng);
    for (int i = 0; i < 3; ++i)
    {
        (void)redraw.next(redraw_rng);
    }
    const std::vector<std::int64_t> redraw_good = redraw.snapshot();
    REQUIRE(redraw_good.size() == 3 + 3);

    const auto rejects = [&](std::vector<std::int64_t> bad)
    {
        const std::vector<std::int64_t> before = gen.snapshot();
        const bool accepted = gen.restore(bad);
        return !accepted && gen.snapshot() == before;
    };

    SECTION("wrong sizes")
    {
        REQUIRE(rejects({}));
        REQUIRE(rejects({1}));
        REQUIRE(rejects({1, 2}));
        std::vector<std::int64_t> shorter = good;
        shorter.pop_back();
        REQUIRE(rejects(shorter));
        std::vector<std::int64_t> longer = good;
        longer.push_back(good.back());
        REQUIRE(rejects(longer));
    }
    SECTION("bad header")
    {
        std::vector<std::int64_t> bad = good;
        bad[0] = 0; // level
        REQUIRE(rejects(bad));
        bad[0] = -4;
        REQUIRE(rejects(bad));
        bad[0] = std::int64_t{1} << 40;
        REQUIRE(rejects(bad));

        bad = good;
        bad[1] = 3; // mode
        REQUIRE(rejects(bad));
        bad[1] = -1;
        REQUIRE(rejects(bad));
        bad[1] = 1; // redraw mode on a level that lists all its questions
        REQUIRE(rejects(bad));

        bad = good;
        bad[2] = -7; // count
        REQUIRE(rejects(bad));
        bad[2] = 100;
        REQUIRE(rejects(bad));

        // Level 6 holds more than 50 questions, so it cannot be in list mode.
        bad = redraw_good;
        bad[1] = 2;
        REQUIRE(rejects(bad));
    }
    SECTION("keys that are not questions")
    {
        std::vector<std::int64_t> bad = good;
        bad[3] = -1;
        REQUIRE(rejects(bad));
        bad[3] = 0;
        REQUIRE(rejects(bad));
        bad[3] = std::numeric_limits<std::int64_t>::max();
        REQUIRE(rejects(bad));
        bad[3] = good[3] | (std::int64_t{1} << 62); // a stray high bit
        REQUIRE(rejects(bad));
        bad[3] = 6 | (std::int64_t{1} << 14); // 6 + 1 is above the largest number of level 1
        REQUIRE(rejects(bad));
        bad[3] = 1 | (std::int64_t{1} << 14) | (std::int64_t{1} << 28); // a third term on a two-term sentence
        REQUIRE(rejects(bad));
    }
    SECTION("a duplicate question")
    {
        std::vector<std::int64_t> bad = good;
        bad[4] = bad[3];
        REQUIRE(rejects(bad));
    }
    SECTION("questions that belong to another level")
    {
        // The generator at level 1 only deals additions of at most 5, so a level 6 question is not one of them.
        std::vector<std::int64_t> bad = good;
        bad[3] = redraw_good[3];
        REQUIRE(rejects(bad));

        // Level 2 holds subtractions, which are not level 1 questions.
        QuestionGenerator other(tuning());
        Rng other_rng(5);
        other.begin_level(2, other_rng);
        std::vector<std::int64_t> level_two = other.snapshot();
        level_two[0] = 1;
        REQUIRE(rejects(level_two));
    }
    SECTION("the good snapshots are accepted")
    {
        QuestionGenerator target(tuning());
        REQUIRE(target.restore(good));
        REQUIRE(target.snapshot() == good);
        REQUIRE(target.restore(redraw_good));
        REQUIRE(target.snapshot() == redraw_good);
    }
}

TEST_CASE("Restore accepts only canonical states for random input", "[unit][generator]")
{
    Rng noise(123);
    QuestionGenerator gen(tuning());
    int accepted = 0;
    for (int round = 0; round < 3000; ++round)
    {
        // Half the inputs are real snapshots with up to two numbers changed, so the checks past the header run too.
        std::vector<std::int64_t> values;
        if (noise.below(2) == 0)
        {
            QuestionGenerator source(tuning());
            Rng source_rng(noise.next_u64());
            source.begin_level(noise.between(1, 14), source_rng);
            for (int i = noise.between(0, 10); i > 0; --i)
            {
                (void)source.next(source_rng);
            }
            values = source.snapshot();
            for (int changes = noise.between(0, 2); changes > 0; --changes)
            {
                const auto index = static_cast<std::size_t>(noise.below(values.size()));
                values[index] = noise.below(3) == 0 ? values[index] + noise.between(-3, 3)
                                                    : static_cast<std::int64_t>(noise.below(20));
            }
        }
        else
        {
            const int size = noise.between(0, 8);
            for (int i = 0; i < size; ++i)
            {
                values.push_back(noise.below(5) == 0 ? static_cast<std::int64_t>(noise.next_u64())
                                                     : static_cast<std::int64_t>(noise.below(20)));
            }
        }

        const std::vector<std::int64_t> before = gen.snapshot();
        if (gen.restore(values))
        {
            ++accepted;
            REQUIRE(gen.snapshot() == values); // whatever is accepted is exactly what the generator would write
        }
        else
        {
            REQUIRE(gen.snapshot() == before);
        }
    }
    REQUIRE(accepted > 100);
}

TEST_CASE("Golden questions for seed 1 stay the same", "[unit][generator]")
{
    // These pin the generator's output for a fixed seed. Any change to the rules, the order of random draws or the
    // tuning changes them, and the change must be deliberate: regenerate the golden replays with it.
    // Each line reads "sentence [tiles in order] index of the correct tile".
    const std::map<int, std::vector<std::string>> expected{
        {1,
         {
             "2 + 1 = ? [2 4 5 3] 3",
             "1 + 3 = ? [3 5 4 2] 2",
             "3 + 2 = ? [4 2 5 3] 2",
             "1 + 2 = ? [3 2 4 5] 0",
         }},
        {2,
         {
             "4 - 1 = ? [2 3 1 4] 1",
             "4 - 4 = ? [3 1 0 2] 2",
             "3 - 1 = ? [2 0 1 3] 0",
             "5 - 1 = ? [2 4 5 3] 1",
         }},
        {3,
         {
             "10 - 1 = ? [10 8 9 7] 2",
             "5 - 2 = ? [2 4 5 3] 3",
             "7 - 2 = ? [7 4 6 5] 3",
             "7 - 6 = ? [0 2 3 1] 3",
         }},
        {5,
         {
             "10 + ? = 11 [11 3 0 1] 3",
             "2 + ? = 10 [8 18 10 6] 0",
             "2 + ? = 11 [19 8 7 9] 3",
             "2 + ? = 4 [0 2 12 3] 1",
         }},
        {6,
         {
             "2 + 1 + 5 = ? [18 10 8 9] 2",
             "8 + 3 + 2 = ? [11 3 13 12] 2",
             "1 + 2 + 2 = ? [5 7 15 6] 0",
             "8 + 5 + 1 = ? [13 12 15 14] 3",
         }},
        {8,
         {
             "11 + 19 - 15 = ? [14 17 15 5] 2",
             "2 + 36 - 38 = ? [2 10 1 0] 3",
             "18 + 31 - 10 = ? [41 37 49 39] 3",
             "39 + 5 - 1 = ? [45 34 33 43] 3",
         }},
        {10,
         {
             "67 + 12 + 1 - 15 = ? [67 63 65 55] 2",
             "25 + 7 + 22 - 17 = ? [39 37 73 35] 1",
             "61 + 12 + 4 - 31 = ? [48 46 47 64] 1",
             "44 + 7 + 44 - 59 = ? [26 36 34 35] 1",
         }},
        {12,
         {
             "898 - 379 = ? [509 529 519 520] 2",
             "551 + 159 = ? [700 711 710 708] 2",
             "211 + 241 = ? [452 462 425 450] 0",
             "489 - 258 = ? [229 230 221 231] 3",
         }},
        {13,
         {
             "202 + 576 = ? [768 788 780 778] 3",
             "137 + ? = 246 [119 107 109 110] 2",
             "7 + 13 = ? [18 20 22 2] 1",
             "239 + ? = 674 [425 437 445 435] 3",
         }},
        {20,
         {
             "412 + 189 + 115 - 126 = ? [591 592 590 600] 2",
             "144 + 28 + 284 = ? [465 455 454 456] 3",
             "314 + 69 + 410 - 84 = ? [707 708 699 709] 3",
             "278 + 370 + 69 - 29 = ? [687 698 686 688] 3",
         }},
    };
    for (const auto& [level, lines] : expected)
    {
        const std::vector<Question> questions = deal_level(level, 1, static_cast<int>(lines.size()));
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            INFO("level " << level << " question " << i);
            REQUIRE(describe(questions[i]) == lines[i]);
        }
    }

    // A hash over every number of a whole level's ten questions, for levels 1 to 14, covers the rest.
    const std::array<std::uint64_t, 14> hashes{
        0xB8ED0ED6C7CE6722ULL, // level 1
        0xED34721C9A5A4B45ULL, // level 2
        0x5FE28BFC8ED91780ULL, // level 3
        0x41D8C90FA2C00A25ULL, // level 4
        0x75A00089499CCF32ULL, // level 5
        0x9290D64B6E4A9EF9ULL, // level 6
        0x1D05E4CC74270CDDULL, // level 7
        0x443335EFF59251F3ULL, // level 8
        0x2E743987843E2798ULL, // level 9
        0xBC3EAF391BDB0D92ULL, // level 10
        0x40EC33E688691674ULL, // level 11
        0x5B490075DC54213BULL, // level 12
        0xC7B0D428223F60F0ULL, // level 13
        0x721ADEDC2500748AULL, // level 14
    };
    for (int level = 1; level <= 14; ++level)
    {
        std::vector<std::int64_t> numbers;
        for (const Question& q : deal_level(level, 1, 10))
        {
            for (const int term : q.terms)
            {
                numbers.push_back(term);
            }
            for (const Operator op : q.operators)
            {
                numbers.push_back(static_cast<std::int64_t>(op));
            }
            for (const int tile : q.tiles)
            {
                numbers.push_back(tile);
            }
            numbers.push_back(q.correct_tile);
        }
        INFO("level " << level);
        REQUIRE(fnv1a_64(numbers) == hashes[static_cast<std::size_t>(level - 1)]);
    }
}
