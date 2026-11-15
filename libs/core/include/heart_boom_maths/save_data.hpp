// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/tuning.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace heart_boom_maths
{

/// The schema number this build writes.
inline constexpr int SAVE_VERSION = 2;

/// Format bounds, not game numbers. They keep a damaged or hostile file from putting a value in the game that no
/// version of it could have written: the wallet is a 32-bit count, and a saved run is a few dozen integers. A counter
/// (a name change count, an earned or spent total, a top 10 sequence number) is bounded like the wallet. The two list
/// bounds are far above `max_players` and the number of devices a family has, so a real save never meets them; a list
/// that is full refuses to grow, and the file stays valid.
inline constexpr std::int64_t MAX_SAVED_COINS = 2147483647;
inline constexpr std::int64_t MAX_SAVED_COUNTER = MAX_SAVED_COINS;
inline constexpr std::size_t MAX_SAVED_RUN_VALUES = 4096;
inline constexpr std::size_t MAX_SAVED_PLAYERS = 64;      // in the list, removed players included
inline constexpr std::size_t MAX_SAVED_COIN_ENTRIES = 64; // per player: one per device

/// The id of the player every save starts with, so the first players of two devices are the same player in a merge.
inline constexpr std::string_view FIRST_PLAYER_ID = "first";

/// A device id and every later player id are this many lowercase hexadecimal digits.
inline constexpr std::size_t ID_HEX_DIGITS = 16;

/// What one device has earned and spent for one player. Both only ever grow.
struct CoinEntry
{
    std::string device{}; // a device id, or empty before the app has assigned one
    std::int64_t earned = 0;
    std::int64_t spent = 0;

    friend bool operator==(const CoinEntry&, const CoinEntry&) = default;
};

struct Player
{
    std::string id{};          // `first`, or 16 lowercase hex digits
    std::string name{};        // empty until first typed, otherwise a valid name
    std::int64_t name_rev = 0; // counts name changes; the higher wins a merge
    bool removed = false;      // kept as a mark so a merge cannot bring the player back
    bool won_once = false;
    std::int64_t best = 0; // the player's best final score
    std::vector<CoinEntry> coins{};

    friend bool operator==(const Player&, const Player&) = default;
};

/// One place in the top 10. `device` and `seq` are the entry's identity, so a merge can drop a duplicate.
struct ScoreEntry
{
    std::string name{};
    std::int64_t score = 0;
    std::string device{}; // the device that made the entry; empty before the app has assigned an id
    std::int64_t seq = 0;

    friend bool operator==(const ScoreEntry&, const ScoreEntry&) = default;
};

/// What stays on this device.
struct DeviceData
{
    std::string id{}; // 16 lowercase hex digits, or empty until the app assigns one
    bool sound = true;
    bool music = true;
    bool game_center = false;
    bool haptics = true;
    bool cloud_sync = false;
    std::string current_player{FIRST_PLAYER_ID};
    std::int64_t next_seq = 1; // the number the next top 10 entry made on this device takes

    /// The run is a list of integers made by `GameSession::snapshot()`; the save logic does not look inside it.
    /// `run_player` is the player who paused it: set when `run` is, empty when it is not.
    std::optional<std::vector<std::int64_t>> run{};
    std::string run_player{};

    friend bool operator==(const DeviceData&, const DeviceData&) = default;
};

/// What a sync sends and a merge joins.
struct SharedData
{
    std::vector<Player> players{Player{.id = std::string(FIRST_PLAYER_ID)}};
    std::vector<ScoreEntry> top10{}; // best first; of equal scores the older entry comes first

    friend bool operator==(const SharedData&, const SharedData&) = default;
};

/// Everything the game keeps between sessions. The default is a first run: one unnamed player, `first`, who is the
/// current player.
struct SaveData
{
    int version = SAVE_VERSION;
    DeviceData device{};
    SharedData shared{};

    friend bool operator==(const SaveData&, const SaveData&) = default;
};

/// Why a save was refused, whether on reading or before writing.
enum class SaveError
{
    NOT_JSON,      // the text is not JSON this reader accepts (includes floats, null, and any limit)
    NOT_AN_OBJECT, // the document is not an object
    MISSING_FIELD,
    WRONG_TYPE,
    BAD_VERSION, // below 1, or not the version this schema expects
    MIGRATION_FAILED,
    COINS_OUT_OF_RANGE, // an earned or spent total below 0 or above MAX_SAVED_COINS
    TOP10_TOO_LONG,
    NAME_INVALID,  // a top 10 name: empty, too long, or not A to Z and space
    SCORE_INVALID, // a top 10 score below 1
    TOP10_NOT_SORTED,
    RUN_TOO_LONG,
    DEVICE_ID_INVALID,     // the device's own id, or the device of a coin entry or a top 10 entry
    PLAYER_ID_INVALID,     // not `first` or 16 lowercase hex digits (also the current player and the run's player)
    PLAYER_ID_DUPLICATE,   // two players with one id
    PLAYER_NAME_INVALID,   // neither empty nor a valid name
    PLAYERS_TOO_MANY,      // more than MAX_SAVED_PLAYERS in the list
    COIN_ENTRIES_TOO_MANY, // more than MAX_SAVED_COIN_ENTRIES for one player
    COIN_DEVICE_DUPLICATE, // two coin entries of one player for one device
    COUNTER_OUT_OF_RANGE,  // name_rev, seq, next_seq or a best score below 0, or a counter above MAX_SAVED_COUNTER
    RUN_PLAYER_MISSING,    // a run with no player, or a player with no run
};

/// Checks every rule of the format. `expected_version` is the schema the data claims to be. More active players than
/// `max_players` is valid, because a merge can produce it; the limit only stops adding one.
[[nodiscard]] std::expected<void, SaveError> validate(const SaveData& data, const Tuning& tuning = Tuning::defaults(),
                                                      int expected_version = SAVE_VERSION);

/// 16 lowercase hexadecimal digits.
[[nodiscard]] bool is_valid_device_id(std::string_view id) noexcept;

/// `first`, or 16 lowercase hexadecimal digits.
[[nodiscard]] bool is_valid_player_id(std::string_view id) noexcept;

/// An id made from 64 random bits the app drew: 16 lowercase hexadecimal digits, the high bits first.
[[nodiscard]] std::string hex_id(std::uint64_t bits);

/// The id for a player this device adds: a 4 digit creation counter, then the low 48 bits of `random_bits` as 12
/// digits. The counter is one more than the largest among the ids in `shared` (removed players included; `first` has
/// none; an older id counts as whatever its first 4 digits read), and stops at `ffff`. The list is kept in ascending id
/// order, so a player added later sorts later on this device, while two devices that add while apart still agree on one
/// order after a merge, with the random part breaking a tie of counters.
[[nodiscard]] std::string new_player_id(const SharedData& shared, std::uint64_t random_bits);

/// A name of at least one and at most `name_max_length` characters, each A to Z or a space.
[[nodiscard]] bool is_valid_name(std::string_view name, const Tuning& tuning = Tuning::defaults()) noexcept;

/// What the name screen should hold for whatever was typed: letters in capitals, everything else except spaces dropped,
/// cut to the longest allowed name.
[[nodiscard]] std::string normalise_name(std::string_view typed, const Tuning& tuning = Tuning::defaults());

/// The score at the top of the list, or 0 when the list is empty.
[[nodiscard]] std::int64_t best_score(std::span<const ScoreEntry> top10) noexcept;

/// The place (1 is the top) a final score would take, or nothing when it does not qualify. A score qualifies when it is
/// above 0 and either the list has room or it beats the last entry. Against an equal score the older entry ranks first.
[[nodiscard]] std::optional<int> rank_for_score(std::span<const ScoreEntry> top10, std::int64_t score,
                                                const Tuning& tuning = Tuning::defaults()) noexcept;

enum class ScoreError
{
    NOT_QUALIFYING,
    NAME_INVALID,
    PLAYER_NOT_FOUND, // `record_score` only: no active player has that id
    SEQ_EXHAUSTED,    // `record_score` only: `next_seq` is at MAX_SAVED_COUNTER
};

/// Puts a qualifying entry in the list under its rank and drops the entry pushed off the end. Returns the rank. The
/// list is left as it was on an error. The entry's `device` and `seq` are kept as given.
[[nodiscard]] std::expected<int, ScoreError> insert_score(std::vector<ScoreEntry>& top10, ScoreEntry entry,
                                                          const Tuning& tuning = Tuning::defaults());

/// Why a player, wallet or device id change was refused. The data is left as it was.
enum class PlayerError
{
    ID_INVALID,        // not 16 lowercase hex digits (a new player or device), or not a device id (a coin entry)
    ID_TAKEN,          // a player with that id is already in the list, removed or not
    NOT_FOUND,         // no active player with that id
    NAME_INVALID,      // empty, too long, or not A to Z and space
    TOO_MANY_PLAYERS,  // the active count is already `max_players`
    LIST_FULL,         // the list, removed players included, is at MAX_SAVED_PLAYERS
    LAST_PLAYER,       // removing the only active player
    WALLET_INVALID,    // a negative wallet
    COIN_ENTRIES_FULL, // a new device would be the 65th for this player
    COUNTER_FULL,      // the change would take a counter past MAX_SAVED_COUNTER
    DEVICE_ID_SET,     // the device already has an id
    DEVICE_ID_IN_USE,  // the new id already appears on a coin entry or a top 10 entry
};

/// The wallet of a player: the sum of `earned` less the sum of `spent`, over every device, and never below 0.
[[nodiscard]] std::int64_t player_wallet(const Player& player) noexcept;

/// Records a wallet the player now has, as seen from one device, so that `player_wallet` reads `wallet` afterwards. A
/// rise adds to that device's `earned` and a fall to its `spent`; neither counter ever goes down. No change when the
/// wallet is the same. A device with no entry yet gets one (`device_id` may be empty, as before the app assigns one).
/// Should the sums read below 0 (a merge can do that), a rise also clears that shortfall, so the wallet reads as asked.
[[nodiscard]] std::expected<void, PlayerError> record_wallet(Player& player, std::string_view device_id,
                                                             std::int64_t wallet);

/// The player with this id, removed or not, or null.
[[nodiscard]] Player* find_player(SharedData& shared, std::string_view id) noexcept;
[[nodiscard]] const Player* find_player(const SharedData& shared, std::string_view id) noexcept;

/// The current player: the active player `device.current_player` names, or null when it names nobody active.
[[nodiscard]] Player* current_player(SaveData& data) noexcept;
[[nodiscard]] const Player* current_player(const SaveData& data) noexcept;

/// How many players are not removed.
[[nodiscard]] int active_player_count(const SharedData& shared) noexcept;

/// Adds an unnamed player with the id the caller made (`new_player_id`). Refused when the active count is already
/// `max_players` (a removed player does not count), when the id is not 16 lowercase hex digits or is already in the
/// list, or when the list is full. The player goes in at its canonical place (see `player_id_precedes`). Does not
/// change the current player.
[[nodiscard]] std::expected<void, PlayerError> add_player(SharedData& shared, std::string id,
                                                          const Tuning& tuning = Tuning::defaults());

/// Gives an active player a name, which must be a valid name (not empty), and raises `name_rev` by one every time,
/// whether or not the name changed.
[[nodiscard]] std::expected<void, PlayerError> rename_player(SharedData& shared, std::string_view id, std::string name,
                                                             const Tuning& tuning = Tuning::defaults());

/// Marks an active player removed. The last active player cannot be removed. Does not change the current player: see
/// `ensure_current_player`.
[[nodiscard]] std::expected<void, PlayerError> remove_player(SharedData& shared, std::string_view id);

/// Sets `device.id` to the id the caller made, and gives the new id to every coin entry and top 10 entry that carries
/// the empty one. Refused when the device already has an id, when the id is not 16 lowercase hex digits, or when it
/// already appears on an entry.
[[nodiscard]] std::expected<void, PlayerError> assign_device_id(SaveData& data, std::string id);

/// Makes sure the current player is an active player. When `device.current_player` does not name one, it becomes the
/// first active player in the list. When no player is active at all, an unnamed one is added with `new_player_id` and
/// becomes current. Fails only in that last case, with the errors of `add_player`.
[[nodiscard]] std::expected<void, PlayerError> ensure_current_player(SaveData& data, std::string new_player_id,
                                                                     const Tuning& tuning = Tuning::defaults());

/// Raises a player's best score to `score` when it is higher.
void record_best(Player& player, std::int64_t score) noexcept;

/// Puts a final score in the shared top 10 for an active player, as an entry of this device that takes
/// `device.next_seq` (which then advances), and raises that player's best. The name is the one the caller gives (a
/// valid name; today the one typed at the name screen). Returns the rank. Everything is left as it was on an error.
[[nodiscard]] std::expected<int, ScoreError> record_score(SaveData& data, std::string_view player_id, std::string name,
                                                          std::int64_t score,
                                                          const Tuning& tuning = Tuning::defaults());

/// Makes an active player the current one: the choice WHO'S PLAYING offers. Refused with NOT_FOUND when no active
/// player has that id (a removed player counts as none), and the data is then left as it was. Changes nothing else, so
/// the caller records the old player's wallet first and gives the session the new one's.
[[nodiscard]] std::expected<void, PlayerError> set_current_player(SaveData& data, std::string_view id);

} // namespace heart_boom_maths
