// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/score_platform.hpp"

namespace heart_boom_maths::game::platform
{

std::unique_ptr<ScoreService> make_score_service()
{
    return std::make_unique<NullScoreService>();
}

} // namespace heart_boom_maths::game::platform
