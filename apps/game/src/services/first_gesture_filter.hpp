// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/audio_service.hpp"

#include <QObject>

namespace heart_boom_maths::game
{

/// Calls AudioService::unlock() once, on the first touch, click or key press the application receives. Installed on the
/// application object; it never consumes an event. A browser starts no sound before such a gesture, and it counts the
/// gesture only when the sound starts inside its handler, so this runs in the event itself rather than on a timer.
class FirstGestureFilter final : public QObject
{
    Q_OBJECT

public:
    explicit FirstGestureFilter(AudioService& audio, QObject* parent = nullptr);

    [[nodiscard]] bool unlocked() const noexcept
    {
        return m_Unlocked;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    AudioService& m_Audio;
    bool m_Unlocked = false;
};

} // namespace heart_boom_maths::game
