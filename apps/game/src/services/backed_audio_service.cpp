// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/backed_audio_service.hpp"

#include "heart_boom_maths/profile.hpp"

#include <utility>

namespace heart_boom_maths::game
{

BackedAudioService::BackedAudioService(std::unique_ptr<AudioBackend> backend, bool requires_unlock)
    : m_Backend(std::move(backend)), m_Unlocked(!requires_unlock)
{
}

bool BackedAudioService::may_play() const
{
    return !m_Suspended && m_Unlocked;
}

void BackedAudioService::play_effect(SoundId id)
{
    HBM_ZONE("audio.play_effect");
    if (!m_EffectsEnabled || !may_play())
    {
        return;
    }
    if (const auto clip = clip_for(id))
    {
        m_Backend->play_effect(id, *clip);
    }
}

void BackedAudioService::stop_effect(SoundId id)
{
    m_Backend->stop_effect(id);
}

void BackedAudioService::play_music(MusicId id)
{
    HBM_ZONE("audio.play_music");
    m_WantedMusic = id;
    refresh_music();
}

void BackedAudioService::set_enabled(bool effects, bool music)
{
    HBM_ZONE("audio.set_enabled");
    m_EffectsEnabled = effects;
    m_MusicEnabled = music;
    if (!effects)
    {
        m_Backend->stop_all_effects();
    }
    refresh_music();
}

void BackedAudioService::suspend()
{
    m_Suspended = true;
    m_Backend->stop_all_effects();
    refresh_music();
}

void BackedAudioService::resume()
{
    m_Suspended = false;
    refresh_music();
}

void BackedAudioService::unlock()
{
    m_Unlocked = true;
    refresh_music();
}

// Brings what the backend plays in line with what should sound now. Starting a bed again after a pause begins it from
// the start, which suits a loop of a few seconds.
void BackedAudioService::refresh_music()
{
    HBM_ZONE("audio.refresh_music");
    MusicId wanted = MusicId::NONE;
    if (m_MusicEnabled && may_play() && clip_for(m_WantedMusic))
    {
        wanted = m_WantedMusic;
    }
    if (wanted == m_PlayingMusic)
    {
        return;
    }
    m_PlayingMusic = wanted;
    if (const auto clip = clip_for(wanted))
    {
        m_Backend->play_music(wanted, *clip);
    }
    else
    {
        m_Backend->stop_music();
    }
}

} // namespace heart_boom_maths::game
