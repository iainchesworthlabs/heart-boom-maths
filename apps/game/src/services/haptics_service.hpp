// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace heart_boom_maths::game
{

/// What a haptic means, not what a device calls it: each platform picks its own nearest feel (docs/haptics.md). A
/// haptic is an extra on top of a cue that already shows on screen, never the only one. The order here is not the order
/// of strength; haptic_priority() says which cue wins when two want the same moment.
enum class HapticId : std::uint8_t
{
    TAP,           // a light selection tick: a button, a key of the gate keypad, a switch
    RIGHT,         // a right answer: light and crisp
    WRONG,         // a wrong answer: a short error buzz
    HINT,          // a hint bought: soft
    MULTIPLIER_UP, // a streak step: a medium bump
    ALERT,         // the timer entered the last seconds: a warning, once
    LEVEL_CLEAR,   // a level cleared: success
    WIN,           // the game won: a short run of three small beats
    BOOM,          // the bomb went off: heavy, once
};

class HapticsService
{
public:
    virtual ~HapticsService() = default;

    /// True only where the device can do it: a phone with a vibration motor, a browser that has `navigator.vibrate` on
    /// a touch screen. Settings shows the switch only then.
    [[nodiscard]] virtual bool available() const = 0;

    virtual void play(HapticId id) = 0;

    /// The Settings switch. Turning it off stops whatever is still to come at once.
    virtual void set_enabled(bool enabled) = 0;

    /// The game is paused or the app left the foreground: nothing plays, and what was still to come is dropped, until
    /// resume().
    virtual void suspend() = 0;
    virtual void resume() = 0;
};

/// Plays nothing. The service on the desktop and in tests that do not look at haptics.
class NullHapticsService final : public HapticsService
{
public:
    [[nodiscard]] bool available() const override
    {
        return false;
    }

    void play(HapticId) override {}

    void set_enabled(bool) override {}

    void suspend() override {}

    void resume() override {}
};

} // namespace heart_boom_maths::game
