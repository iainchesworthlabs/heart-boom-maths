// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/haptic_backend.hpp"

#include <memory>

namespace heart_boom_maths::game::platform
{

/// The haptic backend of this operating system: UIKit's feedback generators on iOS, View.performHapticFeedback on
/// Android, `navigator.vibrate` in a browser, and NullHapticBackend on the desktop. Like platform.hpp, every directory
/// under src/platform/<os>/ defines it and CMake compiles the one that matches the target. Each one only translates a
/// beat; whether and when one plays is BackedHapticsService's.
[[nodiscard]] std::unique_ptr<HapticBackend> make_haptic_backend();

} // namespace heart_boom_maths::game::platform
