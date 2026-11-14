// SPDX-License-Identifier: GPL-3.0-or-later
#include "event_haptics.hpp"

#include "services/haptic_patterns.hpp"

namespace heart_boom_maths::game
{

std::optional<HapticId> haptic_for_event(const Event& event) noexcept
{
    switch (event.kind)
    {
    case EventKind::ANSWER_CORRECT:
        return event.multiplier_rose ? HapticId::MULTIPLIER_UP : HapticId::RIGHT;
    case EventKind::ANSWER_WRONG:
        return HapticId::WRONG;
    case EventKind::HINT_USED:
        return HapticId::HINT;
    case EventKind::ALERT_ENTERED:
        return HapticId::ALERT;
    case EventKind::LEVEL_CLEARED:
        return HapticId::LEVEL_CLEAR;
    case EventKind::GAME_WON:
        return HapticId::WIN;
    case EventKind::GAME_OVER:
        if (event.reason == GameOverReason::BOMB)
        {
            return HapticId::BOOM;
        }
        return std::nullopt;
    case EventKind::LEVEL_STARTED:
    case EventKind::QUESTION_SHOWN:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<HapticId> haptic_for_events(std::span<const Event> events) noexcept
{
    std::optional<HapticId> strongest;
    for (const Event& event : events)
    {
        const std::optional<HapticId> id = haptic_for_event(event);
        if (id.has_value() && (!strongest.has_value() || haptic_priority(*id) > haptic_priority(*strongest)))
        {
            strongest = id;
        }
    }
    return strongest;
}

} // namespace heart_boom_maths::game
