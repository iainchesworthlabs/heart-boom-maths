// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/tuning.hpp"

#include "heart_boom_maths/tuning_data.hpp"

namespace heart_boom_maths
{

const Tuning& Tuning::defaults() noexcept
{
    return generated::TUNING_DEFAULTS;
}

} // namespace heart_boom_maths
