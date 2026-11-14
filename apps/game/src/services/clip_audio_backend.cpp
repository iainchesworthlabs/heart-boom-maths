// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/clip_audio_backend.hpp"

#include "heart_boom_maths/profile.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace heart_boom_maths::game
{

namespace
{

ClipPlayer* find(const std::map<int, std::unique_ptr<ClipPlayer>>& players, int key)
{
    const auto found = players.find(key);
    return found == players.end() ? nullptr : found->second.get();
}

} // namespace

float player_volume(int volume_percent)
{
    return static_cast<float>(std::clamp(volume_percent, 0, 100)) / 100.0F;
}

ClipAudioBackend::ClipAudioBackend(std::unique_ptr<PlayerHost> host, ClipReader reader, ClipLog log)
    : m_Host(std::move(host)), m_Reader(std::move(reader)), m_Log(std::move(log))
{
    if (!m_Log)
    {
        m_Log = [](std::string_view) {};
    }
    load_players();
}

std::unique_ptr<ClipPlayer> ClipAudioBackend::load_player(const AudioClip& clip)
{
    const std::string name(clip.file);
    std::optional<ClipBytes> bytes;
    if (m_Reader)
    {
        bytes = m_Reader(clip);
    }
    if (!bytes)
    {
        m_Log("audio: cannot read " + name + ", it stays silent");
        return nullptr;
    }
    auto player = m_Host->make_player(*bytes, clip.looping, player_volume(clip.volume_percent));
    if (!player)
    {
        m_Log("audio: cannot decode " + name + ", it stays silent");
    }
    return player;
}

void ClipAudioBackend::load_players()
{
    HBM_ZONE("audio.load_players");
    m_Effects.clear();
    m_Music.clear();
    for (const SoundId id : all_sounds())
    {
        if (const auto clip = clip_for(id))
        {
            if (auto player = load_player(*clip))
            {
                m_Effects.emplace(static_cast<int>(id), std::move(player));
            }
        }
    }
    for (const MusicId id : all_music())
    {
        if (const auto clip = clip_for(id))
        {
            if (auto player = load_player(*clip))
            {
                m_Music.emplace(static_cast<int>(id), std::move(player));
            }
        }
    }
}

std::size_t ClipAudioBackend::loaded_clips() const
{
    return m_Effects.size() + m_Music.size();
}

// The session is activated when a sound is first wanted, so a game that is silent (both switches off) never takes the
// audio route from anything else. A failed activation is tried again at the next sound.
bool ClipAudioBackend::ensure_session()
{
    if (!m_SessionActive)
    {
        m_SessionActive = m_Host->activate_session();
    }
    return m_SessionActive;
}

void ClipAudioBackend::play_effect(SoundId id, const AudioClip&)
{
    HBM_ZONE("audio.backend_play_effect");
    if (ClipPlayer* player = find(m_Effects, static_cast<int>(id)); player != nullptr && ensure_session())
    {
        player->play_from_start();
    }
}

void ClipAudioBackend::stop_effect(SoundId id)
{
    if (ClipPlayer* player = find(m_Effects, static_cast<int>(id)))
    {
        player->stop();
    }
}

void ClipAudioBackend::stop_all_effects()
{
    for (auto& entry : m_Effects)
    {
        entry.second->stop();
    }
}

void ClipAudioBackend::play_music(MusicId id, const AudioClip&)
{
    stop_music();
    m_WantedMusic = id;
    start_wanted_music();
}

void ClipAudioBackend::stop_music()
{
    m_WantedMusic = MusicId::NONE;
    for (auto& entry : m_Music)
    {
        entry.second->stop();
    }
}

void ClipAudioBackend::start_wanted_music()
{
    if (ClipPlayer* player = find(m_Music, static_cast<int>(m_WantedMusic)); player != nullptr && ensure_session())
    {
        player->play_from_start();
    }
}

void ClipAudioBackend::stop_all_players()
{
    stop_all_effects();
    for (auto& entry : m_Music)
    {
        entry.second->stop();
    }
}

// The bed that was asked for stays remembered, so it can come back. An effect is over by the time the interruption is.
void ClipAudioBackend::interruption_began()
{
    m_SessionActive = false;
    stop_all_players();
}

// Whatever ends an interruption, the session has to be activated again. The bed comes back only when the system says
// the app may resume, and only if nothing stopped it in the meantime (stop_music forgets it).
void ClipAudioBackend::interruption_ended(bool should_resume)
{
    m_SessionActive = false;
    if (should_resume)
    {
        start_wanted_music();
    }
}

void ClipAudioBackend::media_services_reset()
{
    m_SessionActive = false;
    load_players();
    start_wanted_music();
}

} // namespace heart_boom_maths::game
