// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/score_service.hpp"

#include <memory>

namespace heart_boom_maths::game::platform
{

/// The score service of this operating system: the Game Center one on iOS, NullScoreService everywhere else. Like
/// platform.hpp, every directory under src/platform/<os>/ defines it and CMake compiles the one that matches the
/// target.
[[nodiscard]] std::unique_ptr<ScoreService> make_score_service();

} // namespace heart_boom_maths::game::platform
