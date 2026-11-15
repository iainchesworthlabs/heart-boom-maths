// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace heart_boom_maths;

namespace
{

constexpr std::string_view DEVICE_A = "aaaaaaaaaaaaaaaa";
constexpr std::string_view DEVICE_B = "bbbbbbbbbbbbbbbb";
constexpr std::string_view PLAYER_2 = "0000000000000002";
constexpr std::string_view PLAYER_3 = "0000000000000003";
constexpr std::string_view PLAYER_4 = "0000000000000004";
constexpr std::string_view PLAYER_5 = "0000000000000005";

// True when every counter of `after` is at least its counterpart in `before` (a device missing from `before` counts as
// zero), and no device has gone.
bool counters_only_grew(const Player& before, const Player& after)
{
    for (const CoinEntry& old : before.coins)
    {
        bool found = false;
        for (const CoinEntry& now : after.coins)
        {
            if (now.device == old.device)
            {
                found = true;
                if (now.earned < old.earned || now.spent < old.spent)
                {
                    return false;
                }
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

// Four players, one of them removed, for the list rules.
SaveData family()
{
    SaveData data;
    data.shared.players = {Player{.id = "first", .name = "ADA"}, Player{.id = std::string(PLAYER_2)},
                           Player{.id = std::string(PLAYER_3), .removed = true}};
    return data;
}

} // namespace

TEST_CASE("max_players is a game number of 4", "[unit][save][players]")
{
    REQUIRE(Tuning::defaults().max_players == 4);
}

TEST_CASE("the wallet is earned less spent over every device, and never below 0", "[unit][save][players]")
{
    Player player;
    REQUIRE(player_wallet(player) == 0);
    player.coins = {{std::string(DEVICE_A), 100, 30}, {std::string(DEVICE_B), 50, 10}};
    REQUIRE(player_wallet(player) == 110);
    player.coins = {{std::string(DEVICE_A), 10, 30}, {std::string(DEVICE_B), 5, 0}};
    REQUIRE(player_wallet(player) == 0); // 15 earned, 30 spent: a merge can order it so
    player.coins = {{"", 7, 7}};
    REQUIRE(player_wallet(player) == 0);
    player.coins = {{std::string(DEVICE_A), MAX_SAVED_COINS, 0}, {std::string(DEVICE_B), MAX_SAVED_COINS, 0}};
    REQUIRE(player_wallet(player) == 2 * MAX_SAVED_COINS);
}

TEST_CASE("a wallet that rose adds to earned and one that fell adds to spent", "[unit][save][players]")
{
    Player player;

    SECTION("the first rise makes the device's entry")
    {
        REQUIRE(record_wallet(player, DEVICE_A, 40).has_value());
        REQUIRE(player.coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 40, 0}});
        REQUIRE(player_wallet(player) == 40);
    }
    SECTION("a fall adds to spent and leaves earned alone")
    {
        REQUIRE(record_wallet(player, DEVICE_A, 40).has_value());
        REQUIRE(record_wallet(player, DEVICE_A, 25).has_value());
        REQUIRE(player.coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 40, 15}});
        REQUIRE(player_wallet(player) == 25);
        REQUIRE(record_wallet(player, DEVICE_A, 0).has_value());
        REQUIRE(player.coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 40, 40}});
        REQUIRE(record_wallet(player, DEVICE_A, 12).has_value());
        REQUIRE(player.coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 52, 40}});
        REQUIRE(player_wallet(player) == 12);
    }
    SECTION("the same wallet changes nothing, and makes no entry")
    {
        REQUIRE(record_wallet(player, DEVICE_A, 0).has_value());
        REQUIRE(player.coins.empty());
        REQUIRE(record_wallet(player, DEVICE_A, 9).has_value());
        const Player before = player;
        REQUIRE(record_wallet(player, DEVICE_A, 9).has_value());
        REQUIRE(record_wallet(player, DEVICE_B, 9).has_value());
        REQUIRE(player == before);
    }
    SECTION("each device keeps its own counters, and the wallet is the sum")
    {
        REQUIRE(record_wallet(player, DEVICE_A, 100).has_value());
        player.coins.push_back({std::string(DEVICE_B), 30, 0}); // earned elsewhere and synced in
        REQUIRE(player_wallet(player) == 130);
        REQUIRE(record_wallet(player, DEVICE_A, 150).has_value()); // this device earned 20 more
        REQUIRE(player.coins[0] == CoinEntry{std::string(DEVICE_A), 120, 0});
        REQUIRE(record_wallet(player, DEVICE_B, 100).has_value()); // the other device spent 50
        REQUIRE(player.coins[1] == CoinEntry{std::string(DEVICE_B), 30, 50});
        REQUIRE(player_wallet(player) == 100);
    }
    SECTION("before a device id exists, the empty id is used")
    {
        REQUIRE(record_wallet(player, "", 5).has_value());
        REQUIRE(player.coins == std::vector<CoinEntry>{{"", 5, 0}});
    }
    SECTION("a wallet read as 0 from a shortfall is brought to the amount asked")
    {
        player.coins = {{std::string(DEVICE_A), 10, 30}, {std::string(DEVICE_B), 5, 0}}; // 15 earned, 30 spent
        REQUIRE(player_wallet(player) == 0);
        REQUIRE(record_wallet(player, DEVICE_A, 0).has_value()); // already 0: nothing to record
        REQUIRE(player.coins[0] == CoinEntry{std::string(DEVICE_A), 10, 30});
        REQUIRE(record_wallet(player, DEVICE_A, 7).has_value());
        REQUIRE(player_wallet(player) == 7);
        REQUIRE(player.coins[0] == CoinEntry{std::string(DEVICE_A), 10 + 7 + 15, 30});
    }
}

TEST_CASE("recording a wallet refuses what it cannot keep", "[unit][save][players]")
{
    Player player{.id = "first"};
    REQUIRE(record_wallet(player, DEVICE_A, 10).has_value());
    const Player before = player;

    REQUIRE(record_wallet(player, DEVICE_A, -1) == std::unexpected(PlayerError::WALLET_INVALID));
    REQUIRE(record_wallet(player, "not an id", 20) == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(record_wallet(player, "AAAAAAAAAAAAAAAA", 20) == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(record_wallet(player, DEVICE_A, MAX_SAVED_COINS + 1) == std::unexpected(PlayerError::COUNTER_FULL));
    REQUIRE(player == before);

    // A counter at its bound cannot take a rise, but a rise to the very bound is fine.
    REQUIRE(record_wallet(player, DEVICE_A, MAX_SAVED_COINS).has_value());
    REQUIRE(player.coins[0].earned == MAX_SAVED_COINS);
    SaveData holder;
    holder.shared.players = {player};
    REQUIRE(validate(holder).has_value());
    REQUIRE(record_wallet(player, DEVICE_A, 0).has_value());
    REQUIRE(player.coins[0].spent == MAX_SAVED_COINS);
    const Player full = player;
    REQUIRE(record_wallet(player, DEVICE_A, 1) == std::unexpected(PlayerError::COUNTER_FULL));
    REQUIRE(player == full);

    SECTION("a 65th device")
    {
        Player many;
        for (std::size_t i = 0; i < MAX_SAVED_COIN_ENTRIES; ++i)
        {
            REQUIRE(record_wallet(many, hex_id(i + 1), static_cast<std::int64_t>(i + 1)).has_value());
        }
        REQUIRE(many.coins.size() == MAX_SAVED_COIN_ENTRIES);
        const Player before_many = many;
        REQUIRE(record_wallet(many, hex_id(1000), 100000) == std::unexpected(PlayerError::COIN_ENTRIES_FULL));
        REQUIRE(many == before_many);
        // A device that already has an entry still works when the list is full.
        REQUIRE(record_wallet(many, hex_id(1), 1 + player_wallet(many)).has_value());
    }
}

TEST_CASE("earned and spent never decrease through any helper", "[unit][save][players]")
{
    SaveData data;
    REQUIRE(assign_device_id(data, std::string(DEVICE_A)).has_value());
    Player before = data.shared.players[0];
    // A long walk of wallets: up, down, to zero, up from a shortfall, the same, and another device's coins arriving.
    const std::int64_t walk[] = {5, 50, 20, 0, 0, 3, 3, 900, 899, 1, 1000, 0, 7};
    int step = 0;
    for (const std::int64_t wallet : walk)
    {
        INFO("step " << step++ << " wallet " << wallet);
        Player& player = data.shared.players[0];
        if (step == 6)
        {
            player.coins.push_back({std::string(DEVICE_B), 100, 400}); // spent more elsewhere than earned so far
        }
        before = player;
        REQUIRE(record_wallet(player, DEVICE_A, wallet).has_value());
        REQUIRE(player_wallet(player) == wallet);
        REQUIRE(counters_only_grew(before, player));
        REQUIRE(validate(data).has_value());
    }
    // The other helpers never touch a counter either.
    Player& player = data.shared.players[0];
    before = player;
    REQUIRE(rename_player(data.shared, "first", "ADA").has_value());
    REQUIRE(record_score(data, "first", "ADA", 100).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_2)).has_value());
    REQUIRE(remove_player(data.shared, PLAYER_2).has_value());
    REQUIRE(ensure_current_player(data, std::string(PLAYER_3)).has_value());
    REQUIRE(counters_only_grew(before, data.shared.players[0]));
}

TEST_CASE("a player is found by id, and the current player is the active one named", "[unit][save][players]")
{
    SaveData data = family();
    REQUIRE(find_player(data.shared, "first") == &data.shared.players[0]);
    REQUIRE(find_player(data.shared, PLAYER_2) == &data.shared.players[1]);
    REQUIRE(find_player(data.shared, PLAYER_3) == &data.shared.players[2]); // a removed player is still found
    REQUIRE(find_player(data.shared, PLAYER_4) == nullptr);
    REQUIRE(find_player(data.shared, "") == nullptr);
    const SharedData& read_only = data.shared;
    REQUIRE(find_player(read_only, "first") == &data.shared.players[0]);

    REQUIRE(current_player(data) == &data.shared.players[0]);
    data.device.current_player = std::string(PLAYER_2);
    const SaveData& read_only_data = data;
    REQUIRE(current_player(read_only_data) == &data.shared.players[1]);
    data.device.current_player = std::string(PLAYER_3); // removed
    REQUIRE(current_player(data) == nullptr);
    data.device.current_player = std::string(PLAYER_4); // not in the list
    REQUIRE(current_player(data) == nullptr);

    REQUIRE(active_player_count(data.shared) == 2);
    REQUIRE(active_player_count(SharedData{.players = {}}) == 0);
}

TEST_CASE("a player is added up to max_players, and only active players count", "[unit][save][players]")
{
    SaveData data;
    REQUIRE(active_player_count(data.shared) == 1);

    REQUIRE(add_player(data.shared, std::string(PLAYER_2)).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_3)).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_4)).has_value());
    REQUIRE(active_player_count(data.shared) == 4);
    // The new players are unnamed, not removed, and have no coins.
    REQUIRE(data.shared.players.back() == Player{.id = std::string(PLAYER_4)});
    REQUIRE(data.device.current_player == "first"); // adding does not switch player
    REQUIRE(validate(data).has_value());

    SECTION("the fifth is refused")
    {
        const SharedData before = data.shared;
        REQUIRE(add_player(data.shared, std::string(PLAYER_5)) == std::unexpected(PlayerError::TOO_MANY_PLAYERS));
        REQUIRE(data.shared == before);
    }
    SECTION("removing one makes room, and the removed one stays in the list")
    {
        REQUIRE(remove_player(data.shared, PLAYER_2).has_value());
        REQUIRE(add_player(data.shared, std::string(PLAYER_5)).has_value());
        REQUIRE(data.shared.players.size() == 5);
        REQUIRE(active_player_count(data.shared) == 4);
        REQUIRE(find_player(data.shared, PLAYER_2)->removed);
    }
    SECTION("a smaller max_players in the tuning is taken")
    {
        Tuning tuning = Tuning::defaults();
        tuning.max_players = 4;
        REQUIRE(add_player(data.shared, std::string(PLAYER_5), tuning) ==
                std::unexpected(PlayerError::TOO_MANY_PLAYERS));
        tuning.max_players = 5;
        REQUIRE(add_player(data.shared, std::string(PLAYER_5), tuning).has_value());
    }
    SECTION("more active players than max_players are valid, so a merged list loads")
    {
        for (const std::string_view id :
             std::array<std::string_view, 3>{PLAYER_5, "0000000000000006", "0000000000000007"})
        {
            data.shared.players.push_back(Player{.id = std::string(id)});
        }
        REQUIRE(active_player_count(data.shared) == 7);
        REQUIRE(validate(data).has_value());
        REQUIRE(parse_save(serialise(data).value()).data == data);
    }
}

TEST_CASE("a player is refused for a bad or repeated id, or a full list", "[unit][save][players]")
{
    SaveData data;
    const SharedData before = data.shared;
    REQUIRE(add_player(data.shared, "") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(add_player(data.shared, "first") == std::unexpected(PlayerError::ID_INVALID)); // not a new player's id
    REQUIRE(add_player(data.shared, "ABCDEF0123456789") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(add_player(data.shared, "123") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(data.shared == before);

    REQUIRE(add_player(data.shared, std::string(PLAYER_2)).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_2)) == std::unexpected(PlayerError::ID_TAKEN));
    // A removed player's id is taken too: reusing it would bring the old player back in a merge.
    REQUIRE(remove_player(data.shared, PLAYER_2).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_2)) == std::unexpected(PlayerError::ID_TAKEN));

    SECTION("the list itself is bounded")
    {
        SaveData crowded;
        crowded.shared.players.clear();
        for (std::size_t i = 0; i < MAX_SAVED_PLAYERS; ++i)
        {
            crowded.shared.players.push_back(Player{.id = hex_id(i + 1), .removed = i != 0});
        }
        REQUIRE(active_player_count(crowded.shared) == 1);
        REQUIRE(add_player(crowded.shared, hex_id(1000)) == std::unexpected(PlayerError::LIST_FULL));
        REQUIRE(crowded.shared.players.size() == MAX_SAVED_PLAYERS);
    }
}

TEST_CASE("a player is renamed with a valid name and the change counter rises", "[unit][save][players]")
{
    SaveData data = family();
    REQUIRE(data.shared.players[1].name.empty());
    REQUIRE(data.shared.players[1].name_rev == 0);

    REQUIRE(rename_player(data.shared, PLAYER_2, "BEN").has_value());
    REQUIRE(data.shared.players[1].name == "BEN");
    REQUIRE(data.shared.players[1].name_rev == 1);
    REQUIRE(rename_player(data.shared, PLAYER_2, "BEN K").has_value());
    REQUIRE(data.shared.players[1].name == "BEN K");
    REQUIRE(data.shared.players[1].name_rev == 2);
    REQUIRE(rename_player(data.shared, PLAYER_2, "BEN K").has_value()); // the same name still counts as a change
    REQUIRE(data.shared.players[1].name_rev == 3);
    REQUIRE(data.shared.players[0].name_rev == 0); // nobody else moved
    REQUIRE(validate(data).has_value());

    SECTION("refusals leave the player alone")
    {
        const SharedData before = data.shared;
        REQUIRE(rename_player(data.shared, PLAYER_2, "") == std::unexpected(PlayerError::NAME_INVALID));
        REQUIRE(rename_player(data.shared, PLAYER_2, "ben") == std::unexpected(PlayerError::NAME_INVALID));
        REQUIRE(rename_player(data.shared, PLAYER_2, "B3N") == std::unexpected(PlayerError::NAME_INVALID));
        REQUIRE(rename_player(data.shared, PLAYER_2, "ABCDEFGHIJKLM") == std::unexpected(PlayerError::NAME_INVALID));
        REQUIRE(rename_player(data.shared, PLAYER_4, "BEN") == std::unexpected(PlayerError::NOT_FOUND));
        REQUIRE(rename_player(data.shared, PLAYER_3, "BEN") == std::unexpected(PlayerError::NOT_FOUND)); // removed
        REQUIRE(rename_player(data.shared, "", "BEN") == std::unexpected(PlayerError::NOT_FOUND));
        REQUIRE(data.shared == before);
    }
    SECTION("a counter at its bound cannot rise")
    {
        data.shared.players[1].name_rev = MAX_SAVED_COUNTER;
        REQUIRE(rename_player(data.shared, PLAYER_2, "ZED") == std::unexpected(PlayerError::COUNTER_FULL));
        REQUIRE(data.shared.players[1].name == "BEN K");
    }
}

TEST_CASE("a player is removed by a mark, except the last one", "[unit][save][players]")
{
    SaveData data = family(); // first and PLAYER_2 active, PLAYER_3 already removed
    data.shared.players[1].coins = {{std::string(DEVICE_A), 30, 5}};
    data.shared.players[1].best = 700;

    REQUIRE(remove_player(data.shared, PLAYER_2).has_value());
    REQUIRE(data.shared.players.size() == 3); // kept, so a merge cannot bring the player back
    const Player& gone = data.shared.players[1];
    REQUIRE(gone.removed);
    REQUIRE(gone.coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 30, 5}}); // everything else stays
    REQUIRE(gone.best == 700);
    REQUIRE(active_player_count(data.shared) == 1);
    REQUIRE(validate(data).has_value());

    SECTION("the last active player cannot go")
    {
        const SharedData before = data.shared;
        REQUIRE(remove_player(data.shared, "first") == std::unexpected(PlayerError::LAST_PLAYER));
        REQUIRE(data.shared == before);
    }
    SECTION("removing again, or someone unknown, is refused")
    {
        REQUIRE(remove_player(data.shared, PLAYER_2) == std::unexpected(PlayerError::NOT_FOUND));
        REQUIRE(remove_player(data.shared, PLAYER_3) == std::unexpected(PlayerError::NOT_FOUND));
        REQUIRE(remove_player(data.shared, PLAYER_4) == std::unexpected(PlayerError::NOT_FOUND));
    }
    SECTION("removing the current player leaves the repair to ensure_current_player")
    {
        data.shared.players[1].removed = false;
        data.device.current_player = std::string(PLAYER_2);
        REQUIRE(remove_player(data.shared, PLAYER_2).has_value());
        REQUIRE(data.device.current_player == PLAYER_2);
        REQUIRE(current_player(data) == nullptr);
    }
}

TEST_CASE("the device id is assigned once and given to what carried none", "[unit][save][players]")
{
    SaveData data;
    data.shared.players[0].coins = {{"", 137, 0}};
    data.shared.players.push_back(
        Player{.id = std::string(PLAYER_2), .coins = {{"", 4, 1}, {std::string(DEVICE_B), 9, 0}}});
    data.shared.players.push_back(Player{.id = std::string(PLAYER_3)});
    data.shared.top10 = {{"ADA", 300, "", 1}, {"BEN", 200, std::string(DEVICE_B), 5}, {"CY", 100, "", 2}};
    data.device.run = std::vector<std::int64_t>{1, 2};
    data.device.run_player = "first";
    const SaveData before = data;

    REQUIRE(assign_device_id(data, std::string(DEVICE_A)).has_value());
    REQUIRE(data.device.id == DEVICE_A);
    REQUIRE(data.shared.players[0].coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 137, 0}});
    REQUIRE(data.shared.players[1].coins ==
            std::vector<CoinEntry>{{std::string(DEVICE_A), 4, 1}, {std::string(DEVICE_B), 9, 0}});
    REQUIRE(data.shared.players[2].coins.empty());
    REQUIRE(data.shared.top10 == std::vector<ScoreEntry>{{"ADA", 300, std::string(DEVICE_A), 1},
                                                         {"BEN", 200, std::string(DEVICE_B), 5},
                                                         {"CY", 100, std::string(DEVICE_A), 2}});
    // Nothing else moved.
    REQUIRE(data.device.run == before.device.run);
    REQUIRE(data.device.next_seq == before.device.next_seq);
    REQUIRE(player_wallet(data.shared.players[0]) == 137);
    REQUIRE(validate(data).has_value());

    SECTION("a second assignment is refused and changes nothing")
    {
        const SaveData assigned = data;
        REQUIRE(assign_device_id(data, std::string(DEVICE_B)) == std::unexpected(PlayerError::DEVICE_ID_SET));
        REQUIRE(data == assigned);
    }
}

TEST_CASE("the device id is refused when it is not an id or is already in use", "[unit][save][players]")
{
    SaveData data;
    data.shared.players[0].coins = {{"", 10, 0}};
    data.shared.top10 = {{"ADA", 300, "", 1}};
    const SaveData before = data;

    REQUIRE(assign_device_id(data, "") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(assign_device_id(data, "first") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(assign_device_id(data, "AAAAAAAAAAAAAAAA") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(assign_device_id(data, "aaaa") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(data == before);

    SECTION("on a coin entry")
    {
        data.shared.players[0].coins.push_back({std::string(DEVICE_B), 1, 0});
        const SaveData with_entry = data;
        REQUIRE(assign_device_id(data, std::string(DEVICE_B)) == std::unexpected(PlayerError::DEVICE_ID_IN_USE));
        REQUIRE(data == with_entry);
    }
    SECTION("on a top 10 entry")
    {
        data.shared.top10.push_back({"BEN", 100, std::string(DEVICE_B), 1});
        const SaveData with_entry = data;
        REQUIRE(assign_device_id(data, std::string(DEVICE_B)) == std::unexpected(PlayerError::DEVICE_ID_IN_USE));
        REQUIRE(data == with_entry);
    }
}

TEST_CASE("a current player is always made available", "[unit][save][players]")
{
    SECTION("a current player that is active is left alone")
    {
        SaveData data = family();
        data.device.current_player = std::string(PLAYER_2);
        const SaveData before = data;
        REQUIRE(ensure_current_player(data, std::string(PLAYER_4)).has_value());
        REQUIRE(data == before);
    }
    SECTION("one that names a removed player becomes the first active player")
    {
        SaveData data = family();
        data.device.current_player = std::string(PLAYER_3);
        REQUIRE(ensure_current_player(data, std::string(PLAYER_4)).has_value());
        REQUIRE(data.device.current_player == "first");
        REQUIRE(data.shared.players.size() == 3); // nobody added
    }
    SECTION("one that names nobody becomes the first active player in the list")
    {
        SaveData data = family();
        data.shared.players[0].removed = true;
        data.device.current_player = std::string(PLAYER_4);
        REQUIRE(ensure_current_player(data, std::string(PLAYER_5)).has_value());
        REQUIRE(data.device.current_player == PLAYER_2);
        REQUIRE(data.shared.players.size() == 3);
    }
    SECTION("with nobody active, an unnamed player is added with the id given")
    {
        SaveData data = family();
        data.shared.players[0].removed = true;
        data.shared.players[1].removed = true;
        REQUIRE(active_player_count(data.shared) == 0);
        REQUIRE(ensure_current_player(data, std::string(PLAYER_4)).has_value());
        REQUIRE(data.shared.players.size() == 4);
        REQUIRE(data.shared.players.back() == Player{.id = std::string(PLAYER_4)});
        REQUIRE(data.device.current_player == PLAYER_4);
        REQUIRE(current_player(data) == &data.shared.players.back());
        REQUIRE(validate(data).has_value());
    }
    SECTION("an empty list gets one too")
    {
        SaveData data;
        data.shared.players.clear();
        REQUIRE(ensure_current_player(data, std::string(PLAYER_2)).has_value());
        REQUIRE(data.shared.players == std::vector<Player>{Player{.id = std::string(PLAYER_2)}});
        REQUIRE(data.device.current_player == PLAYER_2);
    }
    SECTION("a bad id for the new player, or a full list, is refused and nothing changes")
    {
        SaveData data;
        data.shared.players.clear();
        data.device.current_player = "first";
        const SaveData before = data;
        REQUIRE(ensure_current_player(data, "nope") == std::unexpected(PlayerError::ID_INVALID));
        REQUIRE(data == before);

        SaveData crowded;
        crowded.shared.players.clear();
        for (std::size_t i = 0; i < MAX_SAVED_PLAYERS; ++i)
        {
            crowded.shared.players.push_back(Player{.id = hex_id(i + 1), .removed = true});
        }
        const SaveData crowded_before = crowded;
        REQUIRE(ensure_current_player(crowded, hex_id(1000)) == std::unexpected(PlayerError::LIST_FULL));
        REQUIRE(crowded == crowded_before);
    }
}

TEST_CASE("a qualifying score goes in the top 10 for this device and raises the best", "[unit][save][players]")
{
    SaveData data;
    REQUIRE(assign_device_id(data, std::string(DEVICE_A)).has_value());
    data.device.next_seq = 7;
    REQUIRE(add_player(data.shared, std::string(PLAYER_2)).has_value());

    REQUIRE(record_score(data, "first", "ADA", 500) == 1);
    REQUIRE(data.shared.top10 == std::vector<ScoreEntry>{{"ADA", 500, std::string(DEVICE_A), 7}});
    REQUIRE(data.device.next_seq == 8);
    REQUIRE(data.shared.players[0].best == 500);

    REQUIRE(record_score(data, PLAYER_2, "BEN", 900) == 1);
    REQUIRE(record_score(data, "first", "ADA", 500) == 3); // behind BEN's 900 and the older 500
    REQUIRE(record_score(data, "first", "ADA", 300) == 4);
    REQUIRE(data.shared.top10 == std::vector<ScoreEntry>{{"BEN", 900, std::string(DEVICE_A), 8},
                                                         {"ADA", 500, std::string(DEVICE_A), 7},
                                                         {"ADA", 500, std::string(DEVICE_A), 9},
                                                         {"ADA", 300, std::string(DEVICE_A), 10}});
    REQUIRE(data.device.next_seq == 11);
    REQUIRE(data.shared.players[0].best == 500); // a lower score does not lower it
    REQUIRE(data.shared.players[1].best == 900);
    REQUIRE(validate(data).has_value());
    REQUIRE(parse_save(serialise(data).value()).data == data);

    SECTION("before the app has assigned an id the entry carries the empty one, and assignment fills it in")
    {
        SaveData fresh;
        REQUIRE(record_score(fresh, "first", "ADA", 40) == 1);
        REQUIRE(fresh.shared.top10 == std::vector<ScoreEntry>{{"ADA", 40, "", 1}});
        REQUIRE(assign_device_id(fresh, std::string(DEVICE_B)).has_value());
        REQUIRE(fresh.shared.top10[0].device == DEVICE_B);
    }
    SECTION("the entry pushed off the end of a full list is gone, and the numbers still advance")
    {
        for (int i = 0; i < 10; ++i)
        {
            REQUIRE(record_score(data, "first", "X", 1000 + i).has_value());
        }
        REQUIRE(data.shared.top10.size() == static_cast<std::size_t>(Tuning::defaults().top10_size));
        REQUIRE(data.shared.top10.back().score == 1000);
        REQUIRE(data.device.next_seq == 21);
    }
}

TEST_CASE("a score that is refused changes nothing, not even the sequence number", "[unit][save][players]")
{
    SaveData data;
    REQUIRE(record_score(data, "first", "ADA", 500).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_2)).has_value());
    REQUIRE(remove_player(data.shared, PLAYER_2).has_value());
    const SaveData before = data;

    REQUIRE(record_score(data, "first", "ADA", 0) == std::unexpected(ScoreError::NOT_QUALIFYING));
    REQUIRE(record_score(data, "first", "ADA", -3) == std::unexpected(ScoreError::NOT_QUALIFYING));
    REQUIRE(record_score(data, "first", "ada", 600) == std::unexpected(ScoreError::NAME_INVALID));
    REQUIRE(record_score(data, "first", "", 600) == std::unexpected(ScoreError::NAME_INVALID));
    REQUIRE(record_score(data, PLAYER_3, "ADA", 600) == std::unexpected(ScoreError::PLAYER_NOT_FOUND));
    REQUIRE(record_score(data, PLAYER_2, "ADA", 600) == std::unexpected(ScoreError::PLAYER_NOT_FOUND)); // removed
    REQUIRE(record_score(data, "", "ADA", 600) == std::unexpected(ScoreError::PLAYER_NOT_FOUND));
    REQUIRE(data == before);

    SECTION("a full list that the score does not beat")
    {
        for (int i = 0; i < 9; ++i)
        {
            REQUIRE(record_score(data, "first", "X", 1000 + i).has_value());
        }
        const SaveData full = data;
        REQUIRE(record_score(data, "first", "LOW", 500) == std::unexpected(ScoreError::NOT_QUALIFYING));
        REQUIRE(data == full);
    }
    SECTION("a sequence number at its bound")
    {
        data.device.next_seq = MAX_SAVED_COUNTER;
        const SaveData at_bound = data;
        REQUIRE(record_score(data, "first", "ADA", 900) == std::unexpected(ScoreError::SEQ_EXHAUSTED));
        REQUIRE(data == at_bound);
        data.device.next_seq = MAX_SAVED_COUNTER - 1;
        REQUIRE(record_score(data, "first", "ADA", 900).has_value());
        REQUIRE(data.device.next_seq == MAX_SAVED_COUNTER);
        REQUIRE(validate(data).has_value());
    }
}

TEST_CASE("a best score only rises", "[unit][save][players]")
{
    Player player;
    record_best(player, 0);
    REQUIRE(player.best == 0);
    record_best(player, 300);
    REQUIRE(player.best == 300);
    record_best(player, 200);
    REQUIRE(player.best == 300);
    record_best(player, 300);
    REQUIRE(player.best == 300);
    record_best(player, -5);
    REQUIRE(player.best == 300);
}

TEST_CASE("a family's save survives text and the helpers together", "[unit][save][players]")
{
    // The first run, as the controller drives it: defaults, an id, a name, a score, coins, a second player.
    SaveData data;
    REQUIRE(assign_device_id(data, std::string(DEVICE_A)).has_value());
    REQUIRE(ensure_current_player(data, std::string(PLAYER_2)).has_value());
    Player* player = current_player(data);
    REQUIRE(player != nullptr);
    REQUIRE(record_wallet(*player, data.device.id, 120).has_value());
    REQUIRE(record_wallet(*player, data.device.id, 80).has_value());
    const std::string id = player->id; // the pointer is not kept across add_player
    REQUIRE(rename_player(data.shared, id, "ROCKET").has_value());
    REQUIRE(record_score(data, id, "ROCKET", 1640).has_value());
    REQUIRE(add_player(data.shared, std::string(PLAYER_3)).has_value());
    data.device.run = std::vector<std::int64_t>{1, 2, 3};
    data.device.run_player = id;

    const auto text = serialise(data);
    REQUIRE(text.has_value());
    const ParseResult back = parse_save(*text);
    REQUIRE(back.status == LoadStatus::OK);
    REQUIRE(back.data == data);
    REQUIRE(player_wallet(back.data.shared.players[0]) == 80);
    REQUIRE(back.data.shared.players[0].coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 120, 40}});
}

TEST_CASE("set_current_player picks an active player and refuses a missing or removed one", "[unit][save][players]")
{
    SaveData data = family(); // `first` (current), PLAYER_2, and PLAYER_3 removed
    REQUIRE(data.device.current_player == "first");

    REQUIRE(set_current_player(data, PLAYER_2).has_value());
    REQUIRE(data.device.current_player == PLAYER_2);
    REQUIRE(current_player(data) == find_player(data.shared, PLAYER_2));

    // Choosing the current player again is not an error.
    REQUIRE(set_current_player(data, PLAYER_2).has_value());
    REQUIRE(data.device.current_player == PLAYER_2);

    const SaveData before = data;
    const auto removed = set_current_player(data, PLAYER_3);
    REQUIRE_FALSE(removed.has_value());
    REQUIRE(removed.error() == PlayerError::NOT_FOUND);
    const auto missing = set_current_player(data, PLAYER_4);
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error() == PlayerError::NOT_FOUND);
    const auto empty = set_current_player(data, "");
    REQUIRE_FALSE(empty.has_value());
    REQUIRE(empty.error() == PlayerError::NOT_FOUND);
    REQUIRE(data == before);
}

TEST_CASE("set_current_player changes nothing but the choice", "[unit][save][players]")
{
    SaveData data = family();
    data.shared.players[0].best = 700;
    REQUIRE(record_wallet(data.shared.players[0], DEVICE_A, 30).has_value());
    data.device.run = std::vector<std::int64_t>{1, 2, 3};
    data.device.run_player = "first";
    SaveData expected = data;
    expected.device.current_player = std::string(PLAYER_2);

    REQUIRE(set_current_player(data, PLAYER_2).has_value());
    REQUIRE(data == expected);
}
