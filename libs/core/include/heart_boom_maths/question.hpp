// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/tuning.hpp"

#include <array>

namespace heart_boom_maths
{

enum class Operator
{
    ADD,
    SUBTRACT,
};

/// One number sentence and its four answer tiles.
///
/// `terms[0 .. term_count)` are the numbers shown, with `operators[i]` between `terms[i]` and `terms[i + 1]`. For
/// TWO_TERM, THREE_TERM and FOUR_TERM the sentence reads "t0 op t1 op t2 ... = ?" and `answer` is its value. For
/// MISSING_ADDEND it reads "terms[0] + ? = terms[1]" and `answer` is the missing number.
struct Question
{
    QuestionShape shape = QuestionShape::TWO_TERM;
    int term_count = 2;
    std::array<int, 4> terms{};
    std::array<Operator, 3> operators{};
    int answer = 0;
    std::array<int, 4> tiles{};
    int correct_tile = 0; // index into `tiles`
};

} // namespace heart_boom_maths
