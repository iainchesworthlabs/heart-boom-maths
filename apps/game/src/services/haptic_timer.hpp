// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

namespace heart_boom_maths::game
{

/// A single-shot timer, so the rules of BackedHapticsService (the gap between two haptics, the beats of the win) run
/// without Qt in a test. Starting it again replaces what was pending.
class HapticTimer
{
public:
    virtual ~HapticTimer() = default;

    virtual void start(int delay_ms, std::function<void()> on_fire) = 0;
    virtual void stop() = 0;
};

} // namespace heart_boom_maths::game
