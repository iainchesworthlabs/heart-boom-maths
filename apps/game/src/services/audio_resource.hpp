// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_assets.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace heart_boom_maths::game
{

/// The bytes of a sound file, as a native player needs them: a decoder such as AVFoundation cannot open a Qt resource
/// path, so the game reads the file and hands over the bytes.
using ClipBytes = std::vector<std::byte>;

/// Reads the clip from the application resources (AUDIO_RESOURCE_PREFIX). Empty when the resource is missing or empty.
[[nodiscard]] std::optional<ClipBytes> read_audio_clip(const AudioClip& clip);

} // namespace heart_boom_maths::game
