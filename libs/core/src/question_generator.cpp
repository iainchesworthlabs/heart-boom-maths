// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/question_generator.hpp"

namespace heart_boom_maths
{

// A placeholder that always deals "1 + 1 = ?" so that the session can be built against the interface. The generator
// work replaces this file.

QuestionGenerator::QuestionGenerator(const Tuning& tuning) : m_Tuning(&tuning) {}

void QuestionGenerator::begin_level(int level, Rng&)
{
    m_Level = level;
}

Question QuestionGenerator::next(Rng&)
{
    Question q;
    q.terms = {1, 1, 0, 0};
    q.operators = {Operator::ADD, Operator::ADD, Operator::ADD};
    q.answer = 2;
    q.tiles = {2, 3, 4, 5};
    q.correct_tile = 0;
    return q;
}

std::vector<std::int64_t> QuestionGenerator::snapshot() const
{
    return {};
}

bool QuestionGenerator::restore(std::span<const std::int64_t> values)
{
    return values.empty();
}

} // namespace heart_boom_maths
