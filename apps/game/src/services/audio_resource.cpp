// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/audio_resource.hpp"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <algorithm>

namespace heart_boom_maths::game
{

std::optional<ClipBytes> read_audio_clip(const AudioClip& clip)
{
    const QString prefix =
        QString::fromUtf8(AUDIO_RESOURCE_PREFIX.data(), static_cast<qsizetype>(AUDIO_RESOURCE_PREFIX.size()));
    const QString name = QString::fromUtf8(clip.file.data(), static_cast<qsizetype>(clip.file.size()));
    QFile file(prefix + name);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty())
    {
        return std::nullopt;
    }
    ClipBytes result(static_cast<std::size_t>(bytes.size()));
    std::ranges::transform(bytes, result.begin(), [](char c) { return static_cast<std::byte>(c); });
    return result;
}

} // namespace heart_boom_maths::game
