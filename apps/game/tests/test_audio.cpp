// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "services/audio_assets.hpp"
#include "services/audio_backend.hpp"
#include "services/audio_factory.hpp"
#include "services/backed_audio_service.hpp"
#include "services/first_gesture_filter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QSet>
#include <QString>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace heart_boom_maths::game;

namespace
{

/// Writes what it is asked to do into a log the test owns, so the service can be moved away and still be read.
class RecordingBackend final : public AudioBackend
{
public:
    explicit RecordingBackend(std::vector<std::string>& log) : m_Log(log) {}

    void play_effect(SoundId id, const AudioClip&) override
    {
        m_Log.push_back("effect:" + std::to_string(static_cast<int>(id)));
    }

    void stop_effect(SoundId id) override
    {
        m_Log.push_back("stop_effect:" + std::to_string(static_cast<int>(id)));
    }

    void stop_all_effects() override
    {
        m_Log.push_back("stop_effects");
    }

    void play_music(MusicId id, const AudioClip&) override
    {
        m_Log.push_back("music:" + std::to_string(static_cast<int>(id)));
    }

    void stop_music() override
    {
        m_Log.push_back("stop_music");
    }

private:
    std::vector<std::string>& m_Log;
};

using Log = std::vector<std::string>;

std::string effect(SoundId id)
{
    return "effect:" + std::to_string(static_cast<int>(id));
}

std::string music(MusicId id)
{
    return "music:" + std::to_string(static_cast<int>(id));
}

std::unique_ptr<BackedAudioService> make_service(Log& log, bool requires_unlock = false)
{
    return std::make_unique<BackedAudioService>(std::make_unique<RecordingBackend>(log), requires_unlock);
}

std::uint32_t le32(const QByteArray& bytes, int offset)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24U);
}

QString resource_path(const AudioClip& clip)
{
    return QString::fromUtf8(AUDIO_RESOURCE_PREFIX.data(), static_cast<qsizetype>(AUDIO_RESOURCE_PREFIX.size())) +
           QString::fromUtf8(clip.file.data(), static_cast<qsizetype>(clip.file.size()));
}

} // namespace

TEST_CASE("Every sound and music bed has a clip, and nothing else has", "[unit][audio]")
{
    for (const SoundId id : all_sounds())
    {
        const auto clip = clip_for(id);
        REQUIRE(clip.has_value());
        CHECK_FALSE(clip->file.empty());
        CHECK(clip->volume_percent > 0);
        CHECK(clip->volume_percent <= 100);
    }
    for (const MusicId id : all_music())
    {
        const auto clip = clip_for(id);
        REQUIRE(clip.has_value());
        CHECK(clip->looping);
    }
    CHECK(all_sounds().size() == 7);
    CHECK_FALSE(clip_for(MusicId::NONE).has_value());
    CHECK_FALSE(clip_for(static_cast<SoundId>(200)).has_value());
    CHECK_FALSE(clip_for(static_cast<MusicId>(200)).has_value());
    // Only the ticking runs until it is stopped: it lasts as long as the timer is in the alert zone.
    CHECK(clip_for(SoundId::TICKING)->looping);
    CHECK_FALSE(clip_for(SoundId::FANFARE)->looping);
}

TEST_CASE("Every clip is a small mono 16-bit sound file in the resources", "[unit][audio]")
{
    QSet<QString> used;
    qint64 total = 0;
    std::vector<AudioClip> clips;
    for (const SoundId id : all_sounds())
    {
        clips.push_back(*clip_for(id));
    }
    for (const MusicId id : all_music())
    {
        clips.push_back(*clip_for(id));
    }
    for (const AudioClip& clip : clips)
    {
        INFO(clip.file);
        QFile file(resource_path(clip));
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        total += bytes.size();
        used.insert(QString::fromUtf8(clip.file.data(), static_cast<qsizetype>(clip.file.size())));

        REQUIRE(bytes.size() > 44);
        CHECK(bytes.startsWith("RIFF"));
        CHECK(bytes.mid(8, 4) == "WAVE");
        CHECK(bytes.mid(12, 4) == "fmt ");
        CHECK(static_cast<std::uint8_t>(bytes[20]) == 1); // PCM
        CHECK(static_cast<std::uint8_t>(bytes[22]) == 1); // mono
        CHECK(le32(bytes, 24) <= 44100);
        CHECK(static_cast<std::uint8_t>(bytes[34]) == 16); // bits per sample
        CHECK(bytes.mid(36, 4) == "data");
        CHECK(le32(bytes, 40) == static_cast<std::uint32_t>(bytes.size() - 44));
    }
    // The web download size matters: the whole set stays small.
    CHECK(total < 1'500'000);

    // No file in the resources is left without a sound that plays it.
    const QStringList present = QDir(QStringLiteral(":/heart_boom_maths/audio")).entryList(QDir::Files);
    CHECK(QSet<QString>(present.begin(), present.end()) == used);
}

TEST_CASE("An effect plays when the switch is on and not when it is off", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);

    service->play_effect(SoundId::COIN_CHIME);
    CHECK(log == Log{effect(SoundId::COIN_CHIME)});

    log.clear();
    service->set_enabled(false, true);
    CHECK(log == Log{"stop_effects"});
    log.clear();
    service->play_effect(SoundId::COIN_CHIME);
    CHECK(log.empty());

    service->set_enabled(true, true);
    service->play_effect(SoundId::FANFARE);
    CHECK(log == Log{effect(SoundId::FANFARE)});
}

TEST_CASE("An id that is no sound is ignored, and a stop always goes through", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);

    service->play_effect(static_cast<SoundId>(200));
    service->play_music(static_cast<MusicId>(200));
    CHECK(log.empty());

    // The ticking may stop while sound is off, because it can have begun before the switch was turned off.
    service->set_enabled(false, false);
    log.clear();
    service->stop_effect(SoundId::TICKING);
    CHECK(log == Log{"stop_effect:" + std::to_string(static_cast<int>(SoundId::TICKING))});
}

TEST_CASE("Music follows the switch and starts again when it is turned back on", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);

    service->play_music(MusicId::MENU);
    CHECK(log == Log{music(MusicId::MENU)});

    log.clear();
    service->play_music(MusicId::MENU); // the same bed again does not restart it
    CHECK(log.empty());

    service->play_music(MusicId::PLAY);
    CHECK(log == Log{music(MusicId::PLAY)});

    log.clear();
    service->set_enabled(true, false);
    CHECK(log == Log{"stop_music"});

    log.clear();
    service->play_music(MusicId::MENU); // remembered while the music is off
    CHECK(log.empty());
    service->set_enabled(true, true);
    CHECK(log == Log{music(MusicId::MENU)});

    log.clear();
    service->play_music(MusicId::NONE);
    CHECK(log == Log{"stop_music"});
}

TEST_CASE("The music switch does not touch the effects and the effects switch does not touch the music",
          "[unit][audio]")
{
    Log log;
    auto service = make_service(log);
    service->play_music(MusicId::PLAY);

    log.clear();
    service->set_enabled(false, true);
    CHECK(log == Log{"stop_effects"});

    log.clear();
    service->set_enabled(false, false);
    CHECK(log == Log{"stop_effects", "stop_music"});
    service->play_effect(SoundId::WHOOSH);
    CHECK(log == Log{"stop_effects", "stop_music"});
}

TEST_CASE("Leaving the foreground silences everything and coming back restores the music", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);
    service->play_music(MusicId::PLAY);
    log.clear();

    service->suspend();
    CHECK(log == Log{"stop_effects", "stop_music"});

    log.clear();
    service->play_effect(SoundId::COIN_CHIME); // dropped, not queued
    service->suspend();                        // a second suspend changes nothing more
    CHECK(log == Log{"stop_effects"});

    log.clear();
    service->resume();
    CHECK(log == Log{music(MusicId::PLAY)});

    log.clear();
    service->resume();
    service->play_effect(SoundId::COIN_CHIME);
    CHECK(log == Log{effect(SoundId::COIN_CHIME)});
}

TEST_CASE("A bed asked for while suspended starts on resume, and a resume with no bed starts nothing", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);
    service->resume();
    CHECK(log.empty());

    service->suspend();
    log.clear();
    service->play_music(MusicId::MENU);
    CHECK(log.empty());
    service->resume();
    CHECK(log == Log{music(MusicId::MENU)});
}

TEST_CASE("A music switch that is off stays off through a suspend and a resume", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);
    service->set_enabled(true, false);
    service->play_music(MusicId::MENU);
    log.clear();

    service->suspend();
    service->resume();
    CHECK(log == Log{"stop_effects"});
}

TEST_CASE("On the web nothing sounds before the first tap", "[unit][audio]")
{
    Log log;
    auto service = make_service(log, true);

    service->play_effect(SoundId::COIN_CHIME);
    service->play_music(MusicId::MENU);
    CHECK(log.empty());

    service->unlock();
    CHECK(log == Log{music(MusicId::MENU)});

    log.clear();
    service->play_effect(SoundId::COIN_CHIME);
    CHECK(log == Log{effect(SoundId::COIN_CHIME)});

    log.clear();
    service->unlock(); // a second unlock changes nothing
    CHECK(log.empty());
}

TEST_CASE("Elsewhere the first tap changes nothing", "[unit][audio]")
{
    Log log;
    auto service = make_service(log);
    service->play_music(MusicId::MENU);
    log.clear();
    service->unlock();
    CHECK(log.empty());
}

TEST_CASE("The factory picks silence for a test run, a machine with no display and a machine with no device",
          "[unit][audio]")
{
    int made = 0;
    const auto make_backend = [&made]() -> std::unique_ptr<AudioBackend>
    {
        ++made;
        static Log sink;
        return std::make_unique<RecordingBackend>(sink);
    };

    AudioOptions options;
    options.qt_platform = "windows";

    SECTION("a smoke test")
    {
        options.smoke_test = true;
        const AudioSelection selection = make_audio_service(options, make_backend);
        CHECK(selection.choice == AudioChoice::SILENT_SMOKE_TEST);
        CHECK(dynamic_cast<NullAudioService*>(selection.service.get()) != nullptr);
        CHECK(made == 0);
    }
    SECTION("the display-less Qt platforms")
    {
        for (const char* platform : {"offscreen", "minimal", "minimalegl"})
        {
            options.qt_platform = platform;
            const AudioSelection selection = make_audio_service(options, make_backend);
            CHECK(selection.choice == AudioChoice::SILENT_NO_DISPLAY);
            CHECK(dynamic_cast<NullAudioService*>(selection.service.get()) != nullptr);
        }
        CHECK(made == 0);
    }
    SECTION("no device")
    {
        const AudioSelection selection = make_audio_service(options, [] { return std::unique_ptr<AudioBackend>(); });
        CHECK(selection.choice == AudioChoice::SILENT_NO_DEVICE);
        CHECK(dynamic_cast<NullAudioService*>(selection.service.get()) != nullptr);

        const AudioSelection none = make_audio_service(options, nullptr);
        CHECK(none.choice == AudioChoice::SILENT_NO_DEVICE);
    }
    SECTION("a device")
    {
        const AudioSelection selection = make_audio_service(options, make_backend);
        CHECK(selection.choice == AudioChoice::DEVICE);
        CHECK(dynamic_cast<BackedAudioService*>(selection.service.get()) != nullptr);
        CHECK(made == 1);
    }
}

TEST_CASE("The device factory never fails: without a display it gives a silent service", "[unit][audio]")
{
    // The tests run on the offscreen platform, so this is the choice a CI machine makes.
    AudioOptions options;
    options.qt_platform = "offscreen";
    const AudioSelection selection = make_audio_service(options);
    CHECK(selection.choice == AudioChoice::SILENT_NO_DISPLAY);
    selection.service->play_effect(SoundId::COIN_CHIME);
    selection.service->play_music(MusicId::MENU);
    selection.service->suspend();
    selection.service->resume();
    selection.service->unlock();
}

TEST_CASE("Each choice has its own description", "[unit][audio]")
{
    const std::vector<AudioChoice> choices{AudioChoice::DEVICE, AudioChoice::SILENT_SMOKE_TEST,
                                           AudioChoice::SILENT_NO_DISPLAY, AudioChoice::SILENT_NO_DEVICE};
    QSet<QString> seen;
    for (const AudioChoice choice : choices)
    {
        const std::string_view text = describe(choice);
        CHECK(text.starts_with("audio: "));
        seen.insert(QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())));
    }
    CHECK(seen.size() == static_cast<qsizetype>(choices.size()));
    CHECK(describe(static_cast<AudioChoice>(99)) == "audio: unknown");
}

TEST_CASE("The first touch, click or key press unlocks the audio once", "[unit][audio]")
{
    hbm_test::RecordingAudio audio;
    FirstGestureFilter filter(audio);
    QObject target;
    target.installEventFilter(&filter);

    QEvent other(QEvent::User);
    QCoreApplication::sendEvent(&target, &other);
    CHECK(audio.unlocks == 0);
    CHECK_FALSE(filter.unlocked());

    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &key);
    CHECK(audio.unlocks == 1);
    CHECK(filter.unlocked());

    QMouseEvent click(QEvent::MouseButtonPress, QPointF(), QPointF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &click);
    CHECK(audio.unlocks == 1);
}

TEST_CASE("Each kind of gesture unlocks the audio", "[unit][audio]")
{
    for (const QEvent::Type type :
         {QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::TouchBegin, QEvent::TouchEnd, QEvent::KeyPress})
    {
        hbm_test::RecordingAudio audio;
        FirstGestureFilter filter(audio);
        QObject target;
        target.installEventFilter(&filter);
        QEvent event(type);
        // A bare event of a mouse or key type is enough: the filter reads the type and nothing else.
        QCoreApplication::sendEvent(&target, &event);
        CHECK(audio.unlocks == 1);
    }
}
