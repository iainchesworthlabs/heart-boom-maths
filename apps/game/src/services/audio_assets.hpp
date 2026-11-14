// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_service.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace heart_boom_maths::game
{

/// A sound file held in the application resources (apps/game/audio, made by tools/assets/make_placeholder_sounds.py
/// until a sound designer supplies the real ones).
struct AudioClip
{
    std::string_view file;
    bool looping;
    int volume_percent;
};

/// The resource directory of the sound files. A resource, never a path on disk or an address on a network.
inline constexpr std::string_view AUDIO_RESOURCE_PREFIX = ":/heart_boom_maths/audio/";

/// The clip of an effect or a music bed. Empty for NONE and for a value that is no member of the enum, so a caller
/// never needs to check an id before it plays one.
[[nodiscard]] std::optional<AudioClip> clip_for(SoundId id);
[[nodiscard]] std::optional<AudioClip> clip_for(MusicId id);

[[nodiscard]] std::vector<SoundId> all_sounds();
[[nodiscard]] std::vector<MusicId> all_music();

} // namespace heart_boom_maths::game
