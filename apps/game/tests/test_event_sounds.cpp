// SPDX-License-Identifier: GPL-3.0-or-later
#include "event_sounds.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;

namespace
{

Event make(EventKind kind)
{
    Event e;
    e.kind = kind;
    return e;
}

} // namespace

TEST_CASE("Events call for the sounds of the feedback table", "[unit][sounds]")
{
    CHECK(sounds_for_event(make(EventKind::ANSWER_CORRECT)) == std::vector<SoundId>{SoundId::COIN_CHIME});

    Event streak = make(EventKind::ANSWER_CORRECT);
    streak.multiplier_rose = true;
    CHECK(sounds_for_event(streak) == std::vector<SoundId>{SoundId::COIN_CHIME, SoundId::RISING_TONE});

    CHECK(sounds_for_event(make(EventKind::ANSWER_WRONG)) == std::vector<SoundId>{SoundId::SOFT_BUZZ});
    CHECK(sounds_for_event(make(EventKind::HINT_USED)) == std::vector<SoundId>{SoundId::WHOOSH});
    CHECK(sounds_for_event(make(EventKind::ALERT_ENTERED)) == std::vector<SoundId>{SoundId::TICKING});
    CHECK(sounds_for_event(make(EventKind::GAME_OVER)) == std::vector<SoundId>{SoundId::CARTOON_POP});
    CHECK(sounds_for_event(make(EventKind::LEVEL_CLEARED)) == std::vector<SoundId>{SoundId::FANFARE});
}

TEST_CASE("Level starts, question starts and the win add no sound of their own", "[unit][sounds]")
{
    CHECK(sounds_for_event(make(EventKind::LEVEL_STARTED)).empty());
    CHECK(sounds_for_event(make(EventKind::QUESTION_SHOWN)).empty());
    CHECK(sounds_for_event(make(EventKind::GAME_WON)).empty());
}
