// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/question.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace heart_boom_maths
{

/// The kinds of number sentence the generator builds. The shape names in `Tuning` cover several kinds each (the
/// two-term shape is both a + b and a - b), so the generator works with kinds.
enum class QuestionKind
{
    ADD2,         // a + b
    SUB2,         // a - b
    MISSING,      // a + ? = c
    ADD3,         // a + b + c
    ADD_SUB3,     // a + b - c
    ADD_ADD_SUB4, // a + b + c - d
};

/// Every question of one kind has exactly one index in [0, `question_kind_count`). The generator draws an index with
/// equal chance, so every question of the kind is dealt with equal chance. A question here is what a level with these
/// numbers can deal: every operand from `minimum` to `largest`, every running total from 0 to `largest`, and for
/// a + ? = c a missing number of at least `minimum`. `largest` is capped at 16,383, the most a question key holds.
/// The tiles of the returned question are not dealt (all zero).
[[nodiscard]] std::int64_t question_kind_count(QuestionKind kind, int minimum, int largest) noexcept;

/// The question at `index`, which must be in [0, `question_kind_count`).
[[nodiscard]] Question question_kind_at(QuestionKind kind, int minimum, int largest, std::int64_t index) noexcept;

/// Every question of the kind, by plain enumeration and in a fixed order. It is the reference the two functions above
/// are checked against, and the generator lists a small level with it. Use it only for small ranges.
[[nodiscard]] std::vector<Question> list_question_kind(QuestionKind kind, int minimum, int largest);

/// Deals the questions of one level at a time (see "Question generator" in docs/game-architecture.md). It keeps what it
/// needs to avoid repeats within a level, and that state is part of a saved run.
class QuestionGenerator
{
public:
    explicit QuestionGenerator(const Tuning& tuning);

    /// Starts a level (1 to 12 from the level table, 13 and above endless) and forgets the previous level's questions.
    void begin_level(int level, Rng& rng);

    /// The next question of the current level.
    [[nodiscard]] Question next(Rng& rng);

    /// The repeat-avoidance state as integers, and the way back. `restore` returns false and changes nothing when the
    /// values are not a state this generator could have produced.
    [[nodiscard]] std::vector<std::int64_t> snapshot() const;
    [[nodiscard]] bool restore(std::span<const std::int64_t> values);

    /// How many different questions a level can deal, counted up to `cap` (a level with more returns `cap`). The
    /// generator lists and shuffles a level when this is 50 or fewer, and redraws on a repeat otherwise.
    [[nodiscard]] int possible_question_count(int level, int cap = 51) const;

private:
    // NOT_STARTED: no level yet. REDRAW: `m_Keys` holds the questions dealt so far. DECK: `m_Keys` is a shuffled list
    // of every possible question and `m_DeckPos` is the next one to deal.
    enum class Mode
    {
        NOT_STARTED,
        REDRAW,
        DECK,
    };

    const Tuning* m_Tuning;
    int m_Level = 1;
    Mode m_Mode = Mode::NOT_STARTED;
    std::vector<std::int64_t> m_Keys;
    std::size_t m_DeckPos = 0;
};

} // namespace heart_boom_maths
