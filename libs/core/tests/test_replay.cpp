// SPDX-License-Identifier: GPL-3.0-or-later
#include "golden_replays.hpp"
#include "heart_boom_maths/replay.hpp"
#include "heart_boom_maths/state_hash.hpp"
#include "session_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace heart_boom_maths;

namespace
{

const GoldenReplay& golden(std::string_view name)
{
    for (const GoldenReplay& g : GOLDEN_REPLAYS)
    {
        if (g.name == name)
        {
            return g;
        }
    }
    FAIL("no golden replay named " << name);
    return GOLDEN_REPLAYS[0];
}

Replay parsed(const GoldenReplay& g)
{
    const auto replay = parse_replay(g.text);
    INFO(g.name << ": " << (replay ? "" : replay.error()));
    REQUIRE(replay.has_value());
    return *replay;
}

/// Everything a replay sent, in order.
struct Played
{
    ReplayResult result;
    std::vector<Event> events;
};

Played play(const Replay& replay)
{
    Played played;
    const auto result =
        run_replay(replay, [&](const ReplayCommand*, const std::vector<Event>& events, const ReplayRunner&)
                   { played.events.insert(played.events.end(), events.begin(), events.end()); });
    REQUIRE(result.has_value());
    played.result = *result;
    return played;
}

std::size_t count(const Played& played, EventKind kind)
{
    return hbm_test::count_of(played.events, kind);
}

constexpr std::string_view REGENERATE =
    "If the rules or tuning changed on purpose, regenerate the replays in the same commit: "
    "heart_boom_maths_harness --regenerate libs/core/tests/data/replays (see docs/testing.md). "
    "If nothing was meant to change, this is a bug.";

} // namespace

TEST_CASE("Every golden replay plays to its stored hashes and score", "[replay][unit]")
{
    REQUIRE(std::size(GOLDEN_REPLAYS) >= 5);
    for (const GoldenReplay& g : GOLDEN_REPLAYS)
    {
        const Replay replay = parsed(g);
        REQUIRE(replay.expect.has_value());
        const Played played = play(replay);

        INFO("Replay \"" << g.name << "\" no longer plays as recorded. " << REGENERATE);
        INFO("events_hash: stored " << hash_to_hex(replay.expect->events_hash) << ", now "
                                    << hash_to_hex(played.result.events_hash));
        INFO("final_hash: stored " << hash_to_hex(replay.expect->final_hash) << ", now "
                                   << hash_to_hex(played.result.final_hash));
        INFO("score: stored " << replay.expect->score << ", now " << played.result.score);
        CHECK(played.result.events_hash == replay.expect->events_hash);
        CHECK(played.result.final_hash == replay.expect->final_hash);
        CHECK(played.result.score == replay.expect->score);
    }
}

TEST_CASE("Golden replay files are in the canonical form the harness writes", "[replay][unit]")
{
    for (const GoldenReplay& g : GOLDEN_REPLAYS)
    {
        std::string text(g.text);
        std::erase(text, '\r'); // a checkout may have turned the line endings into CRLF
        INFO(g.name);
        CHECK(write_replay(parsed(g)) == text);
    }
}

TEST_CASE("The golden replays cover what they say they cover", "[replay][unit]")
{
    const Tuning& t = Tuning::defaults();

    SECTION("a perfect level")
    {
        const Played p = play(parsed(golden("perfect-level")));
        CHECK(count(p, EventKind::ANSWER_CORRECT) == 10);
        CHECK(count(p, EventKind::ANSWER_WRONG) == 0);
        REQUIRE(count(p, EventKind::LEVEL_CLEARED) == 1);
        const Event cleared = hbm_test::events_of(p.events, EventKind::LEVEL_CLEARED).at(0);
        CHECK(cleared.perfect);
        CHECK(cleared.perfect_bonus_points == t.perfect_level_bonus_points);
        CHECK(p.result.state == SessionState::LEVEL_CLEAR);
    }
    SECTION("wrong answers and hints")
    {
        const Played p = play(parsed(golden("wrong-answers-and-hints")));
        CHECK(count(p, EventKind::ANSWER_WRONG) > 0);
        CHECK(count(p, EventKind::HINT_USED) > 0);
        CHECK(count(p, EventKind::ANSWER_CORRECT) > 0);
        CHECK(count(p, EventKind::LEVEL_CLEARED) >= 1);
        CHECK(p.result.state != SessionState::GAME_OVER);
    }
    SECTION("a time-out in the alert zone")
    {
        const Played p = play(parsed(golden("alert-timeout")));
        REQUIRE(count(p, EventKind::ALERT_ENTERED) >= 1);
        REQUIRE(count(p, EventKind::GAME_OVER) == 1);
        CHECK(p.events.back().kind == EventKind::GAME_OVER);
        CHECK(p.events.back().reason == GameOverReason::BOMB);
        CHECK(p.result.state == SessionState::GAME_OVER);
    }
    SECTION("the twelve-level win")
    {
        const Played p = play(parsed(golden("win-12-levels")));
        CHECK(count(p, EventKind::LEVEL_CLEARED) == 12);
        REQUIRE(count(p, EventKind::GAME_WON) == 1);
        CHECK(hbm_test::events_of(p.events, EventKind::GAME_WON).at(0).coins == t.win_bonus_coins);
        CHECK(p.result.level == t.campaign_levels);
        CHECK(p.result.state == SessionState::GAME_OVER);
    }
    SECTION("endless mode to level 20")
    {
        const Played p = play(parsed(golden("endless-to-level-20")));
        CHECK(count(p, EventKind::GAME_WON) == 1);
        CHECK(p.result.level == 20);
        CHECK(count(p, EventKind::LEVEL_STARTED) == 20);
        CHECK(p.result.state == SessionState::GAME_OVER);
    }
}

TEST_CASE("A replay is deterministic and answers to its seed and its commands", "[replay][unit]")
{
    const Replay replay = parsed(golden("wrong-answers-and-hints"));
    const ReplayResult first = play(replay).result;
    CHECK(play(replay).result.events_hash == first.events_hash);

    Replay other_seed = replay;
    other_seed.seed += 1;
    CHECK(play(other_seed).result.events_hash != first.events_hash);

    Replay fewer = replay;
    fewer.commands.pop_back();
    CHECK(play(fewer).result.final_hash != first.final_hash);

    Replay richer = replay;
    richer.wallet += 1;
    CHECK(play(richer).result.final_hash != first.final_hash);
}

TEST_CASE("The runner counts ticks and hashes events the way the replay says", "[replay][unit]")
{
    ReplayRunner runner(Tuning::defaults(), 9);
    CHECK(runner.tick_count() == 0);
    CHECK(runner.tick().empty()); // nothing is playing yet, so the tick is ignored but still counted
    CHECK(runner.tick_count() == 1);

    const std::vector<Event> started = runner.apply({1, ReplayAction::START_GAME, -1});
    REQUIRE(started.size() == 2);
    CHECK(started[0].kind == EventKind::LEVEL_STARTED);
    CHECK(started[1].kind == EventKind::QUESTION_SHOWN);
    const int full = runner.session().view().ticks_left;
    for (int i = 0; i < 7; ++i)
    {
        (void)runner.tick();
    }
    CHECK(runner.tick_count() == 8);
    CHECK(runner.session().view().ticks_left == full - 7);

    // The events hash is FNV-1a over the events' integers in order, and the final hash is the session's.
    std::vector<std::int64_t> expected;
    for (const Event& e : started)
    {
        e.append_to(expected);
    }
    const ReplayResult result = runner.result();
    CHECK(result.events_hash == fnv1a_64(expected));
    CHECK(result.final_hash == runner.session().state_hash());
    CHECK(result.event_count == 2);
    CHECK(result.level == 1);
    CHECK(result.state == SessionState::PLAYING);
    CHECK(result.as_expect() == ReplayExpect{result.events_hash, result.final_hash, 0});
}

TEST_CASE("run_replay calls the observer for every tick and command and rejects an unknown tuning", "[replay][unit]")
{
    Replay replay;
    replay.seed = 4;
    replay.commands = {{0, ReplayAction::START_GAME, -1}, {3, ReplayAction::PAUSE, -1}, {3, ReplayAction::RESUME, -1}};
    int ticks = 0;
    int commands = 0;
    const auto result = run_replay(replay, [&](const ReplayCommand* c, const std::vector<Event>&, const ReplayRunner&)
                                   { (c == nullptr ? ticks : commands) += 1; });
    REQUIRE(result.has_value());
    CHECK(ticks == 3);
    CHECK(commands == 3);

    CHECK(run_replay(replay).has_value()); // no observer is fine

    replay.tuning = "nonsense";
    const auto failed = run_replay(replay);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().find("nonsense") != std::string::npos);
    CHECK(replay_tuning("defaults") == &Tuning::defaults());
    CHECK(replay_tuning("nonsense") == nullptr);
}

TEST_CASE("Every replay action has a name and the names read back", "[replay][unit]")
{
    for (const ReplayAction a : {ReplayAction::START_GAME, ReplayAction::ANSWER, ReplayAction::USE_HINT,
                                 ReplayAction::PAUSE, ReplayAction::RESUME, ReplayAction::QUIT,
                                 ReplayAction::NEXT_LEVEL, ReplayAction::KEEP_GOING, ReplayAction::FINISH})
    {
        const std::string_view name = replay_action_name(a);
        CHECK(name != "unknown");
        CHECK(replay_action_from_name(name) == a);
    }
    CHECK_FALSE(replay_action_from_name("tick").has_value());
    CHECK(replay_action_name(static_cast<ReplayAction>(99)) == "unknown");
    CHECK(hash_to_hex(0) == "0000000000000000");
    CHECK(hash_to_hex(0xCBF29CE484222325ULL) == "cbf29ce484222325");
}

TEST_CASE("A replay file survives a write and a read", "[replay][unit]")
{
    Replay replay;
    replay.seed = 77;
    replay.wallet = 12;
    replay.won_once = true;
    replay.note = "A note with \"quotes\" and a \\ backslash.";
    replay.recipe = "--seed 77 --auto perfect";
    replay.commands = {{0, ReplayAction::START_GAME, -1}, {40, ReplayAction::ANSWER, 2}, {91, ReplayAction::PAUSE, -1}};
    replay.end_tick = 120;
    replay.expect = ReplayExpect{0x0123456789abcdefULL, 0xfedcba9876543210ULL, 30};

    const std::string text = write_replay(replay);
    const auto again = parse_replay(text);
    REQUIRE(again.has_value());
    CHECK(*again == replay);
    CHECK(write_replay(*again) == text);
    CHECK(text.back() == '\n');

    Replay empty;
    const auto empty_again = parse_replay(write_replay(empty));
    REQUIRE(empty_again.has_value());
    CHECK(*empty_again == empty);
}

TEST_CASE("A bad replay file is refused with the field that is wrong", "[replay][unit]")
{
    const std::string good_head = R"("format": 1, "seed": 5, "tuning": "defaults")";
    const auto refused = [&](const std::string& text, const std::string& field)
    {
        const auto replay = parse_replay(text);
        INFO(text);
        REQUIRE_FALSE(replay.has_value());
        INFO(replay.error());
        CHECK(replay.error().find(field) != std::string::npos);
    };
    const auto with = [&](const std::string& rest) { return "{" + good_head + ", " + rest + "}"; };

    refused("", "not valid");
    refused("[]", "object");
    refused(R"({"seed": 5, "tuning": "defaults", "commands": []})", "format");
    refused(R"({"format": 2, "seed": 5, "tuning": "defaults", "commands": []})", "format");
    refused(R"({"format": 1, "seed": -1, "tuning": "defaults", "commands": []})", "seed");
    refused(R"({"format": 1, "tuning": "defaults", "commands": []})", "seed");
    refused(R"({"format": 1, "seed": 5, "tuning": "other", "commands": []})", "tuning");
    refused(R"({"format": 1, "seed": 5, "commands": []})", "tuning");
    refused(with(R"("wallet": -1, "commands": [])"), "wallet");
    refused(with(R"("wallet": "x", "commands": [])"), "wallet");
    refused(with(R"("won_once": 1, "commands": [])"), "won_once");
    refused(with(R"("note": 3, "commands": [])"), "note");
    refused(with(R"("recipe": true, "commands": [])"), "recipe");
    refused(with(R"("commands": {})"), "commands");
    refused("{" + good_head + "}", "commands");
    refused(with(R"("commands": [3])"), "commands[0]");
    refused(with(R"("commands": [{"do": "start_game"}])"), "commands[0].tick");
    refused(with(R"("commands": [{"tick": -1, "do": "start_game"}])"), "commands[0].tick");
    refused(with(R"("commands": [{"tick": 2000000000, "do": "start_game"}])"), "commands[0].tick");
    refused(with(R"("commands": [{"tick": 0, "do": "dance"}])"), "commands[0].do");
    refused(with(R"("commands": [{"tick": 0}])"), "commands[0].do");
    refused(with(R"("commands": [{"tick": 0, "do": "answer"}])"), "commands[0].tile");
    refused(with(R"("commands": [{"tick": 0, "do": "answer", "tile": 99999999999}])"), "commands[0].tile");
    refused(with(R"("commands": [{"tick": 5, "do": "pause"}, {"tick": 4, "do": "resume"}])"), "commands[1].tick");
    refused(with(R"("commands": [{"tick": 9, "do": "pause"}], "end_tick": 8)"), "end_tick");
    refused(with(R"("commands": [], "end_tick": "x")"), "end_tick");
    refused(with(R"("commands": [], "end_tick": 2000000000)"), "end_tick");
    refused(with(R"("commands": [], "expect": 1)"), "expect");
    refused(with(R"("commands": [], "expect": {"events_hash": "00", "final_hash": "0000000000000000", "score": 0})"),
            "events_hash");
    refused(with(R"("commands": [], "expect": {"events_hash": "0000000000000000", "final_hash": "XYZ", "score": 0})"),
            "final_hash");
    refused(with(R"("commands": [], "expect": {"events_hash": "0000000000000000", "final_hash": "0000000000000000"})"),
            "score");
    // Upper-case hex is not what the harness writes, so it is not accepted either.
    refused(
        with(
            R"("commands": [], "expect": {"events_hash": "000000000000000A", "final_hash": "0000000000000000", "score": 0})"),
        "events_hash");

    // A tile outside the board is a legal command: the session ignores it.
    const auto odd = parse_replay(with(R"("commands": [{"tick": 0, "do": "answer", "tile": -3}])"));
    REQUIRE(odd.has_value());
    CHECK(odd->commands.front().tile == -3);
}
