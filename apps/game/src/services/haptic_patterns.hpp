// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/haptics_service.hpp"

#include <span>

namespace heart_boom_maths::game
{

/// A beat of a haptic and when it starts, counted from the start of the haptic.
struct HapticBeat
{
    int delay_ms = 0;
    HapticId id = HapticId::TAP;
};

/// The beats a haptic is made of. Every id is one beat except WIN, which is three small ones about a tenth of a second
/// apart: light, medium, then the success feel of a cleared level. An app-layer feel, not a game rule: to be judged on
/// a device and adjusted there.
[[nodiscard]] std::span<const HapticBeat> haptic_beats(HapticId id) noexcept;

/// Which of two cues that want the same moment is felt: the higher. A wrong answer outranks the alert it may cause
/// (the tap was the child's), and the end of a level or the game outranks the answer that ended it.
[[nodiscard]] int haptic_priority(HapticId id) noexcept;

} // namespace heart_boom_maths::game
