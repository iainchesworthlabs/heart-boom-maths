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
inline constexpr int SAVE_VERSION = 1;

/// Format bounds, not game numbers. They keep a damaged or hostile file from putting a value in the game that no
/// version of it could have written: the wallet is a 32-bit count, and a saved run is a few dozen integers.
inline constexpr std::int64_t MAX_SAVED_COINS = 2147483647;
inline constexpr std::size_t MAX_SAVED_RUN_VALUES = 4096;

struct ScoreEntry
{
    std::string name;
    std::int64_t score = 0;

    friend bool operator==(const ScoreEntry&, const ScoreEntry&) = default;
};

/// Everything the game keeps between sessions. The run is a list of integers made by `GameSession::snapshot()`; the
/// save logic does not look inside it.
struct SaveData
{
    int version = SAVE_VERSION;
    std::int64_t coins = 0;
    bool won_once = false;
    bool sound = true;
    bool music = true;
    bool game_center = false;
    std::vector<ScoreEntry> top10; // best first; of equal scores the older entry comes first
    std::optional<std::vector<std::int64_t>> run;

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
    COINS_OUT_OF_RANGE,
    TOP10_TOO_LONG,
    NAME_INVALID,  // empty, too long, or not A to Z and space
    SCORE_INVALID, // below 1
    TOP10_NOT_SORTED,
    RUN_TOO_LONG,
};

/// Checks every rule of the format. `expected_version` is the schema the data claims to be.
[[nodiscard]] std::expected<void, SaveError> validate(const SaveData& data, const Tuning& tuning = Tuning::defaults(),
                                                      int expected_version = SAVE_VERSION);

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
};

/// Puts a qualifying score in the list under its rank and drops the entry pushed off the end. Returns the rank. The
/// list is left as it was on an error.
[[nodiscard]] std::expected<int, ScoreError> insert_score(std::vector<ScoreEntry>& top10, std::string name,
                                                          std::int64_t score,
                                                          const Tuning& tuning = Tuning::defaults());

} // namespace heart_boom_maths
