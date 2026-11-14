// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/tuning.hpp"

#include <string>

namespace heart_boom_maths::game
{

/// What a level asks, in a few words a child can read: "add three numbers", "take away, up to 20". It is read from the
/// campaign table in the tuning, so a balance change reaches the words without a code change. Levels past the campaign
/// are the endless ones.
[[nodiscard]] std::string level_blurb(const Tuning& tuning, int level);

/// The line under NEXT LEVEL on the level-clear card: "Next up: level 6, add three numbers".
[[nodiscard]] std::string next_up_text(const Tuning& tuning, int next_level);

} // namespace heart_boom_maths::game
