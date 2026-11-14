// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/audio_backend.hpp"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QSoundEffect>
#include <QString>
#include <QSysInfo>
#include <QUrl>
#include <map>
#include <utility>

namespace heart_boom_maths::game
{

namespace
{

// Every clip is a small PCM file in the application resources. QSoundEffect decodes it once into memory and starts it
// at once, which suits short effects, and with a loop count it also suits the music beds, so no media decoder or
// network stack is needed, on the web too.
class QtAudioBackend final : public AudioBackend
{
public:
    QtAudioBackend()
    {
        // Loaded up front so the first effect is not late. A clip that fails to load stays silent.
        for (const SoundId id : all_sounds())
        {
            if (const auto clip = clip_for(id))
            {
                m_Effects.emplace(static_cast<int>(id), make_voice(*clip));
            }
        }
        for (const MusicId id : all_music())
        {
            if (const auto clip = clip_for(id))
            {
                m_Music.emplace(static_cast<int>(id), make_voice(*clip));
            }
        }
    }

    void play_effect(SoundId id, const AudioClip&) override
    {
        if (QSoundEffect* voice = find(m_Effects, static_cast<int>(id)))
        {
            voice->stop();
            voice->play();
        }
    }

    void stop_effect(SoundId id) override
    {
        if (QSoundEffect* voice = find(m_Effects, static_cast<int>(id)))
        {
            voice->stop();
        }
    }

    void stop_all_effects() override
    {
        for (auto& entry : m_Effects)
        {
            entry.second->stop();
        }
    }

    void play_music(MusicId id, const AudioClip&) override
    {
        stop_music();
        if (QSoundEffect* voice = find(m_Music, static_cast<int>(id)))
        {
            voice->play();
        }
    }

    void stop_music() override
    {
        for (auto& entry : m_Music)
        {
            entry.second->stop();
        }
    }

private:
    using Voices = std::map<int, std::unique_ptr<QSoundEffect>>;

    static std::unique_ptr<QSoundEffect> make_voice(const AudioClip& clip)
    {
        auto voice = std::make_unique<QSoundEffect>();
        const QString file = QString::fromUtf8(clip.file.data(), static_cast<qsizetype>(clip.file.size()));
        const QString prefix =
            QString::fromUtf8(AUDIO_RESOURCE_PREFIX.data(), static_cast<qsizetype>(AUDIO_RESOURCE_PREFIX.size()));
        voice->setSource(QUrl(QStringLiteral("qrc") + prefix + file));
        voice->setLoopCount(clip.looping ? QSoundEffect::Infinite : 1);
        voice->setVolume(static_cast<float>(clip.volume_percent) / 100.0F);
        return voice;
    }

    static QSoundEffect* find(const Voices& voices, int key)
    {
        const auto found = voices.find(key);
        return found == voices.end() ? nullptr : found->second.get();
    }

    Voices m_Effects;
    Voices m_Music;
};

} // namespace

std::unique_ptr<AudioBackend> make_device_audio_backend()
{
    // A browser lists its devices only after the first gesture, so the web build does not look for one here.
    if (QSysInfo::productType() != QStringLiteral("wasm") && QMediaDevices::audioOutputs().isEmpty())
    {
        return nullptr;
    }
    return std::make_unique<QtAudioBackend>();
}

} // namespace heart_boom_maths::game
