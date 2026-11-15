// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/question_generator.hpp"

#include "heart_boom_maths/profile.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace heart_boom_maths
{

namespace
{

// A level with this many possible questions or fewer is listed in full, shuffled and dealt from the front. Larger
// levels redraw on a repeat. The number comes from the PRD, which states it as a rule and not as a tuning value.
constexpr int LIST_ALL_LIMIT = 50;

// A redraw has this many chances to avoid a repeat. Real levels have hundreds of possibilities against ten dealt, so
// the first or second draw is almost always new. The bound only keeps a badly tuned level from looping for ever.
constexpr int REDRAW_ATTEMPTS = 1000;

// A term is stored in 14 bits of a question key, so the level's largest number is capped here. Release values are far
// below it.
constexpr int KEY_TERM_BITS = 14;
constexpr int MAX_TERM = (1 << KEY_TERM_BITS) - 1;
constexpr int KEY_OPERATOR_SHIFT = 4 * KEY_TERM_BITS;
constexpr int KEY_SHAPE_SHIFT = KEY_OPERATOR_SHIFT + 3;

constexpr int SNAPSHOT_HEADER_SIZE = 3; // level, mode, count

constexpr std::array<QuestionKind, 6> ALL_KINDS{QuestionKind::ADD2, QuestionKind::SUB2,     QuestionKind::MISSING,
                                                QuestionKind::ADD3, QuestionKind::ADD_SUB3, QuestionKind::ADD_ADD_SUB4};

struct KindTraits
{
    QuestionShape shape;
    int term_count;
    std::array<Operator, 3> operators;
};

constexpr KindTraits traits_of(QuestionKind kind) noexcept
{
    switch (kind)
    {
    case QuestionKind::ADD2:
        return {QuestionShape::TWO_TERM, 2, {Operator::ADD, Operator::ADD, Operator::ADD}};
    case QuestionKind::SUB2:
        return {QuestionShape::TWO_TERM, 2, {Operator::SUBTRACT, Operator::ADD, Operator::ADD}};
    case QuestionKind::MISSING:
        return {QuestionShape::MISSING_ADDEND, 2, {Operator::ADD, Operator::ADD, Operator::ADD}};
    case QuestionKind::ADD3:
        return {QuestionShape::THREE_TERM, 3, {Operator::ADD, Operator::ADD, Operator::ADD}};
    case QuestionKind::ADD_SUB3:
        return {QuestionShape::THREE_TERM, 3, {Operator::ADD, Operator::SUBTRACT, Operator::ADD}};
    case QuestionKind::ADD_ADD_SUB4:
        return {QuestionShape::FOUR_TERM, 4, {Operator::ADD, Operator::ADD, Operator::SUBTRACT}};
    }
    return {QuestionShape::TWO_TERM, 2, {Operator::ADD, Operator::ADD, Operator::ADD}};
}

// What one level asks for: its number range and the kinds it deals.
struct LevelInfo
{
    int largest = 0;
    int minimum = 1;
    bool endless = false;
    int multi_term_percent = 0; // endless only
    std::array<QuestionKind, 2> kinds{};
    int kind_count = 0;
};

void set_kinds(LevelInfo& info, QuestionShape shape, Operations operations)
{
    switch (shape)
    {
    case QuestionShape::TWO_TERM:
        switch (operations)
        {
        case Operations::ADD:
            info.kinds = {QuestionKind::ADD2, QuestionKind::ADD2};
            info.kind_count = 1;
            break;
        case Operations::TAKE_AWAY:
            info.kinds = {QuestionKind::SUB2, QuestionKind::SUB2};
            info.kind_count = 1;
            break;
        case Operations::MIXED:
            info.kinds = {QuestionKind::ADD2, QuestionKind::SUB2};
            info.kind_count = 2;
            break;
        }
        break;
    case QuestionShape::MISSING_ADDEND:
        info.kinds = {QuestionKind::MISSING, QuestionKind::MISSING};
        info.kind_count = 1;
        break;
    case QuestionShape::THREE_TERM:
        // The PRD has one three-term sentence for adding and one for mixed. A take-away three-term level does not
        // exist, so it is read as mixed.
        info.kinds = {operations == Operations::ADD ? QuestionKind::ADD3 : QuestionKind::ADD_SUB3,
                      QuestionKind::ADD_SUB3};
        info.kind_count = 1;
        break;
    case QuestionShape::FOUR_TERM:
        info.kinds = {QuestionKind::ADD_ADD_SUB4, QuestionKind::ADD_ADD_SUB4};
        info.kind_count = 1;
        break;
    }
}

LevelInfo make_level_info(const Tuning& tuning, int level)
{
    LevelInfo info;
    // The floors keep every range below non-empty if a developer's tuning file is careless.
    info.minimum = std::max(tuning.operand_minimum, 1);
    const int shortest = 4 * info.minimum;

    level = std::max(level, 1);
    if (level <= tuning.campaign_levels)
    {
        const LevelSpec& spec = tuning.levels[static_cast<std::size_t>(level - 1)];
        info.largest = spec.largest_number;
        set_kinds(info, spec.shape, spec.operations);
    }
    else
    {
        const EndlessTuning& e = tuning.endless;
        info.endless = true;
        info.largest = e.largest_number;
        // Past the end of the ramp the share no longer changes, and capping k keeps its multiplication in range.
        const int k = std::clamp(level - e.first_level + 1, 1, e.multi_term_percent_span_k + 1);
        info.multi_term_percent = endless_multi_term_percent(e, k);
    }
    info.largest = std::clamp(info.largest, shortest, MAX_TERM);
    return info;
}

[[nodiscard]] bool level_allows(const LevelInfo& info, QuestionKind kind)
{
    if (info.endless)
    {
        return true;
    }
    for (int i = 0; i < info.kind_count; ++i)
    {
        if (info.kinds[static_cast<std::size_t>(i)] == kind)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool matches(const Question& q, const KindTraits& traits)
{
    if (q.shape != traits.shape || q.term_count != traits.term_count)
    {
        return false;
    }
    for (std::size_t i = 0; i < q.operators.size(); ++i)
    {
        // The operators after the last term are unused and always ADD, so that one sentence has one representation.
        const Operator expected = static_cast<int>(i) < traits.term_count - 1 ? traits.operators[i] : Operator::ADD;
        if (q.operators[i] != expected)
        {
            return false;
        }
    }
    return true;
}

// The value of the sentence without any range check: the running total, or c - a for a missing addend.
[[nodiscard]] int evaluate(const Question& q)
{
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        return q.terms[1] - q.terms[0];
    }
    int value = q.terms[0];
    for (int i = 1; i < q.term_count; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        value += q.operators[index - 1] == Operator::ADD ? q.terms[index] : -q.terms[index];
    }
    return value;
}

// True when the sentence is one this level can deal and every number in it is in range.
[[nodiscard]] bool is_valid(const LevelInfo& info, const Question& q)
{
    bool allowed = false;
    for (const QuestionKind kind : ALL_KINDS)
    {
        allowed = allowed || (level_allows(info, kind) && matches(q, traits_of(kind)));
    }
    if (!allowed)
    {
        return false;
    }

    for (std::size_t i = 0; i < q.terms.size(); ++i)
    {
        const bool used = static_cast<int>(i) < q.term_count;
        const int term = q.terms[i];
        if (used ? (term < info.minimum || term > info.largest) : term != 0)
        {
            return false;
        }
    }

    if (q.answer != evaluate(q))
    {
        return false;
    }
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        return q.answer >= info.minimum;
    }
    int running = q.terms[0];
    for (int i = 1; i < q.term_count; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        running += q.operators[index - 1] == Operator::ADD ? q.terms[index] : -q.terms[index];
        if (running < 0 || running > info.largest)
        {
            return false;
        }
    }
    return true;
}

// A question without its tiles, as one non-negative integer: four 14-bit terms, three operator bits and the shape.
// It is what the repeat check compares and what a saved run stores.
[[nodiscard]] std::int64_t encode_key(const Question& q)
{
    std::int64_t key = 0;
    for (std::size_t i = 0; i < q.terms.size(); ++i)
    {
        key |= static_cast<std::int64_t>(q.terms[i]) << (KEY_TERM_BITS * static_cast<int>(i));
    }
    for (std::size_t i = 0; i < q.operators.size(); ++i)
    {
        if (q.operators[i] == Operator::SUBTRACT)
        {
            key |= std::int64_t{1} << (KEY_OPERATOR_SHIFT + static_cast<int>(i));
        }
    }
    key |= static_cast<std::int64_t>(q.shape) << KEY_SHAPE_SHIFT;
    return key;
}

// The way back. Returns false for a number that is not a key, and the caller then checks the sentence with
// `is_valid`. Decoding and encoding again must give the same key, so a key has one meaning only.
[[nodiscard]] bool decode_key(std::int64_t key, Question& out)
{
    if (key < 0)
    {
        return false;
    }
    const std::int64_t shape = (key >> KEY_SHAPE_SHIFT) & 0x7;
    if ((key >> (KEY_SHAPE_SHIFT + 3)) != 0 || shape > static_cast<std::int64_t>(QuestionShape::FOUR_TERM))
    {
        return false;
    }

    Question q;
    q.shape = static_cast<QuestionShape>(shape);
    q.term_count = q.shape == QuestionShape::THREE_TERM ? 3 : (q.shape == QuestionShape::FOUR_TERM ? 4 : 2);
    for (std::size_t i = 0; i < q.terms.size(); ++i)
    {
        q.terms[i] = static_cast<int>((key >> (KEY_TERM_BITS * static_cast<int>(i))) & MAX_TERM);
    }
    for (std::size_t i = 0; i < q.operators.size(); ++i)
    {
        const bool used = static_cast<int>(i) < q.term_count - 1;
        const bool subtract = ((key >> (KEY_OPERATOR_SHIFT + static_cast<int>(i))) & 1) != 0;
        q.operators[i] = used && subtract ? Operator::SUBTRACT : Operator::ADD;
    }
    q.answer = evaluate(q);
    if (encode_key(q) != key)
    {
        return false;
    }
    out = q;
    return true;
}

[[nodiscard]] Question skeleton_of(QuestionKind kind)
{
    const KindTraits traits = traits_of(kind);
    Question q;
    q.shape = traits.shape;
    q.term_count = traits.term_count;
    q.operators = traits.operators;
    return q;
}

// Lists every question of one kind, in a fixed order, until `cap` questions are in `out`.
void walk_terms(const LevelInfo& info, int depth, int running, Question& q, std::size_t cap, std::vector<Question>& out)
{
    // The clamp tells the compiler the recursion never indexes past the four terms, which it cannot prove from
    // `term_count` alone once it inlines the recursion.
    const int count = std::min(q.term_count, static_cast<int>(q.terms.size()));
    if (depth >= count)
    {
        q.answer = running;
        out.push_back(q);
        return;
    }
    const bool adding = q.operators[static_cast<std::size_t>(depth - 1)] == Operator::ADD;
    for (int term = info.minimum; term <= info.largest && out.size() < cap; ++term)
    {
        const int value = adding ? running + term : running - term;
        if (adding ? value > info.largest : value < 0)
        {
            break; // a larger term only moves further out of range
        }
        q.terms[static_cast<std::size_t>(depth)] = term;
        walk_terms(info, depth + 1, value, q, cap, out);
    }
    q.terms[static_cast<std::size_t>(depth)] = 0;
}

void list_kind(QuestionKind kind, const LevelInfo& info, std::size_t cap, std::vector<Question>& out)
{
    Question q = skeleton_of(kind);
    if (kind == QuestionKind::MISSING)
    {
        for (int c = 2 * info.minimum; c <= info.largest && out.size() < cap; ++c)
        {
            for (int a = info.minimum; a <= c - info.minimum && out.size() < cap; ++a)
            {
                q.terms = {a, c, 0, 0};
                q.answer = c - a;
                out.push_back(q);
            }
        }
        return;
    }
    for (int first = info.minimum; first <= info.largest && out.size() < cap; ++first)
    {
        q.terms = {first, 0, 0, 0};
        walk_terms(info, 1, first, q, cap, out);
    }
}

// Every question the level can deal, up to `cap` of them.
[[nodiscard]] std::vector<Question> list_questions(const LevelInfo& info, std::size_t cap)
{
    std::vector<Question> out;
    for (const QuestionKind kind : ALL_KINDS)
    {
        if (level_allows(info, kind) && out.size() < cap)
        {
            list_kind(kind, info, cap, out);
        }
    }
    return out;
}

// How a kind is counted and built. Every kind is a number of added parts (`parts`) that sum to a total and, for the
// sentences with a take-away, one more number taken from that total. So a + b is two parts, a - b is one part (a) and
// a take-away, a + ? = c is two parts that sum to c, and a + b + c - d is three parts and a take-away. The total is the
// result for the sentences without a take-away and the sum before the take-away for the others.
struct DealShape
{
    int parts;
    bool take_away;
};

constexpr DealShape deal_shape_of(QuestionKind kind) noexcept
{
    switch (kind)
    {
    case QuestionKind::ADD2:
    case QuestionKind::MISSING:
        return {2, false};
    case QuestionKind::SUB2:
        return {1, true};
    case QuestionKind::ADD3:
        return {3, false};
    case QuestionKind::ADD_SUB3:
        return {2, true};
    case QuestionKind::ADD_ADD_SUB4:
        return {3, true};
    }
    return {2, false};
}

// The binomial coefficient for k from 1 to 4, each written out so the divisor is a constant (a division by a variable
// is too slow for a question dealt in under a microsecond). The product stays below 2^63 for n up to 16,386, the
// largest the callers pass: 16,386^4 is about 7.2 * 10^16 against 9.2 * 10^18.
[[nodiscard]] constexpr std::int64_t choose(std::int64_t n, int k) noexcept
{
    if (n < k)
    {
        return 0;
    }
    switch (k)
    {
    case 1:
        return n;
    case 2:
        return n * (n - 1) / 2;
    case 3:
        return n * (n - 1) * (n - 2) / 6;
    case 4:
        return n * (n - 1) * (n - 2) * (n - 3) / 24;
    default:
        return 0;
    }
}

// How many questions of a shape have a total of `at_most` or less. The number with one total does not depend on the
// largest number, so this is also the count for a level whose largest number is `at_most`.
//
// A total of s split into p parts of at least m each has C(n + p - 1, p - 1) ways, where n = s - p * m, and a
// take-away has s - m + 1 choices on top (any number from m up to the total). Adding over every total up to the limit
// gives C(N + p, p) for the parts alone, by the hockey-stick identity. With the take-away, write s - m + 1 as n + K
// with K = (p - 1) * m + 1 and use n * C(n + p - 1, p - 1) = p * C(n + p - 1, p), which gives
// p * C(N + p, p + 1) + K * C(N + p, p). A test checks both against a plain enumeration.
//
// Size: the largest number is at most MAX_TERM = 16,383, so N + p is at most 16,386. The largest term here is
// p * C(N + p, p + 1) for three parts, about 3 * 3 * 10^15, and K * C(N + p, p) is smaller. The count is therefore
// below 10^16 and fits in 64 bits with room to spare. At the release largest number of 1,000 the four-term kind has
// about 1.25 * 10^11 questions.
[[nodiscard]] constexpr std::int64_t count_up_to(DealShape shape, int minimum, int at_most) noexcept
{
    const std::int64_t n = static_cast<std::int64_t>(at_most) - static_cast<std::int64_t>(shape.parts) * minimum;
    if (n < 0)
    {
        return 0;
    }
    const std::int64_t ways = choose(n + shape.parts, shape.parts);
    if (!shape.take_away)
    {
        return ways;
    }
    const std::int64_t k = static_cast<std::int64_t>(shape.parts - 1) * minimum + 1;
    return shape.parts * choose(n + shape.parts, shape.parts + 1) + k * ways;
}

// The smallest value in [low, high] for which `reached` is true, given that it is true at `high` and, once true, stays
// true. It replaces a walk over up to a thousand totals with about ten steps.
template <typename Reached>
[[nodiscard]] int first_reached(int low, int high, Reached reached)
{
    while (low < high)
    {
        const int middle = low + (high - low) / 2;
        if (reached(middle))
        {
            high = middle;
        }
        else
        {
            low = middle + 1;
        }
    }
    return low;
}

// Splits `total` into three parts of at least `minimum`. Every such split has one index: first parts in increasing
// order and, for each first part, second parts in increasing order. So an even choice of index is an even choice among
// all the splits, not first part then second.
void split_three(int total, int minimum, std::int64_t index, std::array<int, 3>& parts)
{
    const std::int64_t spare = static_cast<std::int64_t>(total) - 3 * minimum;
    const std::int64_t all = choose(spare + 2, 2);
    // The splits with a first part of minimum + j - 1 or less number all - C(spare - j + 2, 2).
    const auto before = [&](std::int64_t j) { return all - choose(spare - j + 2, 2); };
    const int first = first_reached(0, static_cast<int>(spare), [&](int j) { return before(j + 1) > index; });
    parts[0] = minimum + first;
    parts[1] = minimum + static_cast<int>(index - before(first));
    parts[2] = total - parts[0] - parts[1];
}

// Builds one question with every question of the kind equally likely: one draw picks an index among them all. That is
// the same as choosing the total with a chance in proportion to the questions it leads to and then one of those with
// equal chance. No retry is needed, because every index is a question in range.
[[nodiscard]] Question build(QuestionKind kind, const LevelInfo& info, Rng& rng)
{
    const std::int64_t count = question_kind_count(kind, info.minimum, info.largest);
    const auto index = static_cast<std::int64_t>(rng.below(static_cast<std::uint64_t>(count)));
    return question_kind_at(kind, info.minimum, info.largest, index);
}

[[nodiscard]] QuestionKind pick_kind(const LevelInfo& info, Rng& rng)
{
    if (!info.endless)
    {
        return info.kind_count == 1
                   ? info.kinds[0]
                   : info.kinds[static_cast<std::size_t>(rng.below(static_cast<std::uint64_t>(info.kind_count)))];
    }

    // Endless: the multi-term share decides the group, then each group splits evenly (decision G5).
    if (static_cast<int>(rng.below(100)) < info.multi_term_percent)
    {
        if (rng.below(2) == 0)
        {
            return rng.below(2) == 0 ? QuestionKind::ADD3 : QuestionKind::ADD_SUB3;
        }
        return QuestionKind::ADD_ADD_SUB4;
    }
    if (rng.below(2) == 0)
    {
        return QuestionKind::MISSING;
    }
    return rng.below(2) == 0 ? QuestionKind::ADD2 : QuestionKind::SUB2;
}

// 37 becomes 73 and 123 becomes 132: the last two digits change places, a mistake children really make.
[[nodiscard]] int swap_last_two_digits(int value)
{
    if (value < 10)
    {
        return value;
    }
    const int tens = (value / 10) % 10;
    const int units = value % 10;
    return value - tens * 10 - units + units * 10 + tens;
}

// A side of the answer: the near misses that lie on it, at most four (the three offsets and the digit swap).
struct NearMisses
{
    std::array<int, 4> values{};
    int count = 0;
};

// Chooses `need` wrong tiles on one side of the answer and writes them to `out`. Near misses come first, picked at
// random when there are more than needed; a side with too few is topped up with the nearest unused numbers, which are
// at distance 1, 2, 3 and so on. `step` is -1 for the side below the answer and 1 for the side above it.
void deal_side(NearMisses& near, int need, int answer, int step, int largest, Rng& rng, std::span<int> out)
{
    if (near.count > need)
    {
        for (int i = 0; i < need; ++i)
        {
            const auto j = static_cast<int>(rng.below(static_cast<std::uint64_t>(near.count - i))) + i;
            std::swap(near.values[static_cast<std::size_t>(i)], near.values[static_cast<std::size_t>(j)]);
        }
        near.count = need;
    }
    int filled = 0;
    for (int i = 0; i < near.count; ++i)
    {
        out[static_cast<std::size_t>(filled++)] = near.values[static_cast<std::size_t>(i)];
    }
    for (int distance = 1; filled < need; ++distance)
    {
        const int value = answer + step * distance;
        if (value < 0 || value > largest)
        {
            break; // the caller only asks for as many as the range holds, so this is not reached
        }
        const auto used = out.first(static_cast<std::size_t>(filled));
        if (std::find(used.begin(), used.end(), value) == used.end())
        {
            out[static_cast<std::size_t>(filled++)] = value;
        }
    }
}

// Fills in the four tiles: the answer and three near misses, in a random order. First the number of wrong tiles below
// the answer is chosen, with equal chance among the counts the range allows, so the answer is as likely to be the
// smallest or the largest tile as a middle one. Choosing three near misses at random instead would, on small numbers,
// nearly always give a mix of -2, -1, +1 and +2, and the answer would almost always be one of the two middle tiles.
void deal_tiles(Question& q, const LevelInfo& info, Rng& rng)
{
    NearMisses below;
    NearMisses above;
    const auto offer = [&](int value)
    {
        if (value < 0 || value > info.largest || value == q.answer)
        {
            return;
        }
        NearMisses& side = value < q.answer ? below : above;
        const auto end = side.values.begin() + side.count;
        if (std::find(side.values.begin(), end, value) == end)
        {
            side.values[static_cast<std::size_t>(side.count++)] = value;
        }
    };
    offer(q.answer + 1);
    offer(q.answer - 1);
    offer(q.answer + 2);
    offer(q.answer - 2);
    offer(q.answer + 10);
    offer(q.answer - 10);
    offer(swap_last_two_digits(q.answer));

    // A count of wrong tiles below is allowed when that many numbers exist below the answer and the rest exist above.
    std::array<int, 4> allowed{};
    int allowed_count = 0;
    for (int count_below = 0; count_below <= 3; ++count_below)
    {
        if (count_below <= q.answer && 3 - count_below <= info.largest - q.answer)
        {
            allowed[static_cast<std::size_t>(allowed_count++)] = count_below;
        }
    }
    const int count_below =
        allowed[static_cast<std::size_t>(rng.below(static_cast<std::uint64_t>(std::max(allowed_count, 1))))];

    std::array<int, 3> wrong{};
    const std::span<int> wrong_span(wrong);
    deal_side(below, count_below, q.answer, -1, info.largest, rng,
              wrong_span.first(static_cast<std::size_t>(count_below)));
    deal_side(above, 3 - count_below, q.answer, 1, info.largest, rng,
              wrong_span.subspan(static_cast<std::size_t>(count_below)));

    q.tiles = {q.answer, wrong[0], wrong[1], wrong[2]};
    rng.shuffle(std::span<int>(q.tiles));
    q.correct_tile = static_cast<int>(std::find(q.tiles.begin(), q.tiles.end(), q.answer) - q.tiles.begin());
}

} // namespace

std::int64_t question_kind_count(QuestionKind kind, int minimum, int largest) noexcept
{
    return count_up_to(deal_shape_of(kind), std::max(minimum, 1), std::min(largest, MAX_TERM));
}

Question question_kind_at(QuestionKind kind, int minimum, int largest, std::int64_t index) noexcept
{
    const DealShape shape = deal_shape_of(kind);
    const int m = std::max(minimum, 1);
    const int top = std::min(largest, MAX_TERM);
    const int first_total = shape.parts * m;

    // The total whose questions hold this index: the first one where the questions up to it outnumber the index.
    const int total =
        first_reached(first_total, std::max(top, first_total), [&](int s) { return count_up_to(shape, m, s) > index; });
    std::int64_t rest = index - count_up_to(shape, m, total - 1);

    int taken = 0;
    if (shape.take_away)
    {
        const std::int64_t choices = static_cast<std::int64_t>(total) - m + 1;
        taken = m + static_cast<int>(rest % choices);
        rest /= choices;
    }
    std::array<int, 3> parts{};
    switch (shape.parts)
    {
    case 1:
        parts[0] = total;
        break;
    case 2:
        parts[0] = m + static_cast<int>(rest);
        parts[1] = total - parts[0];
        break;
    default:
        split_three(total, m, rest, parts);
        break;
    }

    Question q = skeleton_of(kind);
    switch (kind)
    {
    case QuestionKind::ADD2:
        q.terms = {parts[0], parts[1], 0, 0};
        break;
    case QuestionKind::SUB2:
        q.terms = {total, taken, 0, 0};
        break;
    case QuestionKind::MISSING:
        q.terms = {parts[0], total, 0, 0}; // a + ? = c, where the parts are a and the missing number
        break;
    case QuestionKind::ADD3:
        q.terms = {parts[0], parts[1], parts[2], 0};
        break;
    case QuestionKind::ADD_SUB3:
        q.terms = {parts[0], parts[1], taken, 0};
        break;
    case QuestionKind::ADD_ADD_SUB4:
        q.terms = {parts[0], parts[1], parts[2], taken};
        break;
    }
    q.answer = evaluate(q);
    return q;
}

std::vector<Question> list_question_kind(QuestionKind kind, int minimum, int largest)
{
    LevelInfo info;
    info.minimum = std::max(minimum, 1);
    info.largest = std::min(largest, MAX_TERM);
    std::vector<Question> out;
    list_kind(kind, info, std::numeric_limits<std::size_t>::max(), out);
    return out;
}

QuestionGenerator::QuestionGenerator(const Tuning& tuning) : m_Tuning(&tuning) {}

int QuestionGenerator::possible_question_count(int level, int cap) const
{
    const LevelInfo info = make_level_info(*m_Tuning, level);
    return static_cast<int>(list_questions(info, static_cast<std::size_t>(std::max(cap, 0))).size());
}

void QuestionGenerator::begin_level(int level, Rng& rng)
{
    HBM_ZONE("generator.begin_level");
    m_Level = std::max(level, 1);
    m_Keys.clear();
    m_DeckPos = 0;

    const LevelInfo info = make_level_info(*m_Tuning, m_Level);
    const std::vector<Question> all = list_questions(info, LIST_ALL_LIMIT + 1);
    if (all.size() > static_cast<std::size_t>(LIST_ALL_LIMIT))
    {
        m_Mode = Mode::REDRAW;
        return;
    }

    m_Mode = Mode::DECK;
    for (const Question& q : all)
    {
        m_Keys.push_back(encode_key(q));
    }
    rng.shuffle(std::span<std::int64_t>(m_Keys));
}

Question QuestionGenerator::next(Rng& rng)
{
    HBM_ZONE("generator.next");
    if (m_Mode == Mode::NOT_STARTED)
    {
        begin_level(m_Level, rng);
    }
    const LevelInfo info = make_level_info(*m_Tuning, m_Level);

    Question q;
    if (m_Mode == Mode::DECK)
    {
        if (m_DeckPos >= m_Keys.size())
        {
            // More questions were asked than the level has. Deal the whole list again in a new order.
            begin_level(m_Level, rng);
        }
        const bool decoded = decode_key(m_Keys[m_DeckPos++], q);
        (void)decoded; // the keys were encoded from valid questions, so this cannot fail
    }
    else
    {
        for (int attempt = 0; attempt < REDRAW_ATTEMPTS; ++attempt)
        {
            q = build(pick_kind(info, rng), info, rng);
            if (std::find(m_Keys.begin(), m_Keys.end(), encode_key(q)) == m_Keys.end())
            {
                break;
            }
        }
        m_Keys.push_back(encode_key(q));
    }

    deal_tiles(q, info, rng);
    return q;
}

std::vector<std::int64_t> QuestionGenerator::snapshot() const
{
    HBM_ZONE("generator.snapshot");
    // The dealt questions (redraw) or the questions still to deal (deck) are everything that decides the next question
    // besides the random generator, whose state the session saves.
    const auto first = m_Mode == Mode::DECK ? m_Keys.begin() + static_cast<std::ptrdiff_t>(m_DeckPos) : m_Keys.begin();
    const auto count = static_cast<std::int64_t>(m_Keys.end() - first);

    std::vector<std::int64_t> values;
    values.reserve(SNAPSHOT_HEADER_SIZE + static_cast<std::size_t>(count));
    values.push_back(m_Level);
    values.push_back(static_cast<std::int64_t>(m_Mode));
    values.push_back(count);
    values.insert(values.end(), first, m_Keys.end());
    return values;
}

bool QuestionGenerator::restore(std::span<const std::int64_t> values)
{
    if (values.size() < static_cast<std::size_t>(SNAPSHOT_HEADER_SIZE))
    {
        return false;
    }
    const std::int64_t level = values[0];
    const std::int64_t mode = values[1];
    const std::int64_t count = values[2];
    if (level < 1 || level > std::numeric_limits<int>::max() || mode < 0 ||
        mode > static_cast<std::int64_t>(Mode::DECK))
    {
        return false;
    }
    if (count < 0 || static_cast<std::uint64_t>(count) != values.size() - SNAPSHOT_HEADER_SIZE)
    {
        return false;
    }

    const auto new_mode = static_cast<Mode>(mode);
    const std::vector<std::int64_t> keys(values.begin() + SNAPSHOT_HEADER_SIZE, values.end());

    if (new_mode == Mode::NOT_STARTED)
    {
        // A generator that has not begun a level has dealt nothing and is at level 1.
        if (level != 1 || !keys.empty())
        {
            return false;
        }
    }
    else
    {
        const LevelInfo info = make_level_info(*m_Tuning, static_cast<int>(level));
        const bool lists_all =
            list_questions(info, LIST_ALL_LIMIT + 1).size() <= static_cast<std::size_t>(LIST_ALL_LIMIT);
        if ((new_mode == Mode::DECK) != lists_all)
        {
            return false;
        }
        for (const std::int64_t key : keys)
        {
            Question q;
            if (!decode_key(key, q) || !is_valid(info, q))
            {
                return false;
            }
        }
        std::vector<std::int64_t> sorted = keys;
        std::sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
        {
            return false; // a question can only be dealt once per level
        }
    }

    m_Level = static_cast<int>(level);
    m_Mode = new_mode;
    m_Keys = keys;
    m_DeckPos = 0;
    return true;
}

} // namespace heart_boom_maths
