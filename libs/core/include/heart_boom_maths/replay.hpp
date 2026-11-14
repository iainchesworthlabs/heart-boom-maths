// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/session.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Golden replays: a seed, a tuning name and the commands of a whole run, each with the tick it arrives on. The same
/// code serves the Tier 1 test, the text harness and the WebAssembly run under Node, so one file proves the same
/// thing on every platform. See "Golden replays" in docs/game-architecture.md.
namespace heart_boom_maths
{

inline constexpr int REPLAY_FORMAT = 1;

/// The commands a replay can carry. tick() is not one of them: the runner calls it between commands.
enum class ReplayAction
{
    START_GAME,
    ANSWER,
    USE_HINT,
    PAUSE,
    RESUME,
    QUIT,
    NEXT_LEVEL,
    KEEP_GOING,
    FINISH,
};

/// The name a replay file uses for an action ("start_game", "answer", ...).
[[nodiscard]] std::string_view replay_action_name(ReplayAction action) noexcept;
[[nodiscard]] std::optional<ReplayAction> replay_action_from_name(std::string_view name) noexcept;

struct ReplayCommand
{
    int tick = 0; // tick() calls made before this command; never decreases from one command to the next
    ReplayAction action = ReplayAction::START_GAME;
    int tile = -1; // ANSWER only

    friend bool operator==(const ReplayCommand&, const ReplayCommand&) = default;
};

/// What a run is expected to produce. Hashes are written as 16 lowercase hex digits.
struct ReplayExpect
{
    std::uint64_t events_hash = 0;
    std::uint64_t final_hash = 0;
    std::int64_t score = 0;

    friend bool operator==(const ReplayExpect&, const ReplayExpect&) = default;
};

struct Replay
{
    int format = REPLAY_FORMAT;
    std::uint64_t seed = 0; // at most INT64_MAX, because the JSON reader has no unsigned numbers
    std::string tuning = "defaults";
    std::int64_t wallet = 0; // coins in the wallet before the game, as if loaded from a save
    bool won_once = false;
    std::string note;   // free text for readers; not part of the run
    std::string recipe; // the harness arguments that make this file, so a regeneration needs no memory
    std::vector<ReplayCommand> commands;
    int end_tick =
        0; // when above the last command's tick, the runner calls tick() on up to here, so a run can end in the bomb
    std::optional<ReplayExpect> expect; // empty in a script that has not been recorded yet

    friend bool operator==(const Replay&, const Replay&) = default;
};

/// What a run produced.
struct ReplayResult
{
    std::uint64_t events_hash = 0; // FNV-1a over every event's append_to() output, in order
    std::uint64_t final_hash = 0;  // GameSession::state_hash() at the end
    std::int64_t score = 0;        // total points at the end
    int level = 0;
    SessionState state = SessionState::IDLE;
    std::size_t event_count = 0;

    [[nodiscard]] ReplayExpect as_expect() const noexcept
    {
        return {events_hash, final_hash, score};
    }
};

/// The tuning a replay names ("defaults"), or null for a name this build does not know.
[[nodiscard]] const Tuning* replay_tuning(std::string_view name) noexcept;

/// Reads a replay file. Every problem is reported with the field it was found in, and a file that parses is always one
/// the runner can play: ticks never decrease, actions are known and the tuning is named.
[[nodiscard]] std::expected<Replay, std::string> parse_replay(std::string_view text);

/// A replay file with one command per line, so a changed replay reviews as a readable diff. Ends with a newline.
[[nodiscard]] std::string write_replay(const Replay& replay);

/// 16 lowercase hex digits.
[[nodiscard]] std::string hash_to_hex(std::uint64_t hash);

/// Plays commands into a session and keeps the hash of every event it sent. It is the one place that turns "tick N"
/// into calls to GameSession::tick(), so the test, the harness and the Node run all count ticks the same way.
class ReplayRunner
{
public:
    ReplayRunner(const Tuning& tuning, std::uint64_t seed, std::int64_t wallet = 0, bool won_once = false);

    /// One call to GameSession::tick(). Returns the events it sent.
    std::vector<Event> tick();

    /// One command, with no ticks before it. Returns the events it sent.
    std::vector<Event> apply(const ReplayCommand& command);

    [[nodiscard]] int tick_count() const noexcept
    {
        return m_Tick;
    }

    [[nodiscard]] const GameSession& session() const noexcept
    {
        return m_Session;
    }

    [[nodiscard]] ReplayResult result() const;

private:
    std::vector<Event> collect();

    GameSession m_Session;
    int m_Tick = 0;
    std::size_t m_EventCount = 0;
    std::vector<std::int64_t> m_Hashed;
};

/// Called after every tick (`command` is null) and every command, with the events it sent. For printing and tests.
using ReplayObserver =
    std::function<void(const ReplayCommand* command, const std::vector<Event>& events, const ReplayRunner& runner)>;

/// Plays a whole replay. Fails only when the tuning name is not one this build has.
[[nodiscard]] std::expected<ReplayResult, std::string> run_replay(const Replay& replay,
                                                                  const ReplayObserver& observer = {});

} // namespace heart_boom_maths
