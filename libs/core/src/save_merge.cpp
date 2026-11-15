// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_merge.hpp"

#include "heart_boom_maths/profile.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace heart_boom_maths
{

namespace
{

bool coin_precedes(const CoinEntry& lhs, const CoinEntry& rhs) noexcept
{
    return lhs.device < rhs.device;
}

// The canonical order of the top 10. Two entries with one identity (`device` and `seq`) compare equal.
bool score_precedes(const ScoreEntry& lhs, const ScoreEntry& rhs) noexcept
{
    if (lhs.score != rhs.score)
    {
        return lhs.score > rhs.score;
    }
    if (lhs.device != rhs.device)
    {
        return lhs.device < rhs.device;
    }
    return lhs.seq < rhs.seq;
}

bool same_entry(const ScoreEntry& lhs, const ScoreEntry& rhs) noexcept
{
    return lhs.device == rhs.device && lhs.seq == rhs.seq;
}

// Of two entries with one identity, the one that is kept comes first: the higher score, then the name that sorts first.
bool identity_then_better(const ScoreEntry& lhs, const ScoreEntry& rhs) noexcept
{
    if (lhs.device != rhs.device)
    {
        return lhs.device < rhs.device;
    }
    if (lhs.seq != rhs.seq)
    {
        return lhs.seq < rhs.seq;
    }
    if (lhs.score != rhs.score)
    {
        return lhs.score > rhs.score;
    }
    return lhs.name < rhs.name;
}

// Folds the entries of one device into one, taking the larger counter of each kind. `coins` must be sorted by device.
void fold_coins(std::vector<CoinEntry>& coins)
{
    std::size_t kept = 0;
    for (std::size_t i = 0; i < coins.size(); ++i)
    {
        if (kept > 0 && coins[kept - 1].device == coins[i].device)
        {
            coins[kept - 1].earned = std::max(coins[kept - 1].earned, coins[i].earned);
            coins[kept - 1].spent = std::max(coins[kept - 1].spent, coins[i].spent);
        }
        else
        {
            if (kept != i)
            {
                coins[kept] = std::move(coins[i]);
            }
            ++kept;
        }
    }
    coins.resize(kept);
}

// Folds `other`, a player with the same id, into `into`. The coin entries are joined here and folded by the caller.
void merge_player(Player& into, Player&& other)
{
    if (other.name_rev > into.name_rev || (other.name_rev == into.name_rev && other.name < into.name))
    {
        into.name = std::move(other.name);
        into.name_rev = other.name_rev;
    }
    into.removed = into.removed || other.removed;
    into.won_once = into.won_once || other.won_once;
    into.best = std::max(into.best, other.best);
    into.coins.insert(into.coins.end(), std::make_move_iterator(other.coins.begin()),
                      std::make_move_iterator(other.coins.end()));
}

void cut_top10(std::vector<ScoreEntry>& top10, const Tuning& tuning)
{
    const auto limit = static_cast<std::size_t>(std::max(tuning.top10_size, 0));
    if (top10.size() > limit)
    {
        top10.resize(limit);
    }
}

} // namespace

bool player_id_precedes(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs == FIRST_PLAYER_ID || rhs == FIRST_PLAYER_ID)
    {
        return lhs == FIRST_PLAYER_ID && rhs != FIRST_PLAYER_ID;
    }
    return lhs < rhs;
}

SharedData canonical(SharedData shared, const Tuning& tuning)
{
    std::ranges::stable_sort(shared.players,
                             [](const Player& lhs, const Player& rhs) { return player_id_precedes(lhs.id, rhs.id); });
    for (Player& player : shared.players)
    {
        std::ranges::stable_sort(player.coins, coin_precedes);
    }
    std::ranges::stable_sort(shared.top10, score_precedes);
    cut_top10(shared.top10, tuning);
    return shared;
}

std::expected<SharedData, MergeError> merge(const SharedData& lhs, const SharedData& rhs, const Tuning& tuning)
{
    HBM_ZONE("save.merge");

    // Every player of both sides in id order, so the ones with one id are next to each other.
    std::vector<Player> all;
    all.reserve(lhs.players.size() + rhs.players.size());
    all.insert(all.end(), lhs.players.begin(), lhs.players.end());
    all.insert(all.end(), rhs.players.begin(), rhs.players.end());
    std::ranges::stable_sort(all, [](const Player& a, const Player& b) { return player_id_precedes(a.id, b.id); });

    SharedData result;
    result.players.clear(); // the default holds the first player; the inputs say which players there are
    for (Player& player : all)
    {
        if (!result.players.empty() && result.players.back().id == player.id)
        {
            merge_player(result.players.back(), std::move(player));
        }
        else
        {
            result.players.push_back(std::move(player));
        }
    }
    if (result.players.size() > MAX_SAVED_PLAYERS)
    {
        return std::unexpected(MergeError::PLAYERS_TOO_MANY);
    }
    for (Player& player : result.players)
    {
        std::ranges::stable_sort(player.coins, coin_precedes);
        fold_coins(player.coins);
        if (player.coins.size() > MAX_SAVED_COIN_ENTRIES)
        {
            return std::unexpected(MergeError::COIN_ENTRIES_TOO_MANY);
        }
    }

    std::vector<ScoreEntry> entries;
    entries.reserve(lhs.top10.size() + rhs.top10.size());
    entries.insert(entries.end(), lhs.top10.begin(), lhs.top10.end());
    entries.insert(entries.end(), rhs.top10.begin(), rhs.top10.end());
    // Entries of one identity sit together with the one to keep first, and the rest are dropped.
    std::ranges::stable_sort(entries, identity_then_better);
    for (ScoreEntry& entry : entries)
    {
        if (result.top10.empty() || !same_entry(result.top10.back(), entry))
        {
            result.top10.push_back(std::move(entry));
        }
    }
    std::ranges::stable_sort(result.top10, score_precedes);
    cut_top10(result.top10, tuning);
    return result;
}

std::expected<void, PlayerError> adopt_shared(SaveData& local, SharedData merged, std::string new_player_id,
                                              const Tuning& tuning)
{
    // Repaired on a copy, so a refusal leaves the local save as it was.
    SaveData next{local.version, local.device, std::move(merged)};
    if (auto ensured = ensure_current_player(next, std::move(new_player_id), tuning); !ensured)
    {
        return ensured;
    }
    if (next.device.run)
    {
        const Player* owner = find_player(next.shared, next.device.run_player);
        if (owner == nullptr || owner->removed)
        {
            next.device.run.reset();
            next.device.run_player.clear();
        }
    }
    local = std::move(next);
    return {};
}

} // namespace heart_boom_maths
