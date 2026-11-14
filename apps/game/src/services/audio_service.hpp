// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace heart_boom_maths::game
{

/// The effects of the feedback table in SPEC.md. Each has a visual twin on screen, so a muted game plays the same.
enum class SoundId : std::uint8_t
{
    COIN_CHIME,  // correct answer
    SOFT_BUZZ,   // wrong answer
    RISING_TONE, // a streak step
    TICKING,     // the last seconds; runs until stop_effect()
    CARTOON_POP, // the bomb goes off
    FANFARE,     // level complete
    WHOOSH,      // a hint
};

enum class MusicId : std::uint8_t
{
    NONE,
    MENU,
    PLAY,
};

class AudioService
{
public:
    virtual ~AudioService() = default;

    virtual void play_effect(SoundId id) = 0;

    /// Ends an effect that keeps running, such as the ticking, when its cause is over before the sound is.
    virtual void stop_effect(SoundId id) = 0;

    virtual void play_music(MusicId id) = 0;

    /// The two Settings switches.
    virtual void set_enabled(bool effects, bool music) = 0;

    /// The app left the foreground: stop all sound until resume().
    virtual void suspend() = 0;
    virtual void resume() = 0;
};

/// Plays nothing. The service until the Qt Multimedia one exists, and on a platform without audio.
class NullAudioService final : public AudioService
{
public:
    void play_effect(SoundId) override {}

    void stop_effect(SoundId) override {}

    void play_music(MusicId) override {}

    void set_enabled(bool, bool) override {}

    void suspend() override {}

    void resume() override {}
};

} // namespace heart_boom_maths::game
