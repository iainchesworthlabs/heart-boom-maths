// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/clock.hpp"
#include "services/haptic_backend.hpp"
#include "services/haptic_patterns.hpp"
#include "services/haptic_timer.hpp"
#include "services/haptics_service.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace heart_boom_maths::game
{

/// Two beats never come closer than this. The system would merge them into one smear, and a cue that is a few
/// milliseconds late is not felt as late. An app-layer feel, to be judged on a device and adjusted there.
inline constexpr int HAPTIC_MIN_GAP_MS = 40;

/// A TAP after anything, or after another TAP, is dropped inside this time: a child drumming on a button gets a tick
/// now and then, not a buzz.
inline constexpr int HAPTIC_TAP_GAP_MS = 120;

/// The HapticsService every real platform uses: the rules of when a haptic may be felt, over a HapticBackend that makes
/// it.
///
///   nothing           while the Settings switch is off, while suspended (the game is paused or the app is in the
///                     background) or on a device that cannot. A cue asked for then is dropped, not queued
///   spacing           a cue that arrives within HAPTIC_MIN_GAP_MS of the last beat waits out the gap, and a cue that
///                     comes while another waits replaces it only if it is stronger (haptic_priority). So the wrong
///                     answer that ends a level, or the bomb that follows it, is the one felt
///   TAP               light, and only when nothing else is going on: dropped inside HAPTIC_TAP_GAP_MS of the last beat
///   WIN               its three beats go out on the timer; a stronger cue (the bomb) may replace it, nothing else
///   suspend, off      stop the timer, drop what was waiting and tell the backend to cancel
class BackedHapticsService final : public HapticsService
{
public:
    /// Nothing is owned: the backend, the clock and the timer must outlive the service.
    BackedHapticsService(HapticBackend& backend, const Clock& clock, HapticTimer& timer);

    [[nodiscard]] bool available() const override;
    void play(HapticId id) override;
    void set_enabled(bool enabled) override;
    void suspend() override;
    void resume() override;

private:
    [[nodiscard]] bool usable() const;
    [[nodiscard]] std::int64_t now_ms() const;
    [[nodiscard]] std::int64_t since_last_ms(std::int64_t now) const;
    void stop_all();
    void begin(HapticId id);
    void run_next_beat();
    void fire(HapticId beat);

    HapticBackend& m_Backend;
    const Clock& m_Clock;
    HapticTimer& m_Timer;
    bool m_Enabled = true;
    bool m_Suspended = false;
    std::optional<std::int64_t> m_LastFiredMs;
    std::span<const HapticBeat> m_Beats; // what is still to go out, empty when idle
    std::size_t m_NextBeat = 0;
    HapticId m_Running = HapticId::TAP;
};

} // namespace heart_boom_maths::game
