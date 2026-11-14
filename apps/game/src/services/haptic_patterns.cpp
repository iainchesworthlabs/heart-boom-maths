// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/haptic_patterns.hpp"

#include <array>

namespace heart_boom_maths::game
{

namespace
{

constexpr std::array<HapticBeat, 1> TAP_BEATS = {HapticBeat{0, HapticId::TAP}};
constexpr std::array<HapticBeat, 1> RIGHT_BEATS = {HapticBeat{0, HapticId::RIGHT}};
constexpr std::array<HapticBeat, 1> WRONG_BEATS = {HapticBeat{0, HapticId::WRONG}};
constexpr std::array<HapticBeat, 1> HINT_BEATS = {HapticBeat{0, HapticId::HINT}};
constexpr std::array<HapticBeat, 1> MULTIPLIER_UP_BEATS = {HapticBeat{0, HapticId::MULTIPLIER_UP}};
constexpr std::array<HapticBeat, 1> ALERT_BEATS = {HapticBeat{0, HapticId::ALERT}};
constexpr std::array<HapticBeat, 1> LEVEL_CLEAR_BEATS = {HapticBeat{0, HapticId::LEVEL_CLEAR}};
constexpr std::array<HapticBeat, 1> BOOM_BEATS = {HapticBeat{0, HapticId::BOOM}};
constexpr std::array<HapticBeat, 3> WIN_BEATS = {
    HapticBeat{0, HapticId::RIGHT},
    HapticBeat{110, HapticId::MULTIPLIER_UP},
    HapticBeat{220, HapticId::LEVEL_CLEAR},
};

} // namespace

std::span<const HapticBeat> haptic_beats(HapticId id) noexcept
{
    switch (id)
    {
    case HapticId::TAP:
        return TAP_BEATS;
    case HapticId::RIGHT:
        return RIGHT_BEATS;
    case HapticId::WRONG:
        return WRONG_BEATS;
    case HapticId::HINT:
        return HINT_BEATS;
    case HapticId::MULTIPLIER_UP:
        return MULTIPLIER_UP_BEATS;
    case HapticId::ALERT:
        return ALERT_BEATS;
    case HapticId::LEVEL_CLEAR:
        return LEVEL_CLEAR_BEATS;
    case HapticId::WIN:
        return WIN_BEATS;
    case HapticId::BOOM:
        return BOOM_BEATS;
    }
    return TAP_BEATS;
}

int haptic_priority(HapticId id) noexcept
{
    switch (id)
    {
    case HapticId::TAP:
        return 0;
    case HapticId::HINT:
        return 1;
    case HapticId::RIGHT:
        return 2;
    case HapticId::MULTIPLIER_UP:
        return 3;
    case HapticId::ALERT:
        return 4;
    case HapticId::WRONG:
        return 5;
    case HapticId::LEVEL_CLEAR:
        return 6;
    case HapticId::WIN:
        return 7;
    case HapticId::BOOM:
        return 8;
    }
    return 0;
}

} // namespace heart_boom_maths::game
