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
#include <utility>
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
                if (difference == 1 || difference == 2 || difference == 3 || difference == 10 ||
                    (q.answer >= 10 && tile == swapped))
                {
                    ++near_misses;
                }
            }
        }
    }
    // A side of the answer with too few near misses is topped up with the nearest unused numbers, which are 1, 2 or 3
    // away (3 only when the answer is 3 and all three wrong tiles have to be below it).
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

    const auto rejects = [&](const std::vector<std::int64_t>& bad)
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
             "2 + 1 = ? [2 3 5 1] 1",
             "1 + 3 = ? [2 3 4 5] 2",
             "3 + 2 = ? [3 2 4 5] 3",
             "1 + 2 = ? [3 4 2 5] 0",
         }},
        {2,
         {
             "4 - 1 = ? [2 3 1 5] 1",
             "4 - 4 = ? [3 1 0 2] 2",
             "3 - 1 = ? [2 0 1 4] 0",
             "5 - 1 = ? [1 4 3 2] 1",
         }},
        {3,
         {
             "6 - 5 = ? [2 3 1 4] 2",
             "3 - 1 = ? [3 5 2 4] 2",
             "2 + 8 = ? [1 9 10 8] 2",
             "1 + 6 = ? [7 9 8 10] 0",
         }},
        {5,
         {
             "8 + ? = 14 [6 5 3 4] 0",
             "4 + ? = 16 [14 13 11 12] 3",
             "3 + ? = 14 [11 14 12 13] 0",
             "1 + ? = 3 [3 1 2 0] 2",
         }},
        {6,
         {
             "2 + 13 + 4 = ? [19 18 9 17] 0",
             "3 + 7 + 6 = ? [18 17 15 16] 3",
             "4 + 11 + 5 = ? [10 20 19 2] 1",
             "7 + 10 + 2 = ? [9 18 20 19] 3",
         }},
        {8,
         {
             "18 + 21 - 25 = ? [14 13 4 12] 0",
             "31 + 8 - 1 = ? [39 38 40 37] 1",
             "9 + 16 - 23 = ? [0 12 2 3] 2",
             "22 + 7 - 28 = ? [1 11 0 2] 0",
         }},
        {10,
         {
             "52 + 18 + 19 - 32 = ? [57 56 47 55] 0",
             "16 + 23 + 19 - 8 = ? [51 50 52 48] 1",
             "29 + 33 + 4 - 2 = ? [66 65 64 74] 2",
             "37 + 49 + 11 - 84 = ? [15 23 31 13] 3",
         }},
        {12,
         {
             "378 - 267 = ? [121 110 111 101] 2",
             "303 + 669 = ? [962 971 972 974] 2",
             "72 + 648 = ? [720 718 719 730] 0",
             "923 - 229 = ? [693 692 696 694] 3",
         }},
        {13,
         {
             "120 + 170 = ? [300 291 289 290] 3",
             "578 + ? = 667 [98 90 89 91] 2",
             "574 + 168 = ? [741 742 740 724] 1",
             "598 + ? = 629 [41 33 31 32] 2",
         }},
        {20,
         {
             "286 + 558 + 58 - 342 = ? [559 506 560 558] 2",
             "45 + 33 + 307 = ? [387 386 385 395] 2",
             "242 + 267 + 65 = ? [574 584 575 573] 0",
             "131 + 194 + 143 = ? [470 468 469 486] 1",
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
        0xB5EBD2DE229E4A67ULL, // level 1
        0x283F9EC6FFB6C540ULL, // level 2
        0x4519B94E2949886AULL, // level 3
        0x476AEEC67157248EULL, // level 4
        0xF9D70258C84142B1ULL, // level 5
        0x7C08DBDB6EA3A93CULL, // level 6
        0x64C997B88C590924ULL, // level 7
        0x5C24E42855A9BF91ULL, // level 8
        0x9BCBCCB91FD77035ULL, // level 9
        0x8D199CD44EBB3DC0ULL, // level 10
        0x2D26D70D5DAFB73DULL, // level 11
        0x70A70EF8D536D588ULL, // level 12
        0x390B5255388C2692ULL, // level 13
        0xCC5CE09BD46E160FULL, // level 14
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

// ---- Even dealing: every possible question of a kind has the same chance
// ---------------------------------------------

namespace
{

constexpr std::array<QuestionKind, 6> ALL_KINDS{QuestionKind::ADD2, QuestionKind::SUB2,     QuestionKind::MISSING,
                                                QuestionKind::ADD3, QuestionKind::ADD_SUB3, QuestionKind::ADD_ADD_SUB4};

// A question of a kind, without tiles, as what tells it from another of the same kind: the numbers and the answer.
using KindKey = std::pair<std::array<int, 4>, int>;

// Every question of a kind for a minimum and a largest number, by plain nested loops written for this test only. It
// calls `emit(terms, answer)` once for each. The ranges are the rules of a level: every operand from `m` to `top`,
// every running total from 0 to `top`, and for a + ? = c a missing number of at least `m`.
template <typename Emit>
void enumerate_kind(QuestionKind kind, int m, int top, Emit emit)
{
    switch (kind)
    {
    case QuestionKind::ADD2:
        for (int a = m; a <= top; ++a)
        {
            for (int b = m; a + b <= top; ++b)
            {
                emit(std::array<int, 4>{a, b, 0, 0}, a + b);
            }
        }
        break;
    case QuestionKind::SUB2:
        for (int a = m; a <= top; ++a)
        {
            for (int b = m; b <= a; ++b)
            {
                emit(std::array<int, 4>{a, b, 0, 0}, a - b);
            }
        }
        break;
    case QuestionKind::MISSING:
        for (int a = m; a <= top; ++a)
        {
            for (int missing = m; a + missing <= top; ++missing)
            {
                emit(std::array<int, 4>{a, a + missing, 0, 0}, missing);
            }
        }
        break;
    case QuestionKind::ADD3:
        for (int a = m; a <= top; ++a)
        {
            for (int b = m; a + b <= top; ++b)
            {
                for (int c = m; a + b + c <= top; ++c)
                {
                    emit(std::array<int, 4>{a, b, c, 0}, a + b + c);
                }
            }
        }
        break;
    case QuestionKind::ADD_SUB3:
        for (int a = m; a <= top; ++a)
        {
            for (int b = m; a + b <= top; ++b)
            {
                for (int c = m; c <= a + b; ++c)
                {
                    emit(std::array<int, 4>{a, b, c, 0}, a + b - c);
                }
            }
        }
        break;
    case QuestionKind::ADD_ADD_SUB4:
        for (int a = m; a <= top; ++a)
        {
            for (int b = m; a + b <= top; ++b)
            {
                for (int c = m; a + b + c <= top; ++c)
                {
                    for (int d = m; d <= a + b + c; ++d)
                    {
                        emit(std::array<int, 4>{a, b, c, d}, a + b + c - d);
                    }
                }
            }
        }
        break;
    }
}

std::int64_t enumerated_count(QuestionKind kind, int m, int top)
{
    std::int64_t count = 0;
    enumerate_kind(kind, m, top, [&](const std::array<int, 4>&, int) { ++count; });
    return count;
}

// The same count as a sum over the total (the result, or the sum before a take-away), with the number of ways to split
// each total written out by hand. It is cheap at every release size, including the four-term kind at 1,000.
std::int64_t summed_count(QuestionKind kind, int m, int top)
{
    std::int64_t count = 0;
    for (std::int64_t s = 1; s <= top; ++s)
    {
        const std::int64_t two_parts = s >= 2 * m ? s - 2 * m + 1 : 0;
        const std::int64_t three_parts = s >= 3 * m ? (s - 3 * m + 2) * (s - 3 * m + 1) / 2 : 0;
        const std::int64_t takes = s >= m ? s - m + 1 : 0;
        switch (kind)
        {
        case QuestionKind::ADD2:
        case QuestionKind::MISSING:
            count += two_parts;
            break;
        case QuestionKind::SUB2:
            count += takes;
            break;
        case QuestionKind::ADD3:
            count += three_parts;
            break;
        case QuestionKind::ADD_SUB3:
            count += two_parts * takes;
            break;
        case QuestionKind::ADD_ADD_SUB4:
            count += three_parts * takes;
            break;
        }
    }
    return count;
}

// A closed form for the four-term kind at a minimum of 1: a + b + c - d has 3 * C(largest + 1, 4) questions.
std::int64_t four_term_closed_form(std::int64_t top)
{
    return 3 * ((top + 1) * top * (top - 1) * (top - 2) / 24);
}

// True when the question is one of the kind for these numbers, checked from its own numbers and not from a list.
bool is_kind_question(const Question& q, QuestionKind kind, int m, int top)
{
    if (!std::all_of(q.tiles.begin(), q.tiles.end(), [](int tile) { return tile == 0; }))
    {
        return false; // the tiles are not dealt by these functions
    }
    for (std::size_t i = 0; i < q.terms.size(); ++i)
    {
        const bool used = static_cast<int>(i) < q.term_count;
        if (used ? (q.terms[i] < m || q.terms[i] > top) : q.terms[i] != 0)
        {
            return false;
        }
    }
    const auto in_range = [&](int value) { return value >= 0 && value <= top; };
    bool found = false;
    switch (kind)
    {
    case QuestionKind::ADD2:
        found =
            q.shape == QuestionShape::TWO_TERM && q.operators[0] == Operator::ADD && in_range(q.terms[0] + q.terms[1]);
        break;
    case QuestionKind::SUB2:
        found = q.shape == QuestionShape::TWO_TERM && q.operators[0] == Operator::SUBTRACT &&
                in_range(q.terms[0] - q.terms[1]);
        break;
    case QuestionKind::MISSING:
        found = q.shape == QuestionShape::MISSING_ADDEND && q.terms[1] - q.terms[0] >= m;
        break;
    case QuestionKind::ADD3:
        found = q.shape == QuestionShape::THREE_TERM && q.operators[0] == Operator::ADD &&
                q.operators[1] == Operator::ADD && in_range(q.terms[0] + q.terms[1] + q.terms[2]);
        break;
    case QuestionKind::ADD_SUB3:
        found = q.shape == QuestionShape::THREE_TERM && q.operators[0] == Operator::ADD &&
                q.operators[1] == Operator::SUBTRACT && in_range(q.terms[0] + q.terms[1]) &&
                in_range(q.terms[0] + q.terms[1] - q.terms[2]);
        break;
    case QuestionKind::ADD_ADD_SUB4:
        found = q.shape == QuestionShape::FOUR_TERM && q.operators[0] == Operator::ADD &&
                q.operators[1] == Operator::ADD && q.operators[2] == Operator::SUBTRACT &&
                in_range(q.terms[0] + q.terms[1] + q.terms[2]) &&
                in_range(q.terms[0] + q.terms[1] + q.terms[2] - q.terms[3]);
        break;
    }
    return found && q.answer == answer_of(q);
}

// The minimum and largest number pairs the exact tests run over: the level 1 range, ranges around the point where the
// four-term kind first has a question, and some with a minimum above 1.
constexpr std::array<std::pair<int, int>, 11> SMALL_RANGES{
    {{1, 4}, {1, 5}, {1, 6}, {1, 8}, {1, 10}, {1, 13}, {1, 20}, {2, 8}, {2, 11}, {3, 12}, {3, 17}}};

} // namespace

TEST_CASE("Every question of a kind has exactly one index", "[unit][generator]")
{
    for (const QuestionKind kind : ALL_KINDS)
    {
        for (const auto& [m, top] : SMALL_RANGES)
        {
            INFO("kind " << static_cast<int>(kind) << " minimum " << m << " largest " << top);

            // The reference: the plain enumeration of this test and the generator's own list must agree.
            std::set<KindKey> expected;
            enumerate_kind(kind, m, top,
                           [&](const std::array<int, 4>& terms, int answer) { expected.insert({terms, answer}); });
            std::set<KindKey> listed;
            const std::vector<Question> list = list_question_kind(kind, m, top);
            for (const Question& q : list)
            {
                listed.insert({q.terms, q.answer});
            }
            REQUIRE(listed.size() == list.size());
            REQUIRE(listed == expected);

            // The count is the size of the list, and the indexes [0, count) are the questions, each exactly once.
            const std::int64_t count = question_kind_count(kind, m, top);
            REQUIRE(count == static_cast<std::int64_t>(list.size()));
            std::set<KindKey> dealt;
            for (std::int64_t index = 0; index < count; ++index)
            {
                const Question q = question_kind_at(kind, m, top, index);
                REQUIRE(is_kind_question(q, kind, m, top));
                dealt.insert({q.terms, q.answer});
            }
            REQUIRE(static_cast<std::int64_t>(dealt.size()) == count);
            REQUIRE(dealt == expected);
        }
    }
}

TEST_CASE("Every question of the four-term kind has one index in a mid-size range", "[unit][generator]")
{
    // 3 * C(41, 4) = 334,730 questions: enough to reach every branch of the three-way split at larger totals.
    const int m = 1;
    const int top = 40;
    const std::int64_t count = question_kind_count(QuestionKind::ADD_ADD_SUB4, m, top);
    REQUIRE(count == four_term_closed_form(top));
    REQUIRE(count == enumerated_count(QuestionKind::ADD_ADD_SUB4, m, top));
    std::set<KindKey> dealt;
    for (std::int64_t index = 0; index < count; ++index)
    {
        const Question q = question_kind_at(QuestionKind::ADD_ADD_SUB4, m, top, index);
        REQUIRE(is_kind_question(q, QuestionKind::ADD_ADD_SUB4, m, top));
        dealt.insert({q.terms, q.answer});
    }
    REQUIRE(static_cast<std::int64_t>(dealt.size()) == count);
}

TEST_CASE("A range too small for a kind has no questions of it", "[unit][generator]")
{
    REQUIRE(question_kind_count(QuestionKind::ADD2, 1, 1) == 0);
    REQUIRE(question_kind_count(QuestionKind::ADD3, 1, 2) == 0);
    REQUIRE(question_kind_count(QuestionKind::ADD_ADD_SUB4, 1, 2) == 0);
    REQUIRE(question_kind_count(QuestionKind::ADD_ADD_SUB4, 2, 5) == 0);
    REQUIRE(question_kind_count(QuestionKind::ADD_ADD_SUB4, 1, 3) == 3);
    REQUIRE(list_question_kind(QuestionKind::ADD_ADD_SUB4, 1, 3).size() == 3);
}

TEST_CASE("The counts of the kinds match a plain enumeration and a sum at every release size", "[unit][generator]")
{
    // The largest numbers of the campaign (5 to 1,000) and of endless play (1,000).
    std::set<int> largest_numbers{tuning().endless.largest_number};
    for (const LevelSpec& spec : tuning().levels)
    {
        largest_numbers.insert(spec.largest_number);
    }
    REQUIRE(largest_numbers.size() == 7);
    const int m = tuning().operand_minimum;
    REQUIRE(m == 1);

    for (const int top : largest_numbers)
    {
        for (const QuestionKind kind : ALL_KINDS)
        {
            INFO("kind " << static_cast<int>(kind) << " largest " << top);
            const std::int64_t count = question_kind_count(kind, m, top);
            REQUIRE(count == summed_count(kind, m, top));
            if (count <= 20'000'000)
            {
                REQUIRE(count == enumerated_count(kind, m, top)); // slow, so only where it takes a moment
            }
        }
        REQUIRE(question_kind_count(QuestionKind::ADD_ADD_SUB4, m, top) == four_term_closed_form(top));
    }
    // The four-term kind at 1,000 is about 1.2 * 10^11 questions, so a count needs 64 bits.
    REQUIRE(question_kind_count(QuestionKind::ADD_ADD_SUB4, 1, 1000) > std::int64_t{100'000'000'000});
}

TEST_CASE("A level's possible questions are the sum of the counts of its kinds", "[unit][generator]")
{
    const QuestionGenerator gen(tuning());
    const auto kinds_of = [](const LevelSpec& spec)
    {
        std::vector<QuestionKind> kinds;
        switch (spec.shape)
        {
        case QuestionShape::TWO_TERM:
            if (spec.operations != Operations::TAKE_AWAY)
            {
                kinds.push_back(QuestionKind::ADD2);
            }
            if (spec.operations != Operations::ADD)
            {
                kinds.push_back(QuestionKind::SUB2);
            }
            break;
        case QuestionShape::MISSING_ADDEND:
            kinds.push_back(QuestionKind::MISSING);
            break;
        case QuestionShape::THREE_TERM:
            kinds.push_back(spec.operations == Operations::ADD ? QuestionKind::ADD3 : QuestionKind::ADD_SUB3);
            break;
        case QuestionShape::FOUR_TERM:
            kinds.push_back(QuestionKind::ADD_ADD_SUB4);
            break;
        }
        return kinds;
    };

    constexpr int CAP = 200'000;
    for (int level = 1; level <= tuning().campaign_levels; ++level)
    {
        const LevelSpec& spec = tuning().levels[static_cast<std::size_t>(level - 1)];
        std::int64_t sum = 0;
        for (const QuestionKind kind : kinds_of(spec))
        {
            sum += question_kind_count(kind, tuning().operand_minimum, spec.largest_number);
        }
        INFO("level " << level);
        REQUIRE(gen.possible_question_count(level, CAP) == std::min<std::int64_t>(sum, CAP));
    }
    std::int64_t endless = 0;
    for (const QuestionKind kind : ALL_KINDS)
    {
        endless += question_kind_count(kind, tuning().operand_minimum, tuning().endless.largest_number);
    }
    REQUIRE(gen.possible_question_count(13, CAP) == std::min<std::int64_t>(endless, CAP));
}

TEST_CASE("Indexes across a release-size kind give questions in range", "[unit][generator]")
{
    // Too many questions to list, so check the first, the last and a spread of others.
    for (const QuestionKind kind : ALL_KINDS)
    {
        const int m = 1;
        const int top = 1000;
        const std::int64_t count = question_kind_count(kind, m, top);
        REQUIRE(count > 0);
        Rng noise(static_cast<std::uint64_t>(kind) + 11);
        std::vector<std::int64_t> indexes{0, 1, count / 2, count - 2, count - 1};
        for (int i = 0; i < 3000; ++i)
        {
            indexes.push_back(static_cast<std::int64_t>(noise.below(static_cast<std::uint64_t>(count))));
        }
        for (const std::int64_t index : indexes)
        {
            const Question q = question_kind_at(kind, m, top, index);
            INFO("kind " << static_cast<int>(kind) << " index " << index);
            REQUIRE(is_kind_question(q, kind, m, top));
        }
    }
}

TEST_CASE("Every sum on level 3 is dealt with the same chance", "[property][generator]")
{
    // Level 3 mixes a + b (45 questions) and a - b (55 questions) up to 10, and the kind is chosen evenly, so the first
    // question is a given sum with a chance of 1 in 90 and a given take-away with 1 in 110. "1 + 1" used to be 1 in 18.
    // The seeds are fixed, so these counts do not change from run to run.
    constexpr int SEEDS = 20000;
    std::map<std::string, int> first_question;
    for (int seed = 1; seed <= SEEDS; ++seed)
    {
        const std::string text = describe(deal_level(3, static_cast<std::uint64_t>(seed), 1).front());
        ++first_question[text.substr(0, text.find(" ["))];
    }
    // About 222 each for the sums. The bounds are about five standard deviations either way.
    for (const char* sum : {"1 + 1 = ?", "5 + 5 = ?", "2 + 8 = ?", "9 + 1 = ?", "1 + 9 = ?"})
    {
        INFO(sum);
        REQUIRE(first_question[sum] >= 150);
        REQUIRE(first_question[sum] <= 300);
    }
    // About 182 each for the take-aways.
    for (const char* sum : {"10 - 1 = ?", "10 - 10 = ?", "5 - 3 = ?"})
    {
        INFO(sum);
        REQUIRE(first_question[sum] >= 120);
        REQUIRE(first_question[sum] <= 250);
    }
}

// ---- Tiles that give nothing away
// ------------------------------------------------------------------------------------

namespace
{

// How many of the tiles are below the answer: 0 when the answer is the smallest tile, 3 when it is the largest.
int rank_of_answer(const Question& q)
{
    return static_cast<int>(std::count_if(q.tiles.begin(), q.tiles.end(), [&](int tile) { return tile < q.answer; }));
}

} // namespace

TEST_CASE("The answer is as likely to be the smallest or the largest tile as a middle one", "[property][generator]")
{
    for (const int level : {12, 20})
    {
        std::array<int, 4> ranks{};
        int total = 0;
        for (std::uint64_t seed = 1; seed <= 400; ++seed)
        {
            for (const Question& q : deal_level(level, seed, 10))
            {
                // Only an answer with three numbers on each side in range can take every rank. Nearly all of them do.
                if (q.answer >= 3 && q.answer <= largest_number(level) - 3)
                {
                    ++ranks[static_cast<std::size_t>(rank_of_answer(q))];
                    ++total;
                }
            }
        }
        INFO("level " << level);
        REQUIRE(total > 3500);
        // Each rank has a chance of a quarter, so about 1,000 of 4,000 (a spread of 27). A rank under 700 or over 1,300
        // would be a bias a child could use. Before the change the two outer ranks were each about 1 in 20.
        for (const int count : ranks)
        {
            REQUIRE(count >= 700);
            REQUIRE(count <= 1300);
        }
    }
}

TEST_CASE("Near the ends of the range the answer takes only the ranks the range allows", "[property][generator]")
{
    for (const int level : {1, 2, 3, 4})
    {
        const int largest = largest_number(level);
        std::set<int> answers_seen;
        std::array<int, 4> ranks_at_one{};
        for (std::uint64_t seed = 1; seed <= 300; ++seed)
        {
            for (const Question& q : deal_level(level, seed, 10))
            {
                const int rank = rank_of_answer(q);
                answers_seen.insert(q.answer);
                REQUIRE(rank <= q.answer);               // no more numbers exist below the answer than its value
                REQUIRE(3 - rank <= largest - q.answer); // and above it, no more than the range holds
                if (q.answer == 0)
                {
                    REQUIRE(rank == 0); // nothing is below 0, so it is always the smallest tile
                }
                if (q.answer == largest)
                {
                    REQUIRE(rank == 3);
                }
                if (q.answer == 1)
                {
                    ++ranks_at_one[static_cast<std::size_t>(rank)];
                }
            }
        }
        INFO("level " << level);
        REQUIRE(answers_seen.contains(0) == (level != 1)); // level 1 only adds, so its smallest answer is 2
        if (level >= 2)
        {
            // An answer of 1 has one number below it, so 0 or 1 wrong tiles can sit below, each about half the time.
            REQUIRE(ranks_at_one[0] > 0);
            REQUIRE(ranks_at_one[1] > 0);
            REQUIRE(ranks_at_one[2] == 0);
            REQUIRE(ranks_at_one[3] == 0);
        }
    }
}
