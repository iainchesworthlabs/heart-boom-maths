// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/question.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace heart_boom_maths
{

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

private:
    const Tuning* m_Tuning;
    int m_Level = 1;
};

} // namespace heart_boom_maths
