// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/backed_haptics_service.hpp"

#include <algorithm>

namespace heart_boom_maths::game
{

namespace
{

constexpr std::int64_t NS_PER_MS = 1'000'000;

} // namespace

BackedHapticsService::BackedHapticsService(HapticBackend& backend, const Clock& clock, HapticTimer& timer)
    : m_Backend(backend), m_Clock(clock), m_Timer(timer)
{
}

bool BackedHapticsService::available() const
{
    return m_Backend.available();
}

void BackedHapticsService::play(HapticId id)
{
    if (!usable())
    {
        return;
    }
    if (id == HapticId::TAP)
    {
        if (!m_Beats.empty() || since_last_ms(now_ms()) < HAPTIC_TAP_GAP_MS)
        {
            return;
        }
        fire(HapticId::TAP);
        return;
    }
    // Something stronger or as strong is already on its way: the new cue adds nothing to it.
    if (!m_Beats.empty() && haptic_priority(id) <= haptic_priority(m_Running))
    {
        return;
    }
    begin(id);
}

void BackedHapticsService::set_enabled(bool enabled)
{
    if (m_Enabled == enabled)
    {
        return;
    }
    m_Enabled = enabled;
    if (!enabled)
    {
        stop_all();
    }
}

void BackedHapticsService::suspend()
{
    if (m_Suspended)
    {
        return;
    }
    m_Suspended = true;
    stop_all();
}

void BackedHapticsService::resume()
{
    m_Suspended = false;
}

bool BackedHapticsService::usable() const
{
    return m_Enabled && !m_Suspended && m_Backend.available();
}

std::int64_t BackedHapticsService::now_ms() const
{
    return m_Clock.now_ns() / NS_PER_MS;
}

std::int64_t BackedHapticsService::since_last_ms(std::int64_t now) const
{
    // Nothing has fired yet: as long ago as can be, so no gap applies.
    return m_LastFiredMs.has_value() ? now - *m_LastFiredMs : INT32_MAX;
}

void BackedHapticsService::stop_all()
{
    m_Timer.stop();
    m_Beats = {};
    m_NextBeat = 0;
    m_Backend.cancel();
}

void BackedHapticsService::begin(HapticId id)
{
    m_Timer.stop();
    m_Beats = haptic_beats(id);
    m_NextBeat = 0;
    m_Running = id;
    const std::int64_t wait = HAPTIC_MIN_GAP_MS - since_last_ms(now_ms());
    if (wait <= 0)
    {
        run_next_beat();
        return;
    }
    m_Timer.start(static_cast<int>(wait), [this] { run_next_beat(); });
}

void BackedHapticsService::run_next_beat()
{
    // A timer that fires after the service was stopped finds nothing to do.
    if (m_NextBeat >= m_Beats.size() || !usable())
    {
        m_Beats = {};
        return;
    }
    const HapticBeat beat = m_Beats[m_NextBeat];
    ++m_NextBeat;
    fire(beat.id);
    if (m_NextBeat >= m_Beats.size())
    {
        m_Beats = {};
        return;
    }
    const int next_delay = std::max(m_Beats[m_NextBeat].delay_ms - beat.delay_ms, HAPTIC_MIN_GAP_MS);
    m_Timer.start(next_delay, [this] { run_next_beat(); });
}

void BackedHapticsService::fire(HapticId beat)
{
    m_Backend.fire(beat);
    m_LastFiredMs = now_ms();
}

} // namespace heart_boom_maths::game
