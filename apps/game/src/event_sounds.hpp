// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/session.hpp"
#include "services/audio_service.hpp"

#include <vector>

namespace heart_boom_maths::game
{

/// The sounds one core event calls for, in the order they start: the feedback table of SPEC.md. The core knows nothing
/// about audio, so the table lives here.
///
///   correct answer        coin chime, and the rising tone too when the multiplier rose
///   wrong answer          soft buzz
///   hint used             whoosh
///   alert entered         ticking (the controller stops it when the timer climbs out or the level ends)
///   bomb goes off         cartoon pop
///   level cleared         fanfare. The win after level 12 sends GAME_WON as well and plays nothing more
///
/// An event with nothing to play (level and question starts) gives an empty list.
[[nodiscard]] std::vector<SoundId> sounds_for_event(const Event& event);

} // namespace heart_boom_maths::game
