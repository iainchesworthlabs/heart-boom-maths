// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/audio_factory.hpp"

#include "services/backed_audio_service.hpp"

#include <utility>

namespace heart_boom_maths::game
{

namespace
{

bool is_display_less(std::string_view qt_platform)
{
    return qt_platform == "offscreen" || qt_platform == "minimal" || qt_platform == "minimalegl";
}

AudioSelection silent(AudioChoice choice)
{
    return AudioSelection{std::make_unique<NullAudioService>(), choice};
}

} // namespace

AudioSelection make_audio_service(const AudioOptions& options,
                                  const std::function<std::unique_ptr<AudioBackend>()>& make_backend)
{
    if (options.smoke_test)
    {
        return silent(AudioChoice::SILENT_SMOKE_TEST);
    }
    if (is_display_less(options.qt_platform))
    {
        return silent(AudioChoice::SILENT_NO_DISPLAY);
    }
    std::unique_ptr<AudioBackend> backend = make_backend ? make_backend() : nullptr;
    if (!backend)
    {
        return silent(AudioChoice::SILENT_NO_DEVICE);
    }
    return AudioSelection{std::make_unique<BackedAudioService>(std::move(backend), options.requires_unlock),
                          AudioChoice::DEVICE};
}

std::string_view describe(AudioChoice choice)
{
    switch (choice)
    {
    case AudioChoice::DEVICE:
        return "audio: sound device";
    case AudioChoice::SILENT_SMOKE_TEST:
        return "audio: silent (smoke test)";
    case AudioChoice::SILENT_NO_DISPLAY:
        return "audio: silent (no display, so no sound device)";
    case AudioChoice::SILENT_NO_DEVICE:
        return "audio: silent (no Qt Multimedia in this build, or no output device)";
    }
    return "audio: unknown";
}

} // namespace heart_boom_maths::game
