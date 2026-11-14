// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_backend.hpp"
#include "services/audio_service.hpp"

#include <functional>
#include <memory>
#include <string_view>

namespace heart_boom_maths::game
{

/// Why the game has the audio service it has. The silent ones are all NullAudioService.
enum class AudioChoice
{
    DEVICE,            // BackedAudioService over the sound device
    SILENT_SMOKE_TEST, // --smoke-test: a test run never makes a sound
    SILENT_NO_DISPLAY, // the offscreen or minimal Qt platform: a CI machine, which has no sound device either
    SILENT_NO_DEVICE,  // the build has no Qt Multimedia, or the machine has no output device
};

struct AudioOptions
{
    bool smoke_test = false;
    /// QGuiApplication::platformName(): "windows", "cocoa", "xcb", "wasm", "offscreen" and so on.
    std::string_view qt_platform;
    /// A browser starts no sound before the player's first touch, click or key press (AudioService::unlock).
    bool requires_unlock = false;
};

struct AudioSelection
{
    std::unique_ptr<AudioService> service;
    AudioChoice choice;
};

/// Picks the audio service at start-up. It never fails and never blocks: whatever is missing gives a silent game. The
/// backend is made only when nothing else rules sound out, so a test run never opens a sound device; the second
/// parameter lets a test supply its own.
[[nodiscard]] AudioSelection
make_audio_service(const AudioOptions& options,
                   const std::function<std::unique_ptr<AudioBackend>()>& make_backend = make_device_audio_backend);

/// One line for the log or a status message.
[[nodiscard]] std::string_view describe(AudioChoice choice);

} // namespace heart_boom_maths::game
