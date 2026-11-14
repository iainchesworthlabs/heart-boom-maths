// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/haptics_service.hpp"

#include <array>
#include <cstdint>

namespace heart_boom_maths::game
{

// What each system is asked for, for each haptic. The choices are plain C++ here so every desktop test pins them; the
// files of src/platform/<os>/ only hand the answer to the system.

// Android

/// The lowest Android level of the application (apps/game/android, cmake/HbmAndroid.cmake).
inline constexpr int ANDROID_MIN_SDK = 28;
/// Android 11: the first with the CONFIRM and REJECT constants.
inline constexpr int ANDROID_SDK_WITH_CONFIRM_REJECT = 30;

/// The android.view.HapticFeedbackConstants value to give View.performHapticFeedback(int) for a beat, on the Android
/// level `sdk_int`. Only constants that exist on that level are used: CONFIRM and REJECT from level 30, and the older
/// KEYBOARD_TAP, VIRTUAL_KEY, CLOCK_TICK and LONG_PRESS below it. No flag is passed, so the system's own touch-feedback
/// setting still decides (see docs/haptics.md).
[[nodiscard]] int android_feedback_constant(HapticId id, int sdk_int) noexcept;

// iOS and iPadOS

/// The generators of UIKit, by what they are called there.
enum class IosFeedback : std::uint8_t
{
    SELECTION,      // UISelectionFeedbackGenerator
    IMPACT_SOFT,    // UIImpactFeedbackGenerator, UIImpactFeedbackStyleSoft
    IMPACT_LIGHT,   // ... Light
    IMPACT_RIGID,   // ... Rigid (crisp)
    IMPACT_MEDIUM,  // ... Medium
    IMPACT_HEAVY,   // ... Heavy
    NOTIFY_SUCCESS, // UINotificationFeedbackGenerator, UINotificationFeedbackTypeSuccess
    NOTIFY_WARNING, // ... Warning
    NOTIFY_ERROR,   // ... Error
};

[[nodiscard]] IosFeedback ios_feedback(HapticId id) noexcept;

// The web

/// A pattern for `navigator.vibrate`: up to three durations in milliseconds, vibrate then pause then vibrate. A
/// trailing 0 is not part of it.
using VibrationPattern = std::array<int, 3>;

[[nodiscard]] VibrationPattern web_vibration(HapticId id) noexcept;

} // namespace heart_boom_maths::game
