// SPDX-License-Identifier: GPL-3.0-or-later

// The text-mode harness: plays a whole game of Heart Boom Maths in a terminal, from a seed and either a script of
// commands (a replay file) or a policy that plays by itself. It is how a human checks the rules before any screen
// exists, and how the golden replays in tests/data/replays are made. It reads no clock: a tick is a number in the
// script.

#include "heart_boom_maths/replay.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/session.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace heart_boom_maths;

namespace
{

constexpr std::uint64_t DEFAULT_SEED = 20261114;
constexpr int DEFAULT_THINK_TICKS = 30; // a second and a half to read and tap
constexpr int MAX_AUTO_STEPS = 400000;  // a stop for a policy that never reaches its goal

const char* const USAGE =
    "Heart Boom Maths text harness\n"
    "\n"
    "  heart_boom_maths_harness --script FILE|-  [--seed N]\n"
    "  heart_boom_maths_harness --auto POLICY    [--seed N] [--wallet N] [--won-once] [--think TICKS]\n"
    "                                            [--clear-level N] [--endless-to N]\n"
    "  heart_boom_maths_harness --regenerate DIR | --check DIR\n"
    "\n"
    "  --script FILE|-     play the commands of a replay file (or of standard input)\n"
    "  --auto POLICY       play by itself. POLICY is one of\n"
    "                        perfect            every question right, straight away\n"
    "                        always-wrong       every question wrong\n"
    "                        random SEED        a mix of right, wrong, hints and pauses from its own generator\n"
    "                        hint-then-right    a hint when the wallet can pay, then the right tile\n"
    "                        stall N            N questions right, then wait for the bomb\n"
    "  --seed N            the game's seed (default 20261114)\n"
    "  --wallet N          coins in the wallet before the game (default 0)\n"
    "  --won-once          the win bonus was paid in an earlier game\n"
    "  --think TICKS       ticks a policy takes to answer; 20 ticks are one second (default 30)\n"
    "  --clear-level N     stop when level N has been cleared\n"
    "  --endless-to N      keep going past level 12 until level N starts, then pause and quit\n"
    "  --record FILE       write the run as a replay file with fresh hashes\n"
    "  --note TEXT         the note written into a recorded replay\n"
    "  --quiet             print only the final line\n"
    "  --regenerate DIR    rewrite every replay in DIR from its recipe (after a deliberate rule change)\n"
    "  --check DIR         fail when a replay in DIR differs from what its recipe makes now\n"
    "\n"
    "The last line of a run is: FINAL score=<n> level=<n> events_hash=<hex> final_hash=<hex>\n"
    "Tiles are numbered from 0, the same as the \"tile\" of an answer in a replay file.\n";

struct Options
{
    std::optional<std::string> script;
    std::optional<std::string> policy;
    std::uint64_t policy_seed = 0; // random
    int stall_after = 0;           // stall
    std::optional<std::uint64_t> seed;
    std::int64_t wallet = 0;
    bool won_once = false;
    int think = DEFAULT_THINK_TICKS;
    int clear_level = 0;
    int endless_to = 0;
    std::optional<std::string> record;
    std::string note;
    bool quiet = false;
    std::optional<std::string> regenerate_dir;
    std::optional<std::string> check_dir;
};

std::unexpected<std::string> fail(std::string message)
{
    return std::unexpected(std::move(message));
}

template <typename T>
std::optional<T> to_number(std::string_view text)
{
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
    {
        return std::nullopt;
    }
    return value;
}

std::vector<std::string> split_words(const std::string& text)
{
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;
    while (stream >> word)
    {
        words.push_back(word);
    }
    return words;
}

std::expected<Options, std::string> parse_args(const std::vector<std::string>& args)
{
    Options o;
    std::size_t i = 0;
    const auto value = [&](std::string_view name) -> std::expected<std::string, std::string>
    {
        if (i + 1 >= args.size())
        {
            return fail(std::string(name) + " needs a value");
        }
        return args[++i];
    };
    const auto number = [&]<typename T>(std::string_view name, T& target) -> std::optional<std::string>
    {
        const auto text = value(name);
        if (!text)
        {
            return text.error();
        }
        const auto parsed = to_number<T>(*text);
        if (!parsed)
        {
            return std::string(name) + " needs a whole number, not \"" + *text + "\"";
        }
        target = *parsed;
        return std::nullopt;
    };

    for (; i < args.size(); ++i)
    {
        const std::string& arg = args[i];
        std::optional<std::string> problem;
        if (arg == "--script")
        {
            const auto text = value(arg);
            if (!text)
            {
                return fail(text.error());
            }
            o.script = *text;
        }
        else if (arg == "--auto")
        {
            const auto text = value(arg);
            if (!text)
            {
                return fail(text.error());
            }
            o.policy = *text;
            if (*text == "random")
            {
                problem = number("--auto random", o.policy_seed);
            }
            else if (*text == "stall")
            {
                problem = number("--auto stall", o.stall_after);
            }
            else if (*text != "perfect" && *text != "always-wrong" && *text != "hint-then-right")
            {
                return fail("unknown policy \"" + *text + "\"");
            }
        }
        else if (arg == "--seed")
        {
            std::uint64_t seed = 0;
            problem = number(arg, seed);
            o.seed = seed;
        }
        else if (arg == "--wallet")
        {
            problem = number(arg, o.wallet);
        }
        else if (arg == "--won-once")
        {
            o.won_once = true;
        }
        else if (arg == "--think")
        {
            problem = number(arg, o.think);
        }
        else if (arg == "--clear-level")
        {
            problem = number(arg, o.clear_level);
        }
        else if (arg == "--endless-to")
        {
            problem = number(arg, o.endless_to);
        }
        else if (arg == "--record" || arg == "--note" || arg == "--regenerate" || arg == "--check")
        {
            const auto text = value(arg);
            if (!text)
            {
                return fail(text.error());
            }
            if (arg == "--record")
            {
                o.record = *text;
            }
            else if (arg == "--note")
            {
                o.note = *text;
            }
            else if (arg == "--regenerate")
            {
                o.regenerate_dir = *text;
            }
            else
            {
                o.check_dir = *text;
            }
        }
        else if (arg == "--quiet")
        {
            o.quiet = true;
        }
        else
        {
            return fail("unknown option \"" + arg + "\"");
        }
        if (problem)
        {
            return fail(*problem);
        }
    }

    const int modes = (o.script ? 1 : 0) + (o.policy ? 1 : 0) + (o.regenerate_dir ? 1 : 0) + (o.check_dir ? 1 : 0);
    if (modes != 1)
    {
        return fail("choose exactly one of --script, --auto, --regenerate and --check");
    }
    if (o.think < 0 || o.wallet < 0 || o.clear_level < 0 || o.endless_to < 0 || o.stall_after < 0)
    {
        return fail("numbers must not be negative");
    }
    return o;
}

/// The arguments that make this run again, written into a recorded replay so --regenerate needs nothing else.
std::string recipe_of(const Options& o)
{
    std::string text = "--seed " + std::to_string(o.seed.value_or(DEFAULT_SEED));
    if (o.wallet != 0)
    {
        text += " --wallet " + std::to_string(o.wallet);
    }
    if (o.won_once)
    {
        text += " --won-once";
    }
    if (o.think != DEFAULT_THINK_TICKS)
    {
        text += " --think " + std::to_string(o.think);
    }
    if (o.clear_level != 0)
    {
        text += " --clear-level " + std::to_string(o.clear_level);
    }
    if (o.endless_to != 0)
    {
        text += " --endless-to " + std::to_string(o.endless_to);
    }
    text += " --auto " + *o.policy;
    if (*o.policy == "random")
    {
        text += " " + std::to_string(o.policy_seed);
    }
    else if (*o.policy == "stall")
    {
        text += " " + std::to_string(o.stall_after);
    }
    return text;
}

// ---------------------------------------------------------------------------------------------------------------
// Words for what happened

std::string seconds_text(int ticks)
{
    const int per_second = Tuning::defaults().ticks_per_second;
    const int sign = ticks < 0 ? -1 : 1;
    const int magnitude = ticks * sign;
    const int hundredths = magnitude % per_second * 100 / per_second;
    return std::string(sign < 0 ? "-" : "") + std::to_string(magnitude / per_second) + "." +
           (hundredths < 10 ? "0" : "") + std::to_string(hundredths) + "s";
}

std::string plural(std::int64_t count, const std::string& word)
{
    return std::to_string(count) + " " + word + (count == 1 ? "" : "s");
}

std::string state_name(SessionState state)
{
    switch (state)
    {
    case SessionState::IDLE:
        return "IDLE";
    case SessionState::PLAYING:
        return "PLAYING";
    case SessionState::PAUSED:
        return "PAUSED";
    case SessionState::LEVEL_CLEAR:
        return "LEVEL CLEAR";
    case SessionState::WON:
        return "WON";
    case SessionState::GAME_OVER:
        return "GAME OVER";
    }
    return "?";
}

std::string reason_name(GameOverReason reason)
{
    switch (reason)
    {
    case GameOverReason::BOMB:
        return "the bomb went off";
    case GameOverReason::QUIT:
        return "quit from the pause screen";
    case GameOverReason::FINISH:
        return "finished from the win screen";
    }
    return "?";
}

/// The number sentence as the child reads it.
std::string sentence_of(const Question& q)
{
    std::string text = std::to_string(q.terms[0]);
    if (q.shape == QuestionShape::MISSING_ADDEND)
    {
        return text + " + ? = " + std::to_string(q.terms[1]);
    }
    for (std::size_t i = 1; i < static_cast<std::size_t>(q.term_count); ++i)
    {
        text += q.operators[i - 1] == Operator::ADD ? " + " : " - ";
        text += std::to_string(q.terms[i]);
    }
    return text + " = ?";
}

std::string tiles_of(const SessionView& v)
{
    std::string text;
    for (std::size_t i = 0; i < v.question.tiles.size(); ++i)
    {
        text += (i == 0 ? "" : "   ") + std::string("[") + std::to_string(i) + "] ";
        text += ((v.removed_tiles >> i) & 1) != 0 ? "--" : std::to_string(v.question.tiles[i]);
    }
    return text;
}

std::string removed_text(int mask)
{
    std::string text;
    for (int i = 0; i < 4; ++i)
    {
        if (((mask >> i) & 1) != 0)
        {
            text += (text.empty() ? "" : " and ") + std::string("tile ") + std::to_string(i);
        }
    }
    return text;
}

/// Prints a run as it happens. One call per tick and per command, so it also serves as a ReplayObserver.
class Printer
{
public:
    Printer(std::ostream& out, bool quiet) : m_Out(out), m_Quiet(quiet) {}

    void operator()(const ReplayCommand* command, const std::vector<Event>& events, const ReplayRunner& runner)
    {
        const SessionView& v = runner.session().view();
        if (!m_Quiet)
        {
            if (command != nullptr)
            {
                m_Out << "> tick " << command->tick << " (" << seconds_text(command->tick) << " of play)  "
                      << replay_action_name(command->action);
                if (command->action == ReplayAction::ANSWER)
                {
                    m_Out << " tile " << command->tile;
                    if (command->tile >= 0 && command->tile < 4)
                    {
                        m_Out << " (" << m_Last.question.tiles[static_cast<std::size_t>(command->tile)] << ")";
                    }
                }
                m_Out << "\n";
            }
            for (const Event& e : events)
            {
                if (command == nullptr)
                {
                    m_Out << "  [tick " << runner.tick_count() << "] ";
                }
                else
                {
                    m_Out << "  ";
                }
                m_Out << describe(e, v) << "\n";
            }
            if (command != nullptr)
            {
                m_Out << "  clock " << seconds_text(v.ticks_left) << " | score " << v.points << " | coins " << v.wallet
                      << " | run " << v.run << " | step x" << v.multiplier << " | level " << v.level << " question "
                      << v.question_number << "/" << v.questions_per_level << " | " << state_name(v.state) << "\n";
            }
        }
        m_Last = v;
    }

private:
    [[nodiscard]] std::string describe(const Event& e, const SessionView& v) const
    {
        switch (e.kind)
        {
        case EventKind::LEVEL_STARTED:
            return "LEVEL " + std::to_string(e.level) +
                   (e.level > Tuning::defaults().campaign_levels ? " (endless)" : "") + " begins. The fuse is " +
                   seconds_text(e.ticks_left) + ".";
        case EventKind::QUESTION_SHOWN:
            return "QUESTION " + std::to_string(e.question_number) + "/" + std::to_string(v.questions_per_level) +
                   ":  " + sentence_of(v.question) + "\n      " + tiles_of(v);
        case EventKind::ANSWER_CORRECT:
            return "RIGHT. +" + plural(e.points, "point") + " at step x" + std::to_string(e.multiplier) +
                   (e.multiplier_rose ? " (the step rose!)" : "") + ", +" + plural(e.coins, "coin") +
                   (e.streak_bonus_coins != 0 ? " (streak bonus)" : "") + ", " + seconds_text(e.ticks_delta) +
                   " on the clock, run " + std::to_string(e.run) + ".";
        case EventKind::ANSWER_WRONG:
            return "WRONG. Tile " + std::to_string(e.tile) + " was not it; the answer was tile " +
                   std::to_string(e.correct_tile) + ". " + seconds_text(e.ticks_delta) +
                   " on the clock, step drops to x" + std::to_string(e.multiplier) + ", run 0.";
        case EventKind::HINT_USED:
            return "HINT. " + removed_text(e.removed_tiles) + " removed for " + plural(-e.coins, "coin") + ". Wallet " +
                   std::to_string(e.wallet) + ".";
        case EventKind::ALERT_ENTERED:
            return "ALERT! " + seconds_text(e.ticks_left) + " left on the fuse.";
        case EventKind::LEVEL_CLEARED:
            return "LEVEL " + std::to_string(e.level) + " CLEARED with " + plural(e.correct_answers, "right answer") +
                   " and " + std::to_string(e.seconds_left) + "s left. Time bonus " +
                   std::to_string(e.time_bonus_points) + ", perfect bonus " + std::to_string(e.perfect_bonus_points) +
                   (e.perfect ? " (perfect!)" : "") + ", +" + plural(e.coins, "coin") + ". Score " +
                   std::to_string(e.total_points) + ".";
        case EventKind::GAME_WON:
            return "YOU WIN! Win bonus " + plural(e.coins, "coin") + ". Score " + std::to_string(e.total_points) + ".";
        case EventKind::GAME_OVER:
            return "GAME OVER (" + reason_name(e.reason) + "). Score " + std::to_string(e.total_points) + ", wallet " +
                   std::to_string(e.wallet) + ".";
        }
        return "?";
    }

    std::ostream& m_Out;
    bool m_Quiet;
    SessionView m_Last; // the view after the previous call, so an answer can name the tile's number
};

// ---------------------------------------------------------------------------------------------------------------
// One run

/// A runner with a printer and a record of every command applied, so any run can be written out as a replay.
class Play
{
public:
    Play(const Replay& start, std::ostream& out, bool quiet)
        : m_Runner(*replay_tuning(start.tuning), start.seed, start.wallet, start.won_once), m_Printer(out, quiet),
          m_Recording(start)
    {
        m_Recording.commands.clear();
        m_Recording.expect.reset();
    }

    void tick()
    {
        m_Printer(nullptr, m_Runner.tick(), m_Runner);
    }

    void tick_to(int tick_number)
    {
        while (m_Runner.tick_count() < tick_number)
        {
            tick();
        }
    }

    void command(ReplayAction action, int tile = -1)
    {
        const ReplayCommand c{m_Runner.tick_count(), action, tile};
        m_Recording.commands.push_back(c);
        m_Printer(&c, m_Runner.apply(c), m_Runner);
    }

    [[nodiscard]] const SessionView& view() const
    {
        return m_Runner.session().view();
    }

    [[nodiscard]] Replay recording() const
    {
        Replay replay = m_Recording;
        const int last = replay.commands.empty() ? 0 : replay.commands.back().tick;
        replay.end_tick = m_Runner.tick_count() > last ? m_Runner.tick_count() : 0;
        replay.expect = m_Runner.result().as_expect();
        return replay;
    }

    [[nodiscard]] ReplayResult result() const
    {
        return m_Runner.result();
    }

private:
    ReplayRunner m_Runner;
    Printer m_Printer;
    Replay m_Recording;
};

enum class Step
{
    RIGHT,
    WRONG,
    HINT,
    PAUSE_RESUME,
    WAIT, // let the clock run to zero
};

struct Plan
{
    int delay; // ticks to wait before the step
    Step step;
};

/// What a policy does about the question on screen.
using Policy = std::function<std::vector<Plan>(const SessionView&)>;

Policy make_policy(const Options& o)
{
    const int think = o.think;
    const std::string& name = *o.policy;
    if (name == "perfect")
    {
        return [think](const SessionView&) { return std::vector<Plan>{{think, Step::RIGHT}}; };
    }
    if (name == "always-wrong")
    {
        return [think](const SessionView&) { return std::vector<Plan>{{think, Step::WRONG}}; };
    }
    if (name == "hint-then-right")
    {
        return [think](const SessionView&)
        { return std::vector<Plan>{{think / 2, Step::HINT}, {think - think / 2, Step::RIGHT}}; };
    }
    if (name == "stall")
    {
        return [think, left = o.stall_after](const SessionView&) mutable
        {
            if (left > 0)
            {
                --left;
                return std::vector<Plan>{{think, Step::RIGHT}};
            }
            return std::vector<Plan>{{0, Step::WAIT}};
        };
    }
    // random: a child who is mostly right, sometimes wrong, sometimes asks for a hint, sometimes is called away.
    return [think, rng = Rng(o.policy_seed)](const SessionView&) mutable
    {
        const int roll = rng.between(0, 99);
        const int delay = rng.between(think / 3, think * 3);
        if (roll < 55)
        {
            return std::vector<Plan>{{delay, Step::RIGHT}};
        }
        if (roll < 70)
        {
            return std::vector<Plan>{{delay, Step::WRONG}};
        }
        if (roll < 85)
        {
            return std::vector<Plan>{{delay / 2, Step::HINT}, {delay / 2, Step::RIGHT}};
        }
        if (roll < 95)
        {
            return std::vector<Plan>{{delay / 2, Step::PAUSE_RESUME}, {delay / 2, Step::RIGHT}};
        }
        return std::vector<Plan>{{think * 12, Step::RIGHT}}; // a long think, into the alert zone on a short fuse
    };
}

int first_wrong_tile(const SessionView& v)
{
    for (int i = 0; i < static_cast<int>(v.question.tiles.size()); ++i)
    {
        if (i != v.question.correct_tile && ((v.removed_tiles >> i) & 1) == 0)
        {
            return i;
        }
    }
    return -1;
}

void play_question(Play& play, const std::vector<Plan>& plans)
{
    for (const Plan& plan : plans)
    {
        for (int i = 0; i < plan.delay && play.view().state == SessionState::PLAYING; ++i)
        {
            play.tick();
        }
        if (plan.step == Step::WAIT)
        {
            // The clock is the only thing that can end this, so a bound at the fuse's length is enough.
            for (int i = 0; i <= play.view().ticks_total && play.view().state == SessionState::PLAYING; ++i)
            {
                play.tick();
            }
            return;
        }
        if (play.view().state != SessionState::PLAYING)
        {
            return;
        }
        const SessionView& v = play.view();
        switch (plan.step)
        {
        case Step::RIGHT:
            play.command(ReplayAction::ANSWER, v.question.correct_tile);
            break;
        case Step::WRONG:
            // A question always keeps at least two tiles, so a wrong one exists; the fallback is for a hint-less
            // board that somehow has none.
            play.command(ReplayAction::ANSWER,
                         first_wrong_tile(v) >= 0 ? first_wrong_tile(v) : v.question.correct_tile);
            break;
        case Step::HINT:
            if (v.hint_available)
            {
                play.command(ReplayAction::USE_HINT);
            }
            break;
        case Step::PAUSE_RESUME:
            play.command(ReplayAction::PAUSE);
            play.command(ReplayAction::RESUME);
            break;
        case Step::WAIT:
            break;
        }
    }
}

struct Played
{
    Replay replay; // the commands that were played, with fresh expectations
    ReplayResult result;
};

std::expected<Played, std::string> play_auto(const Options& o, std::ostream& out, bool quiet)
{
    Replay start;
    start.seed = o.seed.value_or(DEFAULT_SEED);
    start.wallet = o.wallet;
    start.won_once = o.won_once;
    start.note = o.note;
    start.recipe = recipe_of(o);

    Play play(start, out, quiet);
    Policy policy = make_policy(o);
    const int campaign_levels = Tuning::defaults().campaign_levels;

    for (int step = 0; step < MAX_AUTO_STEPS; ++step)
    {
        const SessionView& v = play.view();
        switch (v.state)
        {
        case SessionState::IDLE:
            play.command(ReplayAction::START_GAME);
            break;
        case SessionState::GAME_OVER:
            return Played{play.recording(), play.result()};
        case SessionState::PAUSED:
            play.command(ReplayAction::RESUME);
            break;
        case SessionState::LEVEL_CLEAR:
            if (o.clear_level != 0 && v.level >= o.clear_level)
            {
                return Played{play.recording(), play.result()};
            }
            play.command(ReplayAction::NEXT_LEVEL);
            break;
        case SessionState::WON:
            if (o.clear_level != 0 && v.level >= o.clear_level)
            {
                return Played{play.recording(), play.result()};
            }
            play.command(o.endless_to > campaign_levels ? ReplayAction::KEEP_GOING : ReplayAction::FINISH);
            break;
        case SessionState::PLAYING:
            if (o.endless_to != 0 && v.level >= o.endless_to)
            {
                play.command(ReplayAction::PAUSE);
                play.command(ReplayAction::QUIT);
                break;
            }
            play_question(play, policy(v));
            break;
        }
    }
    return fail("the policy did not finish within " + std::to_string(MAX_AUTO_STEPS) + " steps");
}

std::expected<Played, std::string> play_script(const Replay& script, const Options& o, std::ostream& out, bool quiet)
{
    Replay start = script;
    if (o.seed)
    {
        start.seed = *o.seed;
    }
    if (replay_tuning(start.tuning) == nullptr)
    {
        return fail("tuning \"" + start.tuning + "\" is not one this build has");
    }
    Play play(start, out, quiet);
    for (const ReplayCommand& c : start.commands)
    {
        play.tick_to(c.tick);
        play.command(c.action, c.tile);
    }
    play.tick_to(start.end_tick);
    Played played{play.recording(), play.result()};
    if (!o.note.empty())
    {
        played.replay.note = o.note;
    }
    return played;
}

// ---------------------------------------------------------------------------------------------------------------
// Files

std::expected<std::string, std::string> read_all(const std::string& path)
{
    std::ostringstream text;
    if (path == "-")
    {
        text << std::cin.rdbuf();
        return text.str();
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return fail("cannot read " + path);
    }
    text << in.rdbuf();
    return text.str();
}

bool write_all(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return static_cast<bool>(out);
}

std::string without_carriage_returns(std::string text)
{
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

/// Makes the file text a replay's recipe would produce now. The note stays as the file has it.
std::expected<std::string, std::string> remake(const std::string& path)
{
    const auto text = read_all(path);
    if (!text)
    {
        return fail(text.error());
    }
    const auto replay = parse_replay(*text);
    if (!replay)
    {
        return fail(path + ": " + replay.error());
    }
    if (replay->recipe.empty())
    {
        return fail(path + ": has no recipe, so it cannot be remade");
    }
    const auto options = parse_args(split_words(replay->recipe));
    if (!options || !options->policy)
    {
        return fail(path + ": the recipe is not an --auto run" + (options ? "" : ": " + options.error()));
    }
    Options o = *options;
    o.note = replay->note;
    std::ostringstream discard;
    auto played = play_auto(o, discard, true);
    if (!played)
    {
        return fail(path + ": " + played.error());
    }
    return write_replay(played->replay);
}

int run_directory(const std::string& dir, bool rewrite)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(dir, error))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            files.push_back(entry.path());
        }
    }
    if (error || files.empty())
    {
        std::cerr << "no replay files found in " << dir << "\n";
        return 1;
    }
    std::sort(files.begin(), files.end());

    int bad = 0;
    for (const auto& file : files)
    {
        const std::string path = file.string();
        const auto made = remake(path);
        if (!made)
        {
            std::cerr << made.error() << "\n";
            ++bad;
            continue;
        }
        const auto current = read_all(path);
        const bool same = current && without_carriage_returns(*current) == *made;
        if (rewrite)
        {
            if (!same && !write_all(path, *made))
            {
                std::cerr << "cannot write " << path << "\n";
                ++bad;
                continue;
            }
            std::cout << (same ? "unchanged  " : "rewritten  ") << file.filename().string() << "\n";
        }
        else
        {
            std::cout << (same ? "current    " : "STALE      ") << file.filename().string() << "\n";
            bad += same ? 0 : 1;
        }
    }
    if (bad != 0 && !rewrite)
    {
        std::cerr << bad << " replay file(s) differ from what their recipes make now. If the rules or tuning "
                  << "changed on purpose, run: heart_boom_maths_harness --regenerate " << dir << "\n";
    }
    return bad == 0 ? 0 : 1;
}

int run(const std::vector<std::string>& args)
{
    if (args.empty() || args[0] == "--help" || args[0] == "-h")
    {
        std::cout << USAGE;
        return args.empty() ? 2 : 0;
    }
    const auto parsed = parse_args(args);
    if (!parsed)
    {
        std::cerr << "error: " << parsed.error() << "\n\n" << USAGE;
        return 2;
    }
    const Options& o = *parsed;

    if (o.regenerate_dir)
    {
        return run_directory(*o.regenerate_dir, true);
    }
    if (o.check_dir)
    {
        return run_directory(*o.check_dir, false);
    }

    std::expected<Played, std::string> played = fail("nothing to play");
    std::optional<ReplayExpect> expected;
    if (o.script)
    {
        const auto text = read_all(*o.script);
        if (!text)
        {
            std::cerr << "error: " << text.error() << "\n";
            return 1;
        }
        const auto script = parse_replay(*text);
        if (!script)
        {
            std::cerr << "error: " << *o.script << ": " << script.error() << "\n";
            return 1;
        }
        std::cout << "seed " << (o.seed ? *o.seed : script->seed) << ", tuning " << script->tuning << ", wallet "
                  << script->wallet << ", " << script->commands.size() << " commands\n";
        expected = o.seed ? std::nullopt : script->expect;
        played = play_script(*script, o, std::cout, o.quiet);
    }
    else
    {
        std::cout << "seed " << o.seed.value_or(DEFAULT_SEED) << ", policy " << *o.policy << ", wallet " << o.wallet
                  << "\n";
        played = play_auto(o, std::cout, o.quiet);
    }
    if (!played)
    {
        std::cerr << "error: " << played.error() << "\n";
        return 1;
    }

    if (o.record)
    {
        if (!write_all(*o.record, write_replay(played->replay)))
        {
            std::cerr << "error: cannot write " << *o.record << "\n";
            return 1;
        }
        std::cout << "recorded " << played->replay.commands.size() << " commands to " << *o.record << "\n";
    }

    int status = 0;
    if (expected)
    {
        if (*expected == played->result.as_expect())
        {
            std::cout << "EXPECT ok: the run matches the hashes and score in the script\n";
        }
        else
        {
            std::cout << "EXPECT MISMATCH: the script says events_hash=" << hash_to_hex(expected->events_hash)
                      << " final_hash=" << hash_to_hex(expected->final_hash) << " score=" << expected->score
                      << "; if the change was deliberate, rewrite it with --record\n";
            status = 1;
        }
    }
    const ReplayResult& r = played->result;
    std::cout << "FINAL score=" << r.score << " level=" << r.level << " events_hash=" << hash_to_hex(r.events_hash)
              << " final_hash=" << hash_to_hex(r.final_hash) << "\n";
    std::cout.flush();
    return status;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }
    return run(args);
}
