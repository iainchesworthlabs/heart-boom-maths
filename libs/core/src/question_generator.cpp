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

// The kinds of number sentence the generator can build. The shape names in `Tuning` cover several kinds each (the
// two-term shape is both a + b and a - b), so the generator works with kinds.
enum class Kind
{
    ADD2,         // a + b
    SUB2,         // a - b
    MISSING,      // a + ? = c
    ADD3,         // a + b + c
    ADD_SUB3,     // a + b - c
    ADD_ADD_SUB4, // a + b + c - d
};

constexpr std::array<Kind, 6> ALL_KINDS{Kind::ADD2, Kind::SUB2,     Kind::MISSING,
                                        Kind::ADD3, Kind::ADD_SUB3, Kind::ADD_ADD_SUB4};

struct KindTraits
{
    QuestionShape shape;
    int term_count;
    std::array<Operator, 3> operators;
};

constexpr KindTraits traits_of(Kind kind) noexcept
{
    switch (kind)
    {
    case Kind::ADD2:
        return {QuestionShape::TWO_TERM, 2, {Operator::ADD, Operator::ADD, Operator::ADD}};
    case Kind::SUB2:
        return {QuestionShape::TWO_TERM, 2, {Operator::SUBTRACT, Operator::ADD, Operator::ADD}};
    case Kind::MISSING:
        return {QuestionShape::MISSING_ADDEND, 2, {Operator::ADD, Operator::ADD, Operator::ADD}};
    case Kind::ADD3:
        return {QuestionShape::THREE_TERM, 3, {Operator::ADD, Operator::ADD, Operator::ADD}};
    case Kind::ADD_SUB3:
        return {QuestionShape::THREE_TERM, 3, {Operator::ADD, Operator::SUBTRACT, Operator::ADD}};
    case Kind::ADD_ADD_SUB4:
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
    std::array<Kind, 2> kinds{};
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
            info.kinds = {Kind::ADD2, Kind::ADD2};
            info.kind_count = 1;
            break;
        case Operations::TAKE_AWAY:
            info.kinds = {Kind::SUB2, Kind::SUB2};
            info.kind_count = 1;
            break;
        case Operations::MIXED:
            info.kinds = {Kind::ADD2, Kind::SUB2};
            info.kind_count = 2;
            break;
        }
        break;
    case QuestionShape::MISSING_ADDEND:
        info.kinds = {Kind::MISSING, Kind::MISSING};
        info.kind_count = 1;
        break;
    case QuestionShape::THREE_TERM:
        // The PRD has one three-term sentence for adding and one for mixed. A take-away three-term level does not
        // exist, so it is read as mixed.
        info.kinds = {operations == Operations::ADD ? Kind::ADD3 : Kind::ADD_SUB3, Kind::ADD_SUB3};
        info.kind_count = 1;
        break;
    case QuestionShape::FOUR_TERM:
        info.kinds = {Kind::ADD_ADD_SUB4, Kind::ADD_ADD_SUB4};
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

[[nodiscard]] bool level_allows(const LevelInfo& info, Kind kind)
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
    for (const Kind kind : ALL_KINDS)
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

[[nodiscard]] Question skeleton_of(Kind kind)
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

void list_kind(Kind kind, const LevelInfo& info, std::size_t cap, std::vector<Question>& out)
{
    Question q = skeleton_of(kind);
    if (kind == Kind::MISSING)
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
    for (const Kind kind : ALL_KINDS)
    {
        if (level_allows(info, kind) && out.size() < cap)
        {
            list_kind(kind, info, cap, out);
        }
    }
    return out;
}

// Splits `total` into `parts` numbers that are each at least `minimum`. `parts` is 2 or 3.
void split(int total, int parts, int minimum, Rng& rng, std::span<int> out)
{
    int left = total;
    for (int i = 0; i < parts - 1; ++i)
    {
        const int remaining_parts = parts - 1 - i;
        const auto slot = static_cast<std::size_t>(i);
        out[slot] = rng.between(minimum, left - remaining_parts * minimum);
        left -= out[slot];
    }
    out[static_cast<std::size_t>(parts - 1)] = left;
}

// Builds a question from its result backwards: pick the result in range, then split it, so every number comes out in
// range at the first try.
[[nodiscard]] Question build(Kind kind, const LevelInfo& info, Rng& rng)
{
    const int m = info.minimum;
    const int largest = info.largest;
    Question q = skeleton_of(kind);
    std::array<int, 3> parts{};

    switch (kind)
    {
    case Kind::ADD2:
    {
        const int result = rng.between(2 * m, largest);
        split(result, 2, m, rng, parts);
        q.terms = {parts[0], parts[1], 0, 0};
        break;
    }
    case Kind::SUB2:
    {
        // a - b = r. Choose r, then b so that a = r + b stays within the largest number.
        const int result = rng.between(0, largest - m);
        const int b = rng.between(m, largest - result);
        q.terms = {result + b, b, 0, 0};
        break;
    }
    case Kind::MISSING:
    {
        // a + ? = c. The sum c is chosen first and the missing number is c - a.
        const int sum = rng.between(2 * m, largest);
        const int a = rng.between(m, sum - m);
        q.terms = {a, sum, 0, 0};
        break;
    }
    case Kind::ADD3:
    {
        const int result = rng.between(3 * m, largest);
        split(result, 3, m, rng, parts);
        q.terms = {parts[0], parts[1], parts[2], 0};
        break;
    }
    case Kind::ADD_SUB3:
    {
        // a + b - c = r. The sum a + b = r + c must be at least 2m and at most the largest number.
        const int result = rng.between(0, largest - m);
        const int c = rng.between(std::max(m, 2 * m - result), largest - result);
        split(result + c, 2, m, rng, parts);
        q.terms = {parts[0], parts[1], c, 0};
        break;
    }
    case Kind::ADD_ADD_SUB4:
    {
        const int result = rng.between(0, largest - m);
        const int d = rng.between(std::max(m, 3 * m - result), largest - result);
        split(result + d, 3, m, rng, parts);
        q.terms = {parts[0], parts[1], parts[2], d};
        break;
    }
    }
    q.answer = evaluate(q);
    return q;
}

[[nodiscard]] Kind pick_kind(const LevelInfo& info, Rng& rng)
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
            return rng.below(2) == 0 ? Kind::ADD3 : Kind::ADD_SUB3;
        }
        return Kind::ADD_ADD_SUB4;
    }
    if (rng.below(2) == 0)
    {
        return Kind::MISSING;
    }
    return rng.below(2) == 0 ? Kind::ADD2 : Kind::SUB2;
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

// Fills in the four tiles: the answer and three near misses, in a random order.
void deal_tiles(Question& q, const LevelInfo& info, Rng& rng)
{
    std::array<int, 7> candidates{};
    int count = 0;
    const auto offer = [&](int value)
    {
        if (value < 0 || value > info.largest || value == q.answer)
        {
            return;
        }
        for (int i = 0; i < count; ++i)
        {
            if (candidates[static_cast<std::size_t>(i)] == value)
            {
                return;
            }
        }
        candidates[static_cast<std::size_t>(count++)] = value;
    };
    offer(q.answer + 1);
    offer(q.answer - 1);
    offer(q.answer + 2);
    offer(q.answer - 2);
    offer(q.answer + 10);
    offer(q.answer - 10);
    offer(swap_last_two_digits(q.answer));

    if (count > 3)
    {
        // Keep three of the near misses at random.
        for (int i = 0; i < 3; ++i)
        {
            const auto j = static_cast<int>(rng.below(static_cast<std::uint64_t>(count - i))) + i;
            std::swap(candidates[static_cast<std::size_t>(i)], candidates[static_cast<std::size_t>(j)]);
        }
        count = 3;
    }
    // Too few near misses in range: take the nearest unused numbers, the lower one first.
    // A while loop, because `offer` changes `count` through a reference and Clang cannot see that in a for header.
    int distance = 1;
    while (count < 3)
    {
        offer(q.answer - distance);
        // cppcheck-suppress identicalInnerCondition ; offer() changes count through a reference
        if (count < 3)
        {
            offer(q.answer + distance);
        }
        ++distance;
    }

    q.tiles = {q.answer, candidates[0], candidates[1], candidates[2]};
    rng.shuffle(std::span<int>(q.tiles));
    q.correct_tile = static_cast<int>(std::find(q.tiles.begin(), q.tiles.end(), q.answer) - q.tiles.begin());
}

} // namespace

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
