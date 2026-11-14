// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/haptic_timer.hpp"

#include <QTimer>
#include <functional>

namespace heart_boom_maths::game
{

/// The HapticTimer of the application: one QTimer, single shot, on the thread that made it.
class QtHapticTimer final : public HapticTimer
{
public:
    QtHapticTimer();

    void start(int delay_ms, std::function<void()> on_fire) override;
    void stop() override;

private:
    QTimer m_Timer;
    std::function<void()> m_OnFire;
};

} // namespace heart_boom_maths::game
