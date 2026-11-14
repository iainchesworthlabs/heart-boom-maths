// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/qt_haptic_timer.hpp"

#include <utility>

namespace heart_boom_maths::game
{

QtHapticTimer::QtHapticTimer()
{
    m_Timer.setSingleShot(true);
    m_Timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&m_Timer, &QTimer::timeout,
                     [this]
                     {
                         // Copied first: the callback may start the timer again and so replace itself.
                         const std::function<void()> callback = m_OnFire;
                         if (callback)
                         {
                             callback();
                         }
                     });
}

void QtHapticTimer::start(int delay_ms, std::function<void()> on_fire)
{
    m_OnFire = std::move(on_fire);
    m_Timer.start(delay_ms);
}

void QtHapticTimer::stop()
{
    m_Timer.stop();
    m_OnFire = nullptr;
}

} // namespace heart_boom_maths::game
