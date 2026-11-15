// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace heart_boom_maths
{

/// Why two shared parts were not merged. Neither input is changed.
enum class MergeError
{
    PLAYERS_TOO_MANY,      // the two sets together are more than MAX_SAVED_PLAYERS
    COIN_ENTRIES_TOO_MANY, // one player's entries from both sides are more than MAX_SAVED_COIN_ENTRIES
};

/// The order of the players in canonical form: `first` before every other id, then ascending id.
[[nodiscard]] bool player_id_precedes(std::string_view lhs, std::string_view rhs) noexcept;

/// Puts the shared part in the one form a merge produces: players by `player_id_precedes`; each player's coin entries
/// by ascending device; the top 10 by descending score, then ascending device, then ascending seq, cut to `top10_size`.
/// It does not remove duplicates: that is the merge's work.
[[nodiscard]] SharedData canonical(SharedData shared, const Tuning& tuning = Tuning::defaults());

/// Joins the shared parts of two devices so that nothing either player earned or was given is lost. Merging in either
/// order, in any grouping, or merging the same data again gives the same result, which is in canonical form.
///
/// - Players are matched by id. The name with the higher `name_rev` wins; on equal counters the name that sorts first.
///   `removed` and `won_once` hold if they hold on either side, and a removed player still merges its other fields.
///   `best` is the larger.
/// - Coin entries are matched by device, and each counter is the larger of the two.
/// - Top 10 entries with the same `device` and `seq` are one entry. Should two of them differ (a damaged file), the
///   higher score is kept, then the name that sorts first. The best `top10_size` are kept.
///
/// More than `max_players` active players is not an error (decision P6). A result that would break a format bound is an
/// error, and nothing is cut to fit: a cut would make the result depend on the order of merging. The inputs are
/// expected to be valid. No clock and no randomness is read.
[[nodiscard]] std::expected<SharedData, MergeError> merge(const SharedData& lhs, const SharedData& rhs,
                                                          const Tuning& tuning = Tuning::defaults());

/// Makes a merged shared part the local one and repairs what stays on the device. When `current_player` no longer
/// names an active player, `ensure_current_player` picks one (and, when nobody is active, adds one with
/// `new_player_id`). A saved run whose player is now removed or missing is dropped, with its `run_player`. `next_seq`
/// is raised to one past the largest `seq` of this device's own entries in the merged top 10 when that is larger, and
/// never lowered, so a device restored from an older backup does not give a new entry a number it has already used
/// (an empty device id matches the entries with an empty device). Every other device field is left alone. `merged` is
/// taken as given and is not checked: it comes from `merge` or from a payload that `parse_shared` has validated. Fails
/// as `ensure_current_player` does, and with COUNTER_FULL when `next_seq` would have to pass MAX_SAVED_COUNTER; then
/// `local` is unchanged.
[[nodiscard]] std::expected<void, PlayerError>
adopt_shared(SaveData& local, SharedData merged, std::string new_player_id, const Tuning& tuning = Tuning::defaults());

} // namespace heart_boom_maths
