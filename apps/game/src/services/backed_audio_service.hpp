// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_backend.hpp"
#include "services/audio_service.hpp"

#include <memory>

namespace heart_boom_maths::game
{

/// The AudioService every real platform uses: the rules of when a sound may play, over an AudioBackend that plays it.
///
///   effects      play only when the Settings switch is on, the app is in the foreground and, on the web, the player
///                has tapped once. An effect asked for at any other time is dropped, not queued
///   music        the bed asked for last is remembered, and sounds whenever all of the same holds for music. Switching
///                the music on, returning to the foreground or the first tap starts it again from its beginning
///   suspend      stops every effect and the music; resume() brings back the music only
///   stop_effect  always reaches the backend, so a ticking that began a moment ago cannot outlive its cause
class BackedAudioService final : public AudioService
{
public:
    /// requires_unlock holds sound back until unlock() is called (a browser); otherwise sound is allowed at once.
    BackedAudioService(std::unique_ptr<AudioBackend> backend, bool requires_unlock);

    void play_effect(SoundId id) override;
    void stop_effect(SoundId id) override;
    void play_music(MusicId id) override;
    void set_enabled(bool effects, bool music) override;
    void suspend() override;
    void resume() override;
    void unlock() override;

private:
    [[nodiscard]] bool may_play() const;
    void refresh_music();

    std::unique_ptr<AudioBackend> m_Backend;
    bool m_EffectsEnabled = true;
    bool m_MusicEnabled = true;
    bool m_Suspended = false;
    bool m_Unlocked;
    MusicId m_WantedMusic = MusicId::NONE;
    MusicId m_PlayingMusic = MusicId::NONE;
};

} // namespace heart_boom_maths::game
