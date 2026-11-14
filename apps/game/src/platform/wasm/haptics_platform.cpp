// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/haptics_platform.hpp"

#include "services/haptic_maps.hpp"

#include <emscripten.h>

#include <memory>

// The whole of the browser haptics, and nothing in it decides anything: the pattern for each beat is web_vibration()
// (plain C++, tested on every desktop) and whether and when one plays is BackedHapticsService's. `navigator.vibrate`
// asks for no permission, but a browser lets a page use it only after the player has touched it, which every cue here
// follows. A browser may refuse the call, so each is JavaScript inside try and catch (EM_JS is compiled into the page's
// script, so the page's content security policy needs no 'unsafe-eval').

namespace heart_boom_maths::game::platform
{
namespace
{

// clang-format off
// A touch screen and a vibrate function. Desktop Chrome has the function and no motor, so the pointer is asked too:
// "coarse" is a finger, and a laptop with a touch screen still has a fine pointer as its main one.
EM_JS(int, hbm_vibration_available, (), {
    try
    {
        return (typeof navigator !== "undefined" && typeof navigator.vibrate === "function" &&
                typeof window.matchMedia === "function" && window.matchMedia("(pointer: coarse)").matches)
                   ? 1
                   : 0;
    }
    catch (error)
    {
        return 0;
    }
});

// One to three durations: vibrate, pause, vibrate. A zero ends the list.
EM_JS(void, hbm_vibrate, (int first, int pause, int second), {
    try
    {
        const pattern = [first];
        if (pause > 0)
        {
            pattern.push(pause);
            if (second > 0)
            {
                pattern.push(second);
            }
        }
        navigator.vibrate(pattern);
    }
    catch (error)
    {
        // The browser said no: not worth a message to the player.
    }
});

EM_JS(void, hbm_vibrate_cancel, (), {
    try
    {
        navigator.vibrate(0);
    }
    catch (error)
    {
    }
});
// clang-format on

class NavigatorVibrateBackend final : public HapticBackend
{
public:
    NavigatorVibrateBackend() : m_Available(hbm_vibration_available() != 0) {}

    [[nodiscard]] bool available() const override
    {
        return m_Available;
    }

    void fire(HapticId beat) override
    {
        const VibrationPattern pattern = web_vibration(beat);
        hbm_vibrate(pattern[0], pattern[1], pattern[2]);
    }

    void cancel() override
    {
        hbm_vibrate_cancel();
    }

private:
    bool m_Available;
};

} // namespace

std::unique_ptr<HapticBackend> make_haptic_backend()
{
    return std::make_unique<NavigatorVibrateBackend>();
}

} // namespace heart_boom_maths::game::platform
