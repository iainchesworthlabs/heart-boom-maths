// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/audio_assets.hpp"
#include "services/audio_resource.hpp"
#include "services/backed_audio_service.hpp"
#include "services/clip_audio_backend.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace heart_boom_maths::game;

namespace
{

using Events = std::vector<std::string>;

/// What the fake platform was asked to do, in order. A player is told apart by the file name the fake reader put into
/// its bytes.
struct World
{
    Events events;
    Events logged;
    std::set<std::string> unreadable;
    std::set<std::string> undecodable;
    bool session_available = true;
    int made = 0;
    float last_volume_of_ticking = -1.0F;
    std::vector<std::pair<std::string, bool>> loops; // file, looping
};

class FakePlayer final : public ClipPlayer
{
public:
    FakePlayer(World& world, std::string name) : m_World(world), m_Name(std::move(name)) {}

    void play_from_start() override
    {
        m_World.events.push_back("play:" + m_Name);
    }

    void stop() override
    {
        m_World.events.push_back("stop:" + m_Name);
    }

private:
    World& m_World;
    std::string m_Name;
};

class FakeHost final : public PlayerHost
{
public:
    explicit FakeHost(World& world) : m_World(world) {}

    bool activate_session() override
    {
        m_World.events.push_back(m_World.session_available ? "session" : "session-failed");
        return m_World.session_available;
    }

    std::unique_ptr<ClipPlayer> make_player(std::span<const std::byte> sound_file, bool looping, float volume) override
    {
        std::string name;
        for (const std::byte b : sound_file)
        {
            name.push_back(static_cast<char>(b));
        }
        ++m_World.made;
        if (m_World.undecodable.contains(name))
        {
            return nullptr;
        }
        m_World.loops.emplace_back(name, looping);
        if (name == "ticking.wav")
        {
            m_World.last_volume_of_ticking = volume;
        }
        return std::make_unique<FakePlayer>(m_World, name);
    }

private:
    World& m_World;
};

std::unique_ptr<ClipAudioBackend> make_backend(World& world)
{
    ClipReader reader = [&world](const AudioClip& clip) -> std::optional<ClipBytes>
    {
        const std::string name(clip.file);
        if (world.unreadable.contains(name))
        {
            return std::nullopt;
        }
        ClipBytes bytes;
        for (const char c : name)
        {
            bytes.push_back(static_cast<std::byte>(c));
        }
        return bytes;
    };
    ClipLog log = [&world](std::string_view message) { world.logged.emplace_back(message); };
    return std::make_unique<ClipAudioBackend>(std::make_unique<FakeHost>(world), std::move(reader), std::move(log));
}

AudioClip clip_of(SoundId id)
{
    return *clip_for(id);
}

AudioClip clip_of(MusicId id)
{
    return *clip_for(id);
}

int count(const Events& events, const std::string& what)
{
    return static_cast<int>(std::ranges::count(events, what));
}

/// The part of the events after the start-up, so a test reads only what it did.
Events take(World& world)
{
    Events taken;
    taken.swap(world.events);
    return taken;
}

} // namespace

TEST_CASE("A player is loaded for every clip when the backend is made, and the session is not touched", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    CHECK(backend->loaded_clips() == all_sounds().size() + all_music().size());
    CHECK(world.made == 9);
    CHECK(world.logged.empty());
    CHECK(world.events.empty());

    // Looping follows the clip table: the ticking and the two beds loop, the other effects do not.
    std::set<std::string> looping;
    for (const auto& [file, loops] : world.loops)
    {
        if (loops)
        {
            looping.insert(file);
        }
    }
    CHECK(looping == std::set<std::string>{"ticking.wav", "music_menu.wav", "music_play.wav"});
}

TEST_CASE("The volume of a player is the percentage of the clip table", "[unit][audio]")
{
    CHECK(player_volume(0) == 0.0F);
    CHECK(player_volume(50) == 0.5F);
    CHECK(player_volume(100) == 1.0F);
    CHECK(player_volume(-20) == 0.0F);
    CHECK(player_volume(250) == 1.0F);

    World world;
    const auto backend = make_backend(world);
    CHECK(world.last_volume_of_ticking == player_volume(clip_of(SoundId::TICKING).volume_percent));
}

TEST_CASE("A clip that cannot be read or decoded is logged and stays silent, and the others still play",
          "[unit][audio]")
{
    World world;
    world.unreadable = {"coin_chime.wav"};
    world.undecodable = {"music_menu.wav"};
    const auto backend = make_backend(world);

    CHECK(backend->loaded_clips() == 7);
    REQUIRE(world.logged.size() == 2);
    CHECK(world.logged[0].find("coin_chime.wav") != std::string::npos);
    CHECK(world.logged[1].find("music_menu.wav") != std::string::npos);

    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    backend->play_music(MusicId::MENU, clip_of(MusicId::MENU));
    backend->stop_effect(SoundId::COIN_CHIME);
    // Not even the session was taken for a sound that cannot play.
    CHECK(count(world.events, "session") == 0);
    CHECK(std::ranges::none_of(world.events, [](const std::string& event) { return event.starts_with("play:"); }));
    take(world);

    backend->play_effect(SoundId::WHOOSH, clip_of(SoundId::WHOOSH));
    CHECK(world.events == Events{"session", "play:whoosh.wav"});
}

TEST_CASE("A backend with nothing to read loads nothing and never crashes", "[unit][audio]")
{
    World world;
    ClipAudioBackend no_reader(std::make_unique<FakeHost>(world), ClipReader{}, ClipLog{});
    CHECK(no_reader.loaded_clips() == 0);
    no_reader.play_effect(SoundId::FANFARE, clip_of(SoundId::FANFARE));
    no_reader.play_music(MusicId::PLAY, clip_of(MusicId::PLAY));
    no_reader.stop_all_effects();
    no_reader.interruption_began();
    no_reader.interruption_ended(true);
    no_reader.media_services_reset();
    CHECK(no_reader.loaded_clips() == 0);
}

TEST_CASE("The session is activated by the first sound only, and an effect restarts from its beginning",
          "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);

    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    backend->play_effect(SoundId::FANFARE, clip_of(SoundId::FANFARE));
    CHECK(take(world) == Events{"session", "play:coin_chime.wav", "play:coin_chime.wav", "play:fanfare.wav"});
}

TEST_CASE("A sound asked for while the session cannot be activated is dropped, and the next one tries again",
          "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);

    world.session_available = false;
    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    backend->play_effect(SoundId::SOFT_BUZZ, clip_of(SoundId::SOFT_BUZZ));
    CHECK(take(world) == Events{"session-failed", "session-failed"});

    world.session_available = true;
    backend->play_effect(SoundId::WHOOSH, clip_of(SoundId::WHOOSH));
    CHECK(take(world) == Events{"session", "play:whoosh.wav"});
}

TEST_CASE("Stopping needs no session and always reaches the player", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->stop_effect(SoundId::TICKING);
    CHECK(take(world) == Events{"stop:ticking.wav"});

    backend->stop_all_effects();
    const Events stopped = take(world);
    CHECK(stopped.size() == all_sounds().size());
    CHECK(count(stopped, "stop:ticking.wav") == 1);
    CHECK(count(stopped, "session") == 0);

    backend->stop_music();
    CHECK(take(world).size() == all_music().size());
}

TEST_CASE("A music bed replaces the one that sounds, and stop_music silences them all", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);

    backend->play_music(MusicId::MENU, clip_of(MusicId::MENU));
    const Events first = take(world);
    CHECK(first.front() == "stop:music_menu.wav");
    CHECK(count(first, "session") == 1);
    CHECK(first.back() == "play:music_menu.wav");

    backend->play_music(MusicId::PLAY, clip_of(MusicId::PLAY));
    const Events second = take(world);
    CHECK(count(second, "stop:music_menu.wav") == 1);
    CHECK(count(second, "session") == 0);
    CHECK(second.back() == "play:music_play.wav");

    backend->play_music(MusicId::NONE, AudioClip{"", false, 0});
    const Events none = take(world);
    CHECK(count(none, "play:music_menu.wav") + count(none, "play:music_play.wav") == 0);
}

TEST_CASE("An interruption stops every player and the bed comes back when it ends and the system allows",
          "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->play_music(MusicId::PLAY, clip_of(MusicId::PLAY));
    backend->play_effect(SoundId::TICKING, clip_of(SoundId::TICKING));
    take(world);

    backend->interruption_began();
    const Events began = take(world);
    CHECK(began.size() == all_sounds().size() + all_music().size());
    CHECK(count(began, "stop:ticking.wav") == 1);
    CHECK(count(began, "stop:music_play.wav") == 1);
    CHECK(count(began, "play:music_play.wav") == 0);

    backend->interruption_ended(true);
    CHECK(take(world) == Events{"session", "play:music_play.wav"});

    // The session was taken again, so the next effect needs no activation and does not wait for anything.
    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    CHECK(take(world) == Events{"play:coin_chime.wav"});
}

TEST_CASE("After an interruption an effect activates the session again", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    take(world);

    backend->interruption_began();
    take(world);
    backend->interruption_ended(false);
    CHECK(take(world).empty());

    backend->play_effect(SoundId::COIN_CHIME, clip_of(SoundId::COIN_CHIME));
    CHECK(take(world) == Events{"session", "play:coin_chime.wav"});
}

TEST_CASE("The bed does not come back when the system does not allow it, or when it was stopped meanwhile",
          "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->play_music(MusicId::MENU, clip_of(MusicId::MENU));
    backend->interruption_began();
    backend->interruption_ended(false);
    CHECK(count(take(world), "play:music_menu.wav") == 1); // only the first start, none after the interruption

    backend->interruption_began();
    backend->stop_music(); // the app went to the background during the interruption
    take(world);
    backend->interruption_ended(true);
    CHECK(take(world).empty());
}

TEST_CASE("An interruption that ends while the session is still unavailable keeps silent and recovers later",
          "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->play_music(MusicId::MENU, clip_of(MusicId::MENU));
    backend->interruption_began();
    take(world);

    world.session_available = false;
    backend->interruption_ended(true);
    CHECK(take(world) == Events{"session-failed"});

    world.session_available = true;
    backend->play_music(MusicId::PLAY, clip_of(MusicId::PLAY));
    CHECK(count(take(world), "play:music_play.wav") == 1);
}

TEST_CASE("Repeating a notification does no harm", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->interruption_began();
    backend->interruption_began();
    backend->interruption_ended(true);
    backend->interruption_ended(true);
    CHECK(count(take(world), "play:music_menu.wav") == 0); // no bed was asked for
}

TEST_CASE("A reset of the media services loads every player again and restarts the bed", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->play_music(MusicId::PLAY, clip_of(MusicId::PLAY));
    take(world);
    const int made_before = world.made;

    backend->media_services_reset();
    CHECK(world.made == made_before * 2);
    CHECK(backend->loaded_clips() == 9);
    CHECK(take(world) == Events{"session", "play:music_play.wav"});
}

TEST_CASE("A reset with no bed asked for starts nothing", "[unit][audio]")
{
    World world;
    const auto backend = make_backend(world);
    backend->media_services_reset();
    CHECK(world.events.empty());
}

TEST_CASE("Under the audio service the backend follows suspend and resume", "[unit][audio]")
{
    World world;
    BackedAudioService service(make_backend(world), false);

    service.play_music(MusicId::MENU);
    service.play_effect(SoundId::TICKING);
    take(world);

    service.suspend();
    const Events suspended = take(world);
    CHECK(count(suspended, "stop:ticking.wav") == 1);
    CHECK(count(suspended, "stop:music_menu.wav") == 1);
    CHECK(count(suspended, "play:music_menu.wav") == 0);

    service.play_effect(SoundId::COIN_CHIME); // dropped while suspended
    CHECK(count(take(world), "play:coin_chime.wav") == 0);

    service.resume();
    CHECK(count(take(world), "play:music_menu.wav") == 1);
}

TEST_CASE("Every clip of the table can be read from the resources, and a name that is none cannot", "[unit][audio]")
{
    std::vector<AudioClip> clips;
    for (const SoundId id : all_sounds())
    {
        clips.push_back(clip_of(id));
    }
    for (const MusicId id : all_music())
    {
        clips.push_back(clip_of(id));
    }
    REQUIRE(clips.size() == 9);
    for (const AudioClip& clip : clips)
    {
        INFO(clip.file);
        const auto bytes = read_audio_clip(clip);
        REQUIRE(bytes.has_value());
        REQUIRE(bytes->size() > 44);
        const std::string header(reinterpret_cast<const char*>(bytes->data()), 4);
        CHECK(header == "RIFF");
    }
    CHECK_FALSE(read_audio_clip(AudioClip{"there_is_no_such_file.wav", false, 50}).has_value());
}
