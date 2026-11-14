// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_assets.hpp"
#include "services/audio_service.hpp"

#include <memory>

namespace heart_boom_maths::game
{

/// The thin layer over a sound device: it starts and stops clips and knows nothing about switches, the lifecycle or the
/// first tap. BackedAudioService decides what to play; a backend only plays it. The Qt Multimedia backend is the one
/// real implementation, and the tests use a recording one, so the rules need no sound device.
class AudioBackend
{
public:
    virtual ~AudioBackend() = default;

    /// Starts the effect from its beginning. An effect that is already sounding starts again.
    virtual void play_effect(SoundId id, const AudioClip& clip) = 0;
    virtual void stop_effect(SoundId id) = 0;
    virtual void stop_all_effects() = 0;

    /// Starts the bed from its beginning, in place of any other.
    virtual void play_music(MusicId id, const AudioClip& clip) = 0;
    virtual void stop_music() = 0;
};

/// The backend for this machine's sound device, or null when the build has no sound support or the machine has no
/// output device. Defined by qt_audio_backend.cpp when Qt Multimedia was found at configure time and by
/// no_audio_backend.cpp otherwise; CMake adds one of the two.
[[nodiscard]] std::unique_ptr<AudioBackend> make_device_audio_backend();

} // namespace heart_boom_maths::game
