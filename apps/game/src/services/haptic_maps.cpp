// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/haptic_maps.hpp"

namespace heart_boom_maths::game
{

namespace
{

// android.view.HapticFeedbackConstants. LONG_PRESS is the heaviest of the old ones; CLOCK_TICK the lightest.
constexpr int ANDROID_LONG_PRESS = 0;
constexpr int ANDROID_VIRTUAL_KEY = 1;
constexpr int ANDROID_KEYBOARD_TAP = 3;
constexpr int ANDROID_CLOCK_TICK = 4;
constexpr int ANDROID_CONFIRM = 16;
constexpr int ANDROID_REJECT = 17;

} // namespace

int android_feedback_constant(HapticId id, int sdk_int) noexcept
{
    const bool has_confirm_reject = sdk_int >= ANDROID_SDK_WITH_CONFIRM_REJECT;
    switch (id)
    {
    case HapticId::TAP:
    case HapticId::HINT:
        return ANDROID_CLOCK_TICK;
    case HapticId::RIGHT:
        return has_confirm_reject ? ANDROID_CONFIRM : ANDROID_KEYBOARD_TAP;
    case HapticId::WRONG:
        return has_confirm_reject ? ANDROID_REJECT : ANDROID_LONG_PRESS;
    case HapticId::MULTIPLIER_UP:
        return ANDROID_VIRTUAL_KEY;
    case HapticId::ALERT:
        return ANDROID_LONG_PRESS;
    case HapticId::LEVEL_CLEAR:
    case HapticId::WIN: // the service splits a win into beats; this is only the answer if one arrives whole
        return has_confirm_reject ? ANDROID_CONFIRM : ANDROID_VIRTUAL_KEY;
    case HapticId::BOOM:
        return ANDROID_LONG_PRESS;
    }
    return ANDROID_CLOCK_TICK;
}

IosFeedback ios_feedback(HapticId id) noexcept
{
    switch (id)
    {
    case HapticId::TAP:
        return IosFeedback::SELECTION;
    case HapticId::RIGHT:
        return IosFeedback::IMPACT_RIGID;
    case HapticId::WRONG:
        return IosFeedback::NOTIFY_ERROR;
    case HapticId::HINT:
        return IosFeedback::IMPACT_SOFT;
    case HapticId::MULTIPLIER_UP:
        return IosFeedback::IMPACT_MEDIUM;
    case HapticId::ALERT:
        return IosFeedback::NOTIFY_WARNING;
    case HapticId::LEVEL_CLEAR:
    case HapticId::WIN:
        return IosFeedback::NOTIFY_SUCCESS;
    case HapticId::BOOM:
        return IosFeedback::IMPACT_HEAVY;
    }
    return IosFeedback::SELECTION;
}

VibrationPattern web_vibration(HapticId id) noexcept
{
    switch (id)
    {
    case HapticId::TAP:
        return {8, 0, 0};
    case HapticId::RIGHT:
        return {14, 0, 0};
    case HapticId::WRONG:
        return {25, 35, 25};
    case HapticId::HINT:
        return {10, 0, 0};
    case HapticId::MULTIPLIER_UP:
        return {30, 0, 0};
    case HapticId::ALERT:
        return {60, 0, 0};
    case HapticId::LEVEL_CLEAR:
    case HapticId::WIN:
        return {20, 40, 40};
    case HapticId::BOOM:
        return {160, 0, 0};
    }
    return {8, 0, 0};
}

} // namespace heart_boom_maths::game
