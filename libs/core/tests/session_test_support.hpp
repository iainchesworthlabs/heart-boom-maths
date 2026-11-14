// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Helpers shared by the session tests. They read the question from view() and never assume what the question
// generator deals, so they work with the placeholder and with the real one.

#include "heart_boom_maths/session.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace hbm_test
{

using namespace heart_boom_maths;

inline void answer_right(GameSession& s)
{
    s.answer(s.view().question.correct_tile);
}

/// The first tile that is wrong and still on the board, or -1.
inline int wrong_tile(const GameSession& s)
{
    const SessionView& v = s.view();
    for (int i = 0; i < static_cast<int>(v.question.tiles.size()); ++i)
    {
        if (i != v.question.correct_tile && ((v.removed_tiles >> i) & 1) == 0)
        {
            return i;
        }
    }
    return -1;
}

inline void answer_wrong(GameSession& s)
{
    s.answer(wrong_tile(s));
}

inline void play_level_right(GameSession& s)
{
    for (int i = 0; i < s.view().questions_per_level; ++i)
    {
        answer_right(s);
    }
}

/// Plays every question right, moving through the level-clear and win screens, until `level` starts. The timer never
/// runs, so every level is perfect with the full timer.
inline void reach_level(GameSession& s, int level)
{
    if (s.view().state == SessionState::IDLE || s.view().state == SessionState::GAME_OVER)
    {
        s.start_game();
    }
    while (s.view().level < level)
    {
        switch (s.view().state)
        {
        case SessionState::PLAYING:
            answer_right(s);
            break;
        case SessionState::LEVEL_CLEAR:
            s.next_level();
            break;
        case SessionState::WON:
            s.keep_going();
            break;
        default:
            return;
        }
    }
}

inline void tick_n(GameSession& s, int count)
{
    for (int i = 0; i < count; ++i)
    {
        s.tick();
    }
}

inline std::vector<Event> events_of(const std::vector<Event>& events, EventKind kind)
{
    std::vector<Event> out;
    std::copy_if(events.begin(), events.end(), std::back_inserter(out),
                 [kind](const Event& e) { return e.kind == kind; });
    return out;
}

inline std::size_t count_of(const std::vector<Event>& events, EventKind kind)
{
    return events_of(events, kind).size();
}

/// One command of a random script. `arg` is a tile index, a tick count or a wallet size, depending on the kind.
enum class Cmd
{
    TICK,
    TICK_BURST,
    ANSWER_RIGHT,
    ANSWER_WRONG,
    ANSWER_ANY,
    HINT,
    PAUSE,
    RESUME,
    QUIT,
    NEXT_LEVEL,
    KEEP_GOING,
    FINISH,
    START,
    SET_WALLET,
};

struct Command
{
    Cmd kind;
    int arg;
};

inline void apply(GameSession& s, const Command& c)
{
    switch (c.kind)
    {
    case Cmd::TICK:
        s.tick();
        break;
    case Cmd::TICK_BURST:
        tick_n(s, c.arg);
        break;
    case Cmd::ANSWER_RIGHT:
        answer_right(s);
        break;
    case Cmd::ANSWER_WRONG:
        answer_wrong(s);
        break;
    case Cmd::ANSWER_ANY:
        s.answer(c.arg);
        break;
    case Cmd::HINT:
        s.use_hint();
        break;
    case Cmd::PAUSE:
        s.pause();
        break;
    case Cmd::RESUME:
        s.resume();
        break;
    case Cmd::QUIT:
        s.quit();
        break;
    case Cmd::NEXT_LEVEL:
        s.next_level();
        break;
    case Cmd::KEEP_GOING:
        s.keep_going();
        break;
    case Cmd::FINISH:
        s.finish();
        break;
    case Cmd::START:
        s.start_game();
        break;
    case Cmd::SET_WALLET:
        s.set_wallet(c.arg);
        break;
    }
}

/// A script from its own generator, so a seed always gives the same commands. The mix leans towards right answers and
/// single ticks so that scripts clear levels, win and enter endless play, with enough bursts, wrong answers and quits
/// to end games as well.
inline std::vector<Command> random_script(std::uint64_t seed, int length)
{
    Rng rng(seed);
    std::vector<Command> script;
    script.reserve(static_cast<std::size_t>(length));
    for (int i = 0; i < length; ++i)
    {
        const int roll = rng.between(0, 999);
        Command c{Cmd::TICK, 0};
        if (roll < 250)
        {
            c.kind = Cmd::TICK;
        }
        else if (roll < 275)
        {
            c = {Cmd::TICK_BURST, rng.between(1, 1500)};
        }
        else if (roll < 600)
        {
            c.kind = Cmd::ANSWER_RIGHT;
        }
        else if (roll < 660)
        {
            c.kind = Cmd::ANSWER_WRONG;
        }
        else if (roll < 690)
        {
            c = {Cmd::ANSWER_ANY, rng.between(-2, 5)};
        }
        else if (roll < 740)
        {
            c.kind = Cmd::HINT;
        }
        else if (roll < 780)
        {
            c.kind = Cmd::PAUSE;
        }
        else if (roll < 840)
        {
            c.kind = Cmd::RESUME;
        }
        else if (roll < 845)
        {
            c.kind = Cmd::QUIT;
        }
        else if (roll < 920)
        {
            c.kind = Cmd::NEXT_LEVEL;
        }
        else if (roll < 960)
        {
            c.kind = Cmd::KEEP_GOING;
        }
        else if (roll < 962)
        {
            c.kind = Cmd::FINISH;
        }
        else if (roll < 990)
        {
            c.kind = Cmd::START;
        }
        else
        {
            c = {Cmd::SET_WALLET, rng.between(-5, 12)};
        }
        script.push_back(c);
    }
    return script;
}

} // namespace hbm_test
