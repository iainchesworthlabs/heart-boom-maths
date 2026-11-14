// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/audio_assets.hpp"

namespace heart_boom_maths::game
{

namespace
{

// Loudness of each clip against the others, in percent of full volume. The files are normalised when they are made, so
// this only balances the effects with each other and keeps the music under them.
constexpr int EFFECT_VOLUME_PERCENT = 80;
constexpr int QUIET_EFFECT_VOLUME_PERCENT = 60;
constexpr int MUSIC_VOLUME_PERCENT = 50;

} // namespace

std::optional<AudioClip> clip_for(SoundId id)
{
    switch (id)
    {
    case SoundId::COIN_CHIME:
        return AudioClip{"coin_chime.wav", false, EFFECT_VOLUME_PERCENT};
    case SoundId::SOFT_BUZZ:
        return AudioClip{"soft_buzz.wav", false, QUIET_EFFECT_VOLUME_PERCENT};
    case SoundId::RISING_TONE:
        return AudioClip{"rising_tone.wav", false, QUIET_EFFECT_VOLUME_PERCENT};
    case SoundId::TICKING:
        return AudioClip{"ticking.wav", true, QUIET_EFFECT_VOLUME_PERCENT};
    case SoundId::CARTOON_POP:
        return AudioClip{"cartoon_pop.wav", false, EFFECT_VOLUME_PERCENT};
    case SoundId::FANFARE:
        return AudioClip{"fanfare.wav", false, EFFECT_VOLUME_PERCENT};
    case SoundId::WHOOSH:
        return AudioClip{"whoosh.wav", false, QUIET_EFFECT_VOLUME_PERCENT};
    }
    return std::nullopt;
}

std::optional<AudioClip> clip_for(MusicId id)
{
    switch (id)
    {
    case MusicId::MENU:
        return AudioClip{"music_menu.wav", true, MUSIC_VOLUME_PERCENT};
    case MusicId::PLAY:
        return AudioClip{"music_play.wav", true, MUSIC_VOLUME_PERCENT};
    case MusicId::NONE:
        return std::nullopt;
    }
    return std::nullopt;
}

std::vector<SoundId> all_sounds()
{
    return {SoundId::COIN_CHIME,  SoundId::SOFT_BUZZ, SoundId::RISING_TONE, SoundId::TICKING,
            SoundId::CARTOON_POP, SoundId::FANFARE,   SoundId::WHOOSH};
}

std::vector<MusicId> all_music()
{
    return {MusicId::MENU, MusicId::PLAY};
}

} // namespace heart_boom_maths::game
