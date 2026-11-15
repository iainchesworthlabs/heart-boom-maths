// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_data.hpp"

#include "heart_boom_maths/save_merge.hpp"

#include <algorithm>
#include <utility>

namespace heart_boom_maths
{

namespace
{

bool is_name_character(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') || c == ' ';
}

bool is_lowercase_hex(std::string_view text) noexcept
{
    return text.size() == ID_HEX_DIGITS &&
           std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

constexpr std::size_t COUNTER_HEX_DIGITS = 4;

// A device that has no id yet writes the empty one, so the empty id is a legal value in an entry.
bool is_entry_device(std::string_view device) noexcept
{
    return device.empty() || is_valid_device_id(device);
}

bool in_counter_range(std::int64_t value) noexcept
{
    return value >= 0 && value <= MAX_SAVED_COUNTER;
}

std::expected<void, SaveError> validate_device(const DeviceData& device)
{
    if (!is_entry_device(device.id))
    {
        return std::unexpected(SaveError::DEVICE_ID_INVALID);
    }
    if (!is_valid_player_id(device.current_player))
    {
        return std::unexpected(SaveError::PLAYER_ID_INVALID);
    }
    if (!in_counter_range(device.next_seq))
    {
        return std::unexpected(SaveError::COUNTER_OUT_OF_RANGE);
    }
    if (device.run && device.run->size() > MAX_SAVED_RUN_VALUES)
    {
        return std::unexpected(SaveError::RUN_TOO_LONG);
    }
    if (device.run)
    {
        if (!is_valid_player_id(device.run_player))
        {
            return std::unexpected(SaveError::RUN_PLAYER_MISSING);
        }
    }
    else if (!device.run_player.empty())
    {
        return std::unexpected(SaveError::RUN_PLAYER_MISSING);
    }
    return {};
}

std::expected<void, SaveError> validate_player(const Player& player, const Tuning& tuning)
{
    if (!is_valid_player_id(player.id))
    {
        return std::unexpected(SaveError::PLAYER_ID_INVALID);
    }
    if (!player.name.empty() && !is_valid_name(player.name, tuning))
    {
        return std::unexpected(SaveError::PLAYER_NAME_INVALID);
    }
    if (!in_counter_range(player.name_rev) || player.best < 0)
    {
        return std::unexpected(SaveError::COUNTER_OUT_OF_RANGE);
    }
    if (player.coins.size() > MAX_SAVED_COIN_ENTRIES)
    {
        return std::unexpected(SaveError::COIN_ENTRIES_TOO_MANY);
    }
    for (std::size_t i = 0; i < player.coins.size(); ++i)
    {
        const CoinEntry& entry = player.coins[i];
        if (!is_entry_device(entry.device))
        {
            return std::unexpected(SaveError::DEVICE_ID_INVALID);
        }
        if (entry.earned < 0 || entry.earned > MAX_SAVED_COINS || entry.spent < 0 || entry.spent > MAX_SAVED_COINS)
        {
            return std::unexpected(SaveError::COINS_OUT_OF_RANGE);
        }
        for (std::size_t j = 0; j < i; ++j)
        {
            if (player.coins[j].device == entry.device)
            {
                return std::unexpected(SaveError::COIN_DEVICE_DUPLICATE);
            }
        }
    }
    return {};
}

std::expected<void, SaveError> validate_shared(const SharedData& shared, const Tuning& tuning)
{
    if (shared.players.size() > MAX_SAVED_PLAYERS)
    {
        return std::unexpected(SaveError::PLAYERS_TOO_MANY);
    }
    for (std::size_t i = 0; i < shared.players.size(); ++i)
    {
        if (auto valid = validate_player(shared.players[i], tuning); !valid)
        {
            return valid;
        }
        for (std::size_t j = 0; j < i; ++j)
        {
            if (shared.players[j].id == shared.players[i].id)
            {
                return std::unexpected(SaveError::PLAYER_ID_DUPLICATE);
            }
        }
    }
    if (shared.top10.size() > static_cast<std::size_t>(tuning.top10_size))
    {
        return std::unexpected(SaveError::TOP10_TOO_LONG);
    }
    // Only the descending score is checked: of equal scores the order is the merge's business, not the format's.
    for (std::size_t i = 0; i < shared.top10.size(); ++i)
    {
        const ScoreEntry& entry = shared.top10[i];
        if (!is_valid_name(entry.name, tuning))
        {
            return std::unexpected(SaveError::NAME_INVALID);
        }
        if (entry.score < 1)
        {
            return std::unexpected(SaveError::SCORE_INVALID);
        }
        if (!is_entry_device(entry.device))
        {
            return std::unexpected(SaveError::DEVICE_ID_INVALID);
        }
        if (!in_counter_range(entry.seq))
        {
            return std::unexpected(SaveError::COUNTER_OUT_OF_RANGE);
        }
        if (i > 0 && entry.score > shared.top10[i - 1].score)
        {
            return std::unexpected(SaveError::TOP10_NOT_SORTED);
        }
    }
    return {};
}

} // namespace

bool is_valid_device_id(std::string_view id) noexcept
{
    return is_lowercase_hex(id);
}

bool is_valid_player_id(std::string_view id) noexcept
{
    return id == FIRST_PLAYER_ID || is_lowercase_hex(id);
}

std::string hex_id(std::uint64_t bits)
{
    static constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string id(ID_HEX_DIGITS, '0');
    for (std::size_t i = ID_HEX_DIGITS; i > 0; --i)
    {
        id[i - 1] = DIGITS[bits & 0xF];
        bits >>= 4;
    }
    return id;
}

std::string new_player_id(const SharedData& shared, std::uint64_t random_bits)
{
    constexpr std::uint64_t COUNTER_MAX = 0xFFFF;
    constexpr int RANDOM_BITS = 48;
    constexpr std::uint64_t RANDOM_MASK = (std::uint64_t{1} << RANDOM_BITS) - 1;

    std::uint64_t largest = 0;
    for (const Player& player : shared.players)
    {
        if (!is_lowercase_hex(player.id))
        {
            continue; // `first`, which has no counter
        }
        std::uint64_t counter = 0;
        for (std::size_t i = 0; i < COUNTER_HEX_DIGITS; ++i)
        {
            const char c = player.id[i];
            counter = counter * 16 + static_cast<std::uint64_t>(c <= '9' ? c - '0' : c - 'a' + 10);
        }
        largest = std::max(largest, counter);
    }
    const std::uint64_t counter = std::min(largest + 1, COUNTER_MAX);
    return hex_id((counter << RANDOM_BITS) | (random_bits & RANDOM_MASK));
}

bool is_valid_name(std::string_view name, const Tuning& tuning) noexcept
{
    return !name.empty() && name.size() <= static_cast<std::size_t>(tuning.name_max_length) &&
           std::ranges::all_of(name, is_name_character);
}

std::string normalise_name(std::string_view typed, const Tuning& tuning)
{
    std::string result;
    for (const char c : typed)
    {
        if (result.size() >= static_cast<std::size_t>(tuning.name_max_length))
        {
            break;
        }
        const char upper = c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
        if (is_name_character(upper))
        {
            result.push_back(upper);
        }
    }
    return result;
}

std::expected<void, SaveError> validate(const SaveData& data, const Tuning& tuning, int expected_version)
{
    if (data.version != expected_version)
    {
        return std::unexpected(SaveError::BAD_VERSION);
    }
    if (auto valid = validate_device(data.device); !valid)
    {
        return valid;
    }
    return validate_shared(data.shared, tuning);
}

std::int64_t best_score(std::span<const ScoreEntry> top10) noexcept
{
    return top10.empty() ? 0 : top10.front().score;
}

std::optional<int> rank_for_score(std::span<const ScoreEntry> top10, std::int64_t score, const Tuning& tuning) noexcept
{
    if (score < 1)
    {
        return std::nullopt;
    }
    // Entries at or above the new score stay ahead of it, which is what makes the older of two equal scores rank first.
    const auto ahead = std::ranges::count_if(top10, [score](const ScoreEntry& entry) { return entry.score >= score; });
    if (ahead >= tuning.top10_size)
    {
        return std::nullopt;
    }
    return static_cast<int>(ahead) + 1;
}

std::expected<int, ScoreError> insert_score(std::vector<ScoreEntry>& top10, ScoreEntry entry, const Tuning& tuning)
{
    const std::optional<int> rank = rank_for_score(top10, entry.score, tuning);
    if (!rank)
    {
        return std::unexpected(ScoreError::NOT_QUALIFYING);
    }
    if (!is_valid_name(entry.name, tuning))
    {
        return std::unexpected(ScoreError::NAME_INVALID);
    }
    top10.insert(top10.begin() + (*rank - 1), std::move(entry));
    if (top10.size() > static_cast<std::size_t>(tuning.top10_size))
    {
        top10.resize(static_cast<std::size_t>(tuning.top10_size));
    }
    return *rank;
}

std::int64_t player_wallet(const Player& player) noexcept
{
    std::int64_t net = 0;
    for (const CoinEntry& entry : player.coins)
    {
        net += entry.earned - entry.spent;
    }
    return std::max<std::int64_t>(net, 0);
}

std::expected<void, PlayerError> record_wallet(Player& player, std::string_view device_id, std::int64_t wallet)
{
    if (wallet < 0)
    {
        return std::unexpected(PlayerError::WALLET_INVALID);
    }
    std::int64_t net = 0;
    for (const CoinEntry& entry : player.coins)
    {
        net += entry.earned - entry.spent;
    }
    const std::int64_t current = std::max<std::int64_t>(net, 0);
    if (wallet == current)
    {
        return {};
    }

    const auto found =
        std::ranges::find_if(player.coins, [device_id](const CoinEntry& e) { return e.device == device_id; });
    const bool is_new = found == player.coins.end();
    if (is_new)
    {
        if (!is_entry_device(device_id))
        {
            return std::unexpected(PlayerError::ID_INVALID);
        }
        if (player.coins.size() >= MAX_SAVED_COIN_ENTRIES)
        {
            return std::unexpected(PlayerError::COIN_ENTRIES_FULL);
        }
    }

    const bool rise = wallet > current;
    // A rise from a negative net clears the shortfall too, so the wallet reads as asked.
    const std::int64_t amount = rise ? wallet - net : current - wallet;
    const std::int64_t base = is_new ? 0 : (rise ? found->earned : found->spent);
    if (amount > MAX_SAVED_COUNTER - base)
    {
        return std::unexpected(PlayerError::COUNTER_FULL);
    }

    CoinEntry& entry = is_new ? player.coins.emplace_back(CoinEntry{std::string(device_id), 0, 0}) : *found;
    (rise ? entry.earned : entry.spent) += amount;
    return {};
}

Player* find_player(SharedData& shared, std::string_view id) noexcept
{
    const auto found = std::ranges::find_if(shared.players, [id](const Player& p) { return p.id == id; });
    return found == shared.players.end() ? nullptr : &*found;
}

const Player* find_player(const SharedData& shared, std::string_view id) noexcept
{
    const auto found = std::ranges::find_if(shared.players, [id](const Player& p) { return p.id == id; });
    return found == shared.players.end() ? nullptr : &*found;
}

Player* current_player(SaveData& data) noexcept
{
    Player* player = find_player(data.shared, data.device.current_player);
    return player != nullptr && !player->removed ? player : nullptr;
}

const Player* current_player(const SaveData& data) noexcept
{
    const Player* player = find_player(data.shared, data.device.current_player);
    return player != nullptr && !player->removed ? player : nullptr;
}

int active_player_count(const SharedData& shared) noexcept
{
    return static_cast<int>(std::ranges::count_if(shared.players, [](const Player& p) { return !p.removed; }));
}

std::expected<void, PlayerError> add_player(SharedData& shared, std::string id, const Tuning& tuning)
{
    if (!is_lowercase_hex(id))
    {
        return std::unexpected(PlayerError::ID_INVALID);
    }
    if (find_player(shared, id) != nullptr)
    {
        return std::unexpected(PlayerError::ID_TAKEN);
    }
    if (active_player_count(shared) >= tuning.max_players)
    {
        return std::unexpected(PlayerError::TOO_MANY_PLAYERS);
    }
    if (shared.players.size() >= MAX_SAVED_PLAYERS)
    {
        return std::unexpected(PlayerError::LIST_FULL);
    }
    // At its canonical place, so a list that a merge made stays in the order the merge gives.
    const auto place =
        std::ranges::find_if(shared.players, [&id](const Player& p) { return player_id_precedes(id, p.id); });
    shared.players.insert(place, Player{.id = std::move(id)});
    return {};
}

std::expected<void, PlayerError> rename_player(SharedData& shared, std::string_view id, std::string name,
                                               const Tuning& tuning)
{
    Player* player = find_player(shared, id);
    if (player == nullptr || player->removed)
    {
        return std::unexpected(PlayerError::NOT_FOUND);
    }
    if (!is_valid_name(name, tuning))
    {
        return std::unexpected(PlayerError::NAME_INVALID);
    }
    if (player->name_rev >= MAX_SAVED_COUNTER)
    {
        return std::unexpected(PlayerError::COUNTER_FULL);
    }
    player->name = std::move(name);
    ++player->name_rev;
    return {};
}

std::expected<void, PlayerError> remove_player(SharedData& shared, std::string_view id)
{
    Player* player = find_player(shared, id);
    if (player == nullptr || player->removed)
    {
        return std::unexpected(PlayerError::NOT_FOUND);
    }
    if (active_player_count(shared) <= 1)
    {
        return std::unexpected(PlayerError::LAST_PLAYER);
    }
    player->removed = true;
    return {};
}

std::expected<void, PlayerError> assign_device_id(SaveData& data, std::string id)
{
    if (!data.device.id.empty())
    {
        return std::unexpected(PlayerError::DEVICE_ID_SET);
    }
    if (!is_valid_device_id(id))
    {
        return std::unexpected(PlayerError::ID_INVALID);
    }
    const auto uses_id = [&id](const auto& entry) { return entry.device == id; };
    const bool in_use = std::ranges::any_of(data.shared.top10, uses_id) ||
                        std::ranges::any_of(data.shared.players,
                                            [&](const Player& p) { return std::ranges::any_of(p.coins, uses_id); });
    if (in_use)
    {
        return std::unexpected(PlayerError::DEVICE_ID_IN_USE);
    }
    for (Player& player : data.shared.players)
    {
        for (CoinEntry& entry : player.coins)
        {
            if (entry.device.empty())
            {
                entry.device = id;
            }
        }
    }
    for (ScoreEntry& entry : data.shared.top10)
    {
        if (entry.device.empty())
        {
            entry.device = id;
        }
    }
    data.device.id = std::move(id);
    return {};
}

std::expected<void, PlayerError> ensure_current_player(SaveData& data, std::string new_player_id, const Tuning& tuning)
{
    if (current_player(data) != nullptr)
    {
        return {};
    }
    const auto first_active = std::ranges::find_if(data.shared.players, [](const Player& p) { return !p.removed; });
    if (first_active != data.shared.players.end())
    {
        data.device.current_player = first_active->id;
        return {};
    }
    std::string id = new_player_id;
    if (auto added = add_player(data.shared, std::move(new_player_id), tuning); !added)
    {
        return added;
    }
    data.device.current_player = std::move(id);
    return {};
}

void record_best(Player& player, std::int64_t score) noexcept
{
    player.best = std::max(player.best, score);
}

std::expected<int, ScoreError> record_score(SaveData& data, std::string_view player_id, std::string name,
                                            std::int64_t score, const Tuning& tuning)
{
    Player* player = find_player(data.shared, player_id);
    if (player == nullptr || player->removed)
    {
        return std::unexpected(ScoreError::PLAYER_NOT_FOUND);
    }
    if (data.device.next_seq >= MAX_SAVED_COUNTER)
    {
        return std::unexpected(ScoreError::SEQ_EXHAUSTED);
    }
    auto rank = insert_score(data.shared.top10,
                             ScoreEntry{std::move(name), score, data.device.id, data.device.next_seq}, tuning);
    if (!rank)
    {
        return rank;
    }
    ++data.device.next_seq;
    record_best(*player, score);
    return rank;
}

std::expected<void, PlayerError> set_current_player(SaveData& data, std::string_view id)
{
    const Player* player = find_player(data.shared, id);
    if (player == nullptr || player->removed)
    {
        return std::unexpected(PlayerError::NOT_FOUND);
    }
    data.device.current_player = std::string(id);
    return {};
}

} // namespace heart_boom_maths
