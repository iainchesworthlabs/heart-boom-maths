// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/session.hpp"
#include "services/haptics_service.hpp"

#include <optional>
#include <span>

namespace heart_boom_maths::game
{

/// The haptic one core event calls for. Like event_sounds, a table of the app layer: the core knows nothing about
/// touch feedback.
///
///   correct answer        RIGHT, or MULTIPLIER_UP when the multiplier rose (one bump, not two)
///   wrong answer          WRONG
///   hint used             HINT
///   alert entered         ALERT, once: the ticking that goes on is not felt
///   level cleared         LEVEL_CLEAR
///   game won              WIN
///   bomb goes off         BOOM (a quit or a finish from the win screen is nothing to feel)
///
/// Level and question starts, and every tick, give nothing.
[[nodiscard]] std::optional<HapticId> haptic_for_event(const Event& event) noexcept;

/// The one haptic for what a single command or tick produced: the strongest of its events (haptic_priority). The last
/// answer of a level sends a right answer and the level clear together, and the answer that ends the game sends the
/// bomb; the child feels the result, not both.
[[nodiscard]] std::optional<HapticId> haptic_for_events(std::span<const Event> events) noexcept;

} // namespace heart_boom_maths::game
