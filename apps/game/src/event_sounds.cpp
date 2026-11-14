// SPDX-License-Identifier: GPL-3.0-or-later
#include "event_sounds.hpp"

namespace heart_boom_maths::game
{

std::vector<SoundId> sounds_for_event(const Event& event)
{
    switch (event.kind)
    {
    case EventKind::ANSWER_CORRECT:
        if (event.multiplier_rose)
        {
            return {SoundId::COIN_CHIME, SoundId::RISING_TONE};
        }
        return {SoundId::COIN_CHIME};
    case EventKind::ANSWER_WRONG:
        return {SoundId::SOFT_BUZZ};
    case EventKind::HINT_USED:
        return {SoundId::WHOOSH};
    case EventKind::ALERT_ENTERED:
        return {SoundId::TICKING};
    case EventKind::GAME_OVER:
        return {SoundId::CARTOON_POP};
    case EventKind::LEVEL_CLEARED:
        return {SoundId::FANFARE};
    case EventKind::LEVEL_STARTED:
    case EventKind::QUESTION_SHOWN:
    case EventKind::GAME_WON:
        return {};
    }
    return {};
}

} // namespace heart_boom_maths::game
