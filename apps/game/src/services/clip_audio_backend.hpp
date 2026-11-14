// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_backend.hpp"
#include "services/audio_resource.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace heart_boom_maths::game
{

/// One sound that a native player holds ready: an AVAudioPlayer on iOS. It loops or not as it was made, at the volume
/// it was made with.
class ClipPlayer
{
public:
    virtual ~ClipPlayer() = default;

    /// Starts from the beginning; a clip that is already sounding starts again.
    virtual void play_from_start() = 0;
    /// Stops and rewinds. Safe on a player that is not sounding.
    virtual void stop() = 0;
};

/// The native side of a ClipAudioBackend: the audio session and the players. A shim over the platform's calls with no
/// decisions of its own.
class PlayerHost
{
public:
    virtual ~PlayerHost() = default;

    /// Makes the audio session ready to sound (on iOS: category Ambient, then active). False when it cannot be, which
    /// can happen during a phone call; the backend then drops what it was about to play and asks again next time.
    [[nodiscard]] virtual bool activate_session() = 0;

    /// A player for a whole sound file held in memory, or null when the platform cannot decode it.
    [[nodiscard]] virtual std::unique_ptr<ClipPlayer> make_player(std::span<const std::byte> sound_file, bool looping,
                                                                  float volume) = 0;
};

using ClipReader = std::function<std::optional<ClipBytes>(const AudioClip&)>;
using ClipLog = std::function<void(std::string_view)>;

/// Volume of a player for the percentage of the clip table: 0 to 1, with an out-of-range percentage held to the ends.
[[nodiscard]] float player_volume(int volume_percent);

/// The AudioBackend of a platform whose native player takes a sound file from memory (iOS). It holds one player per
/// clip, loaded when it is made so the first effect is not late, and owns every decision a platform shim would
/// otherwise make:
///
///   loading       a clip that cannot be read or decoded is logged and skipped, and playing it does nothing
///   session       made active on the first sound, not at start-up, and again after an interruption or a reset of the
///                 media services. A sound asked for while the session cannot be activated is dropped
///   interruption  a phone call, Siri or an alarm stops every player. When it ends, the bed that was sounding starts
///                 again if the system says it may; effects are not brought back, they are over
///   reset         the system reset its media services: every player is gone and is loaded again
class ClipAudioBackend : public AudioBackend
{
public:
    ClipAudioBackend(std::unique_ptr<PlayerHost> host, ClipReader reader, ClipLog log);

    void play_effect(SoundId id, const AudioClip& clip) override;
    void stop_effect(SoundId id) override;
    void stop_all_effects() override;
    void play_music(MusicId id, const AudioClip& clip) override;
    void stop_music() override;

    /// What the platform reports about the audio session. Each is safe to repeat.
    void interruption_began();
    void interruption_ended(bool should_resume);
    void media_services_reset();

    /// How many clips have a player. The device factory gives no backend, and so a silent game, when it is none.
    [[nodiscard]] std::size_t loaded_clips() const;

private:
    using Players = std::map<int, std::unique_ptr<ClipPlayer>>;

    void load_players();
    [[nodiscard]] std::unique_ptr<ClipPlayer> load_player(const AudioClip& clip);
    [[nodiscard]] bool ensure_session();
    void start_wanted_music();
    void stop_all_players();

    std::unique_ptr<PlayerHost> m_Host;
    ClipReader m_Reader;
    ClipLog m_Log;
    Players m_Effects;
    Players m_Music;
    bool m_SessionActive = false;
    MusicId m_WantedMusic = MusicId::NONE;
};

} // namespace heart_boom_maths::game
