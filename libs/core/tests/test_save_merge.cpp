// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/save_merge.hpp"
#include "heart_boom_maths/tuning.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace heart_boom_maths;

namespace
{

constexpr std::string_view DEVICE_A = "aaaaaaaaaaaaaaaa";
constexpr std::string_view DEVICE_B = "bbbbbbbbbbbbbbbb";
constexpr std::string_view DEVICE_C = "cccccccccccccccc";
constexpr std::string_view PLAYER_2 = "0000000000000002";
constexpr std::string_view PLAYER_3 = "0000000000000003";
constexpr std::string_view PLAYER_4 = "0000000000000004";
constexpr std::string_view PLAYER_5 = "0000000000000005";

Player make_player(std::string_view id, std::string name = {}, std::int64_t name_rev = 0)
{
    return Player{.id = std::string(id), .name = std::move(name), .name_rev = name_rev};
}

ScoreEntry entry(std::string name, std::int64_t score, std::string_view device, std::int64_t seq)
{
    return ScoreEntry{std::move(name), score, std::string(device), seq};
}

SharedData shared_of(std::vector<Player> players, std::vector<ScoreEntry> top10 = {})
{
    return SharedData{std::move(players), std::move(top10)};
}

// The result of a merge that must succeed.
SharedData merged(const SharedData& a, const SharedData& b)
{
    const auto result = merge(a, b);
    REQUIRE(result.has_value());
    return *result;
}

// True when the SharedData can sit in a save, as the app would put it.
bool valid_in_save(const SharedData& shared)
{
    SaveData data;
    data.shared = shared;
    return validate(data).has_value();
}

// Two SharedData in one canonical form are equal when their data are.
using MergeResult = std::expected<SharedData, MergeError>;

MergeResult merge_three_left(const SharedData& a, const SharedData& b, const SharedData& c)
{
    return merge(a, b).and_then([&c](const SharedData& ab) { return merge(ab, c); });
}

MergeResult merge_three_right(const SharedData& a, const SharedData& b, const SharedData& c)
{
    return merge(b, c).and_then([&a](const SharedData& bc) { return merge(a, bc); });
}

// ---------------------------------------------------------------------------------------------------------------------
// Generated data. Both sides of a pair are drawn from one small universe of players, devices and top 10 identities, so
// they share history the way two devices of a family do: the same ids and devices turn up on both sides, and an entry
// with one identity usually has one score and one name.
// ---------------------------------------------------------------------------------------------------------------------

constexpr std::array<std::string_view, 4> NAMES = {"ADA", "BEN", "CY", "DOT E"};

std::vector<std::string> player_ids()
{
    return {"first", hex_id(2), hex_id(3), hex_id(4), hex_id(5), hex_id(0xFFFFFFFFFFFFFFFFULL)};
}

std::vector<std::string> device_ids()
{
    return {"", std::string(DEVICE_A), std::string(DEVICE_B), std::string(DEVICE_C)};
}

bool chance(Rng& rng, std::uint64_t one_in)
{
    return rng.below(one_in) == 0;
}

std::string pick_name(Rng& rng)
{
    return std::string(NAMES[rng.below(NAMES.size())]);
}

Player generate_player(Rng& rng, const std::string& id)
{
    Player player;
    player.id = id;
    player.name_rev = static_cast<std::int64_t>(rng.below(4));
    player.name = player.name_rev == 0 && !chance(rng, 4) ? std::string() : pick_name(rng);
    player.removed = chance(rng, 5);
    player.won_once = chance(rng, 3);
    player.best = static_cast<std::int64_t>(rng.below(6));
    for (const std::string& device : device_ids())
    {
        if (chance(rng, 2))
        {
            player.coins.push_back(
                CoinEntry{device, static_cast<std::int64_t>(rng.below(41)), static_cast<std::int64_t>(rng.below(41))});
        }
    }
    rng.shuffle(std::span<CoinEntry>(player.coins));
    return player;
}

SharedData generate_shared(Rng& rng)
{
    SharedData data;
    data.players.clear();
    for (const std::string& id : player_ids())
    {
        if (!chance(rng, 4))
        {
            data.players.push_back(generate_player(rng, id));
        }
    }
    rng.shuffle(std::span<Player>(data.players));

    std::vector<ScoreEntry> pool;
    const std::vector<std::string> devices = device_ids();
    for (std::size_t d = 0; d < devices.size(); ++d)
    {
        for (std::int64_t seq = 1; seq <= 5; ++seq)
        {
            if (!chance(rng, 3))
            {
                continue;
            }
            // The history of an identity: a score and a name that follow from it. Now and then a copy has gone wrong.
            const auto base = static_cast<std::int64_t>(1 + (d * 7 + static_cast<std::size_t>(seq) * 3) % 9);
            ScoreEntry item{std::string(NAMES[(d + static_cast<std::size_t>(seq)) % NAMES.size()]), base, devices[d],
                            seq};
            if (chance(rng, 8))
            {
                item.score = static_cast<std::int64_t>(1 + rng.below(9));
                item.name = pick_name(rng);
            }
            pool.push_back(std::move(item));
        }
    }
    rng.shuffle(std::span<ScoreEntry>(pool));
    if (pool.size() > static_cast<std::size_t>(Tuning::defaults().top10_size))
    {
        pool.resize(static_cast<std::size_t>(Tuning::defaults().top10_size));
    }
    // Valid data is in descending score, and the order of equal scores is left as the shuffle put it.
    std::ranges::stable_sort(pool, [](const ScoreEntry& l, const ScoreEntry& r) { return l.score > r.score; });
    data.top10 = std::move(pool);
    return data;
}

const CoinEntry* find_coin(const Player& player, std::string_view device)
{
    const auto found = std::ranges::find_if(player.coins, [device](const CoinEntry& c) { return c.device == device; });
    return found == player.coins.end() ? nullptr : &*found;
}

// Nothing a side had is below what the result has: counters, the marks and the players themselves.
void require_nothing_lost(const SharedData& side, const SharedData& result)
{
    for (const Player& old : side.players)
    {
        const Player* now = find_player(result, old.id);
        REQUIRE(now != nullptr);
        REQUIRE(now->name_rev >= old.name_rev);
        REQUIRE(now->best >= old.best);
        REQUIRE((now->removed || !old.removed));
        REQUIRE((now->won_once || !old.won_once));
        for (const CoinEntry& coin : old.coins)
        {
            const CoinEntry* kept = find_coin(*now, coin.device);
            REQUIRE(kept != nullptr);
            REQUIRE(kept->earned >= coin.earned);
            REQUIRE(kept->spent >= coin.spent);
        }
    }
    const auto limit = static_cast<std::size_t>(Tuning::defaults().top10_size);
    for (const ScoreEntry& old : side.top10)
    {
        const auto kept = std::ranges::find_if(result.top10, [&old](const ScoreEntry& e)
                                               { return e.device == old.device && e.seq == old.seq; });
        if (kept != result.top10.end())
        {
            REQUIRE(kept->score >= old.score);
        }
        else
        {
            // Only an entry that the best ten no longer reach may be missing.
            REQUIRE(result.top10.size() == limit);
            REQUIRE(result.top10.back().score >= old.score);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Canonical form
// ---------------------------------------------------------------------------------------------------------------------

TEST_CASE("canonical puts `first` first and the other players by ascending id", "[unit][save][merge]")
{
    const SharedData shared = shared_of({make_player(PLAYER_4), make_player("first"), make_player("ffffffffffffffff"),
                                         make_player(PLAYER_2), make_player("a000000000000000")});
    const SharedData result = canonical(shared);
    REQUIRE(result.players.size() == 5);
    REQUIRE(result.players[0].id == "first"); // not after "ffffffffffffffff", which `first` would sort below
    REQUIRE(result.players[1].id == PLAYER_2);
    REQUIRE(result.players[2].id == PLAYER_4);
    REQUIRE(result.players[3].id == "a000000000000000");
    REQUIRE(result.players[4].id == "ffffffffffffffff");

    REQUIRE(player_id_precedes("first", PLAYER_2));
    REQUIRE_FALSE(player_id_precedes(PLAYER_2, "first"));
    REQUIRE_FALSE(player_id_precedes("first", "first"));
    REQUIRE(player_id_precedes(PLAYER_2, PLAYER_3));
    REQUIRE_FALSE(player_id_precedes(PLAYER_3, PLAYER_3));
}

TEST_CASE("canonical orders each player's coin entries by ascending device", "[unit][save][merge]")
{
    Player player = make_player("first");
    player.coins = {{std::string(DEVICE_C), 3, 0}, {"", 1, 0}, {std::string(DEVICE_A), 2, 1}};
    const SharedData result = canonical(shared_of({player}));
    REQUIRE(result.players[0].coins.size() == 3);
    REQUIRE(result.players[0].coins[0].device.empty());
    REQUIRE(result.players[0].coins[1].device == DEVICE_A);
    REQUIRE(result.players[0].coins[2].device == DEVICE_C);
}

TEST_CASE("canonical orders the top 10 by score, then device, then seq, and cuts it to ten", "[unit][save][merge]")
{
    SharedData shared = shared_of({make_player("first")});
    shared.top10 = {entry("LOW", 5, DEVICE_A, 1), entry("TIE", 9, DEVICE_B, 1), entry("TIE", 9, DEVICE_A, 2),
                    entry("TIE", 9, DEVICE_A, 1), entry("TOP", 20, DEVICE_C, 7)};
    const SharedData result = canonical(shared);
    REQUIRE(result.top10[0] == entry("TOP", 20, DEVICE_C, 7));
    REQUIRE(result.top10[1] == entry("TIE", 9, DEVICE_A, 1));
    REQUIRE(result.top10[2] == entry("TIE", 9, DEVICE_A, 2));
    REQUIRE(result.top10[3] == entry("TIE", 9, DEVICE_B, 1));
    REQUIRE(result.top10[4] == entry("LOW", 5, DEVICE_A, 1));

    SECTION("twelve entries are cut to the best ten")
    {
        SharedData long_list = shared_of({make_player("first")});
        for (std::int64_t i = 1; i <= 12; ++i)
        {
            long_list.top10.push_back(entry("X", i, DEVICE_A, i));
        }
        const SharedData cut = canonical(long_list);
        REQUIRE(cut.top10.size() == 10);
        REQUIRE(cut.top10.front().score == 12);
        REQUIRE(cut.top10.back().score == 3);
    }
    SECTION("a smaller top10_size in the tuning is taken")
    {
        Tuning tuning = Tuning::defaults();
        tuning.top10_size = 2;
        REQUIRE(canonical(shared, tuning).top10.size() == 2);
    }
}

TEST_CASE("canonical form is what canonical gives again", "[unit][save][merge]")
{
    Rng rng(0xC0DE);
    for (int i = 0; i < 100; ++i)
    {
        const SharedData once = canonical(generate_shared(rng));
        REQUIRE(canonical(once) == once);
    }
}

TEST_CASE("add_player puts a new player at its canonical place", "[unit][save][merge]")
{
    SharedData shared = shared_of({make_player("first"), make_player(PLAYER_3), make_player("a000000000000000")});
    Tuning roomy = Tuning::defaults();
    roomy.max_players = 9;
    REQUIRE(add_player(shared, std::string(PLAYER_4), roomy).has_value());
    REQUIRE(add_player(shared, std::string(PLAYER_2), roomy).has_value());
    REQUIRE(add_player(shared, "ffffffffffffffff", roomy).has_value());
    std::vector<std::string> ids;
    for (const Player& player : shared.players)
    {
        ids.push_back(player.id);
    }
    REQUIRE(ids == std::vector<std::string>{"first", std::string(PLAYER_2), std::string(PLAYER_3),
                                            std::string(PLAYER_4), "a000000000000000", "ffffffffffffffff"});
    REQUIRE(canonical(shared).players == shared.players);

    SECTION("a list that has no `first` takes the new player by id")
    {
        SharedData without = shared_of({make_player(PLAYER_3)});
        REQUIRE(add_player(without, std::string(PLAYER_2)).has_value());
        REQUIRE(without.players.front().id == PLAYER_2);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Merge rules
// ---------------------------------------------------------------------------------------------------------------------

TEST_CASE("a player only on one side is kept, whichever side", "[unit][save][merge]")
{
    const SharedData a = shared_of({make_player("first", "ADA", 1)});
    const SharedData b = shared_of({make_player("first", "ADA", 1), make_player(PLAYER_2, "BEN", 1)});
    const SharedData both = shared_of({make_player("first", "ADA", 1), make_player(PLAYER_2, "BEN", 1)});
    REQUIRE(merged(a, b) == both);
    REQUIRE(merged(b, a) == both);
    REQUIRE(merged(a, SharedData{.players = {}}) == a);
    REQUIRE(merged(SharedData{.players = {}}, SharedData{.players = {}}) == SharedData{.players = {}});
}

TEST_CASE("the higher name_rev wins the name", "[unit][save][merge]")
{
    const SharedData a = shared_of({make_player("first", "ADA", 1)});
    const SharedData b = shared_of({make_player("first", "ADELE", 2)}); // renamed on the other device
    for (const SharedData& result : {merged(a, b), merged(b, a)})
    {
        REQUIRE(result.players.size() == 1);
        REQUIRE(result.players[0].name == "ADELE");
        REQUIRE(result.players[0].name_rev == 2);
    }

    SECTION("a name against none: the named side has the higher counter")
    {
        const SharedData unnamed = shared_of({make_player("first")});
        REQUIRE(merged(unnamed, a).players[0].name == "ADA");
        REQUIRE(merged(a, unnamed).players[0].name_rev == 1);
    }
}

TEST_CASE("on equal name_rev the name that sorts first wins", "[unit][save][merge]")
{
    const SharedData a = shared_of({make_player("first", "BEN", 3)});
    const SharedData b = shared_of({make_player("first", "ADA", 3)});
    REQUIRE(merged(a, b).players[0].name == "ADA");
    REQUIRE(merged(b, a).players[0].name == "ADA");
    REQUIRE(merged(a, b).players[0].name_rev == 3);
    // A space sorts before a letter.
    const SharedData c = shared_of({make_player("first", "AD A", 3)});
    REQUIRE(merged(b, c).players[0].name == "AD A");
}

TEST_CASE("a removal on one side survives the merge, and the other fields still merge", "[unit][save][merge]")
{
    Player removed = make_player(PLAYER_2, "BEN", 1);
    removed.removed = true;
    removed.best = 4;
    removed.coins = {{std::string(DEVICE_A), 10, 2}};
    Player active = make_player(PLAYER_2, "BENNY", 2); // renamed after the other device removed him
    active.won_once = true;
    active.best = 9;
    active.coins = {{std::string(DEVICE_B), 5, 0}};

    const SharedData a = shared_of({make_player("first"), removed});
    const SharedData b = shared_of({make_player("first"), active});
    for (const SharedData& result : {merged(a, b), merged(b, a)})
    {
        const Player* player = find_player(result, PLAYER_2);
        REQUIRE(player != nullptr);
        REQUIRE(player->removed);
        REQUIRE(player->name == "BENNY");
        REQUIRE(player->won_once);
        REQUIRE(player->best == 9);
        REQUIRE(player->coins.size() == 2);
        REQUIRE(player_wallet(*player) == 10 - 2 + 5);
    }
    REQUIRE(active_player_count(merged(a, b)) == 1);
}

TEST_CASE("won_once is set if either side has it, and best is the larger", "[unit][save][merge]")
{
    Player a_player = make_player("first");
    a_player.won_once = true;
    a_player.best = 100;
    Player b_player = make_player("first");
    b_player.best = 250;
    const SharedData a = shared_of({a_player});
    const SharedData b = shared_of({b_player});
    for (const SharedData& result : {merged(a, b), merged(b, a)})
    {
        REQUIRE(result.players[0].won_once);
        REQUIRE(result.players[0].best == 250);
    }
}

TEST_CASE("coins earned on two devices while apart add up in the wallet", "[unit][save][merge]")
{
    Player start = make_player("first");
    start.coins = {{std::string(DEVICE_A), 100, 0}};
    Player on_a = start;
    on_a.coins = {{std::string(DEVICE_A), 130, 0}}; // earned 30 more here
    Player on_b = start;
    on_b.coins = {{std::string(DEVICE_A), 100, 0}, {std::string(DEVICE_B), 45, 0}}; // and 45 there
    const SharedData result = merged(shared_of({on_a}), shared_of({on_b}));
    REQUIRE(player_wallet(result.players[0]) == 175);
    REQUIRE(result.players[0].coins.size() == 2);
    REQUIRE(result.players[0].coins[0] == CoinEntry{std::string(DEVICE_A), 130, 0});
    REQUIRE(result.players[0].coins[1] == CoinEntry{std::string(DEVICE_B), 45, 0});
}

TEST_CASE("a spend on one device and an earn on the other both count", "[unit][save][merge]")
{
    Player start = make_player("first");
    start.coins = {{std::string(DEVICE_A), 100, 0}, {std::string(DEVICE_B), 0, 0}};
    Player spent_on_a = start;
    spent_on_a.coins[0].spent = 60;
    Player earned_on_b = start;
    earned_on_b.coins[1].earned = 25;
    const SharedData result = merged(shared_of({spent_on_a}), shared_of({earned_on_b}));
    REQUIRE(player_wallet(result.players[0]) == 100 - 60 + 25);
    REQUIRE(result.players[0].coins[0] == CoinEntry{std::string(DEVICE_A), 100, 60});
    REQUIRE(result.players[0].coins[1] == CoinEntry{std::string(DEVICE_B), 25, 0});
}

TEST_CASE("each counter of a device takes the larger of the two sides, and merging the same data adds nothing",
          "[unit][save][merge]")
{
    Player a_player = make_player("first");
    a_player.coins = {{std::string(DEVICE_A), 50, 10}};
    Player b_player = make_player("first");
    b_player.coins = {{std::string(DEVICE_A), 40, 20}};
    const SharedData result = merged(shared_of({a_player}), shared_of({b_player}));
    REQUIRE(result.players[0].coins == std::vector<CoinEntry>{{std::string(DEVICE_A), 50, 20}});
    REQUIRE(merged(result, result) == result);
}

TEST_CASE("the same top 10 entry on both sides appears once", "[unit][save][merge]")
{
    const SharedData a =
        shared_of({make_player("first")}, {entry("ADA", 90, DEVICE_A, 1), entry("BEN", 50, DEVICE_A, 2)});
    const SharedData b =
        shared_of({make_player("first")}, {entry("ADA", 90, DEVICE_A, 1), entry("CY", 70, DEVICE_B, 1)});
    const SharedData result = merged(a, b);
    REQUIRE(result.top10 == std::vector<ScoreEntry>{entry("ADA", 90, DEVICE_A, 1), entry("CY", 70, DEVICE_B, 1),
                                                    entry("BEN", 50, DEVICE_A, 2)});
    REQUIRE(merged(b, a) == result);
}

TEST_CASE("equal scores are ordered by device and then seq", "[unit][save][merge]")
{
    const SharedData a = shared_of({make_player("first")}, {entry("X", 40, DEVICE_B, 1), entry("Y", 40, DEVICE_A, 3)});
    const SharedData b = shared_of({make_player("first")}, {entry("Z", 40, DEVICE_A, 2), entry("W", 40, "", 9)});
    const SharedData result = merged(a, b);
    REQUIRE(result.top10 == std::vector<ScoreEntry>{entry("W", 40, "", 9), entry("Z", 40, DEVICE_A, 2),
                                                    entry("Y", 40, DEVICE_A, 3), entry("X", 40, DEVICE_B, 1)});
}

TEST_CASE("the top 10 is cut to ten and the entries cut are the lowest", "[unit][save][merge]")
{
    SharedData a = shared_of({make_player("first")});
    SharedData b = shared_of({make_player("first")});
    for (std::int64_t i = 1; i <= 7; ++i)
    {
        a.top10.push_back(entry("A", 100 - i * 2, DEVICE_A, i));     // 98 to 86
        b.top10.push_back(entry("B", 100 - i * 2 - 1, DEVICE_B, i)); // 97 to 85
    }
    std::ranges::stable_sort(a.top10, [](const ScoreEntry& l, const ScoreEntry& r) { return l.score > r.score; });
    const SharedData result = merged(a, b);
    REQUIRE(result.top10.size() == 10);
    REQUIRE(result.top10.front().score == 98);
    REQUIRE(result.top10.back().score == 89);
    REQUIRE(valid_in_save(result));

    SECTION("an entry on both sides that would be cut is still cut once, not twice")
    {
        REQUIRE(merged(result, a) == result);
        REQUIRE(merged(result, b) == result);
    }
}

TEST_CASE("two entries of one identity that differ keep the higher score, then the name that sorts first",
          "[unit][save][merge]")
{
    const SharedData low = shared_of({make_player("first")}, {entry("ADA", 50, DEVICE_A, 1)});
    const SharedData high = shared_of({make_player("first")}, {entry("BEN", 60, DEVICE_A, 1)});
    REQUIRE(merged(low, high).top10 == std::vector<ScoreEntry>{entry("BEN", 60, DEVICE_A, 1)});
    REQUIRE(merged(high, low).top10 == std::vector<ScoreEntry>{entry("BEN", 60, DEVICE_A, 1)});

    const SharedData same_b = shared_of({make_player("first")}, {entry("BEN", 60, DEVICE_A, 1)});
    const SharedData same_a = shared_of({make_player("first")}, {entry("ADA", 60, DEVICE_A, 1)});
    REQUIRE(merged(same_b, same_a).top10 == std::vector<ScoreEntry>{entry("ADA", 60, DEVICE_A, 1)});
    REQUIRE(merged(same_a, same_b).top10 == std::vector<ScoreEntry>{entry("ADA", 60, DEVICE_A, 1)});

    SECTION("one side that holds the identity twice is folded too")
    {
        SharedData twice = shared_of({make_player("first")});
        twice.top10 = {entry("BEN", 60, DEVICE_A, 1), entry("ADA", 60, DEVICE_A, 1)};
        REQUIRE(merged(twice, twice).top10 == std::vector<ScoreEntry>{entry("ADA", 60, DEVICE_A, 1)});
    }
}

TEST_CASE("more active players than max_players are all kept", "[unit][save][merge]")
{
    SharedData a = shared_of({make_player("first")});
    SharedData b = shared_of({make_player("first")});
    for (std::uint64_t i = 2; i <= 4; ++i)
    {
        a.players.push_back(make_player(hex_id(i)));
    }
    for (std::uint64_t i = 5; i <= 7; ++i)
    {
        b.players.push_back(make_player(hex_id(i)));
    }
    REQUIRE(active_player_count(a) == 4);
    REQUIRE(active_player_count(b) == 4);
    const SharedData result = merged(a, b);
    REQUIRE(active_player_count(result) == 7);
    REQUIRE(valid_in_save(result));
}

TEST_CASE("a merge that would break a format bound is an error, and nothing is cut", "[unit][save][merge]")
{
    SECTION("the list of players")
    {
        SharedData a = shared_of({});
        SharedData b = shared_of({});
        for (std::uint64_t i = 1; i <= 40; ++i)
        {
            a.players.push_back(make_player(hex_id(i)));
            b.players.push_back(make_player(hex_id(100 + i)));
        }
        REQUIRE(merge(a, b) == std::unexpected(MergeError::PLAYERS_TOO_MANY));
        REQUIRE(merge(b, a) == std::unexpected(MergeError::PLAYERS_TOO_MANY));
        REQUIRE(a.players.size() == 40); // the inputs are as they were

        // The error does not depend on the grouping either.
        const SharedData nothing = shared_of({});
        REQUIRE(merge_three_left(a, b, nothing) == std::unexpected(MergeError::PLAYERS_TOO_MANY));
        REQUIRE(merge_three_right(a, b, nothing) == std::unexpected(MergeError::PLAYERS_TOO_MANY));
        REQUIRE(merge_three_left(nothing, a, b) == std::unexpected(MergeError::PLAYERS_TOO_MANY));
        REQUIRE(merge_three_right(nothing, a, b) == std::unexpected(MergeError::PLAYERS_TOO_MANY));

        // Exactly the bound is fine.
        SharedData c = shared_of({});
        for (std::uint64_t i = 1; i <= 24; ++i)
        {
            c.players.push_back(make_player(hex_id(1000 + i)));
        }
        const auto at_bound = merge(a, c);
        REQUIRE(at_bound.has_value());
        REQUIRE(at_bound->players.size() == MAX_SAVED_PLAYERS);
        REQUIRE(valid_in_save(*at_bound));
        REQUIRE(merge(*at_bound, c) == at_bound); // the same players again are not new
    }
    SECTION("the coin entries of one player")
    {
        Player a_player = make_player("first");
        Player b_player = make_player("first");
        for (std::uint64_t i = 1; i <= 40; ++i)
        {
            a_player.coins.push_back({hex_id(i), 1, 0});
            b_player.coins.push_back({hex_id(100 + i), 1, 0});
        }
        const SharedData a = shared_of({a_player});
        const SharedData b = shared_of({b_player});
        REQUIRE(merge(a, b) == std::unexpected(MergeError::COIN_ENTRIES_TOO_MANY));
        REQUIRE(merge(b, a) == std::unexpected(MergeError::COIN_ENTRIES_TOO_MANY));

        // 64 devices in all is the bound, and a device on both sides counts once.
        Player c_player = make_player("first");
        for (std::uint64_t i = 17; i <= 40; ++i)
        {
            c_player.coins.push_back({hex_id(i), 1, 0});
        }
        for (std::uint64_t i = 141; i <= 140 + 24; ++i)
        {
            c_player.coins.push_back({hex_id(i), 1, 0});
        }
        const auto at_bound = merge(a, shared_of({c_player}));
        REQUIRE(at_bound.has_value());
        REQUIRE(at_bound->players[0].coins.size() == 40 + 24);
        REQUIRE(valid_in_save(*at_bound));
    }
}

TEST_CASE("a merged result is in canonical form and passes validate", "[unit][save][merge]")
{
    SharedData a = shared_of({make_player(PLAYER_4, "DOT", 1), make_player("first", "ADA", 1)},
                             {entry("A", 5, DEVICE_B, 1), entry("B", 5, DEVICE_A, 1)});
    SharedData b = shared_of({make_player(PLAYER_2, "BEN", 1)}, {entry("C", 8, DEVICE_C, 1)});
    a.players[0].coins = {{std::string(DEVICE_B), 1, 0}, {std::string(DEVICE_A), 2, 0}};
    const SharedData result = merged(a, b);
    REQUIRE(result == canonical(result));
    REQUIRE(result.players[0].id == "first");
    REQUIRE(result.players[2].coins[0].device == DEVICE_A);
    REQUIRE(valid_in_save(result));
}

// ---------------------------------------------------------------------------------------------------------------------
// Properties of the merge, over generated data
// ---------------------------------------------------------------------------------------------------------------------

TEST_CASE("the merge is commutative", "[property][save][merge]")
{
    for (std::uint64_t seed = 1; seed <= 400; ++seed)
    {
        CAPTURE(seed);
        Rng rng(seed * 0x9E3779B97F4A7C15ULL);
        const SharedData a = generate_shared(rng);
        const SharedData b = generate_shared(rng);
        REQUIRE(merge(a, b) == merge(b, a));
    }
}

TEST_CASE("the merge is associative", "[property][save][merge]")
{
    for (std::uint64_t seed = 1; seed <= 400; ++seed)
    {
        CAPTURE(seed);
        Rng rng(seed * 0xD1B54A32D192ED03ULL);
        const SharedData a = generate_shared(rng);
        const SharedData b = generate_shared(rng);
        const SharedData c = generate_shared(rng);
        const MergeResult left = merge_three_left(a, b, c);
        REQUIRE(left.has_value());
        REQUIRE(left == merge_three_right(a, b, c));
        REQUIRE(left == merge_three_left(c, a, b)); // and any order of the three
        REQUIRE(left == merge_three_right(b, c, a));
    }
}

TEST_CASE("merging with itself gives the canonical form, and merging again changes nothing", "[property][save][merge]")
{
    for (std::uint64_t seed = 1; seed <= 400; ++seed)
    {
        CAPTURE(seed);
        Rng rng(seed * 0xA24BAED4963EE407ULL);
        const SharedData a = generate_shared(rng);
        const SharedData b = generate_shared(rng);
        REQUIRE(merge(a, a) == MergeResult(canonical(a)));
        const MergeResult once = merge(a, b);
        REQUIRE(once.has_value());
        REQUIRE(merge(*once, b) == once);
        REQUIRE(merge(*once, a) == once);
        REQUIRE(merge(a, *once) == once);
        REQUIRE(merge(*once, *once) == once);
    }
}

TEST_CASE("a merge loses nothing and keeps the format valid", "[property][save][merge]")
{
    for (std::uint64_t seed = 1; seed <= 400; ++seed)
    {
        CAPTURE(seed);
        Rng rng(seed * 0x2545F4914F6CDD1DULL);
        const SharedData a = generate_shared(rng);
        const SharedData b = generate_shared(rng);
        REQUIRE(valid_in_save(a));
        REQUIRE(valid_in_save(b));
        const MergeResult result = merge(a, b);
        REQUIRE(result.has_value());
        REQUIRE(*result == canonical(*result));
        require_nothing_lost(a, *result);
        require_nothing_lost(b, *result);
        REQUIRE(valid_in_save(*result));

        // The wallet is what the two sides earned and spent, and never below 0.
        for (const Player& player : result->players)
        {
            REQUIRE(player_wallet(player) >= 0);
        }
        // No player is made up: every one came from a side.
        for (const Player& player : result->players)
        {
            REQUIRE((find_player(a, player.id) != nullptr || find_player(b, player.id) != nullptr));
        }
    }
}

TEST_CASE("the merge of valid data is the same whatever the order of the lists", "[property][save][merge]")
{
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        CAPTURE(seed);
        Rng rng(seed + 77);
        const SharedData a = generate_shared(rng);
        SharedData shuffled = a;
        rng.shuffle(std::span<Player>(shuffled.players));
        for (Player& player : shuffled.players)
        {
            rng.shuffle(std::span<CoinEntry>(player.coins));
        }
        // Ties among equal scores may be listed either way in valid data; only the score order must hold.
        std::ranges::stable_sort(shuffled.top10,
                                 [](const ScoreEntry& l, const ScoreEntry& r) { return l.score > r.score; });
        REQUIRE(merge(a, a) == merge(shuffled, shuffled));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The shared payload
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

SharedData rich_shared()
{
    SharedData data = shared_of(
        {make_player("first", "ROCKET", 2), make_player(PLAYER_2, "BEN", 1), make_player(PLAYER_3)},
        {entry("ROCKET", 900, DEVICE_A, 4), entry("BEN", 500, DEVICE_B, 1), entry("ROCKET", 500, DEVICE_A, 2)});
    data.players[0].coins = {{std::string(DEVICE_A), 120, 30}, {std::string(DEVICE_B), 5, 0}};
    data.players[0].won_once = true;
    data.players[0].best = 900;
    data.players[1].removed = true;
    return data;
}

std::string shared_member_of(const std::string& save_text)
{
    // `serialise` writes the members of a save in key order: device, shared, version.
    constexpr std::string_view KEY = "\"shared\":";
    constexpr std::string_view TAIL = ",\"version\":2}";
    const auto from = save_text.find(KEY);
    REQUIRE(from != std::string::npos);
    REQUIRE(save_text.ends_with(TAIL));
    return save_text.substr(from + KEY.size(), save_text.size() - TAIL.size() - from - KEY.size());
}

} // namespace

TEST_CASE("a shared payload reads back as the data it was made from", "[unit][save][merge]")
{
    const SharedData data = rich_shared();
    const auto text = serialise_shared(data);
    REQUIRE(text.has_value());
    const SharedParseResult result = parse_shared(*text);
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE_FALSE(result.damage.has_value());
    REQUIRE(result.file_version == 2);
    REQUIRE(result.data == data);

    SECTION("the empty shared part and the default one")
    {
        for (const SharedData& other : {SharedData{}, SharedData{.players = {}}})
        {
            const auto empty = serialise_shared(other);
            REQUIRE(empty.has_value());
            REQUIRE(parse_shared(*empty).data == other);
        }
    }
    SECTION("the same data gives the same text")
    {
        REQUIRE(serialise_shared(data) == text);
    }
}

TEST_CASE("the payload is an envelope round the very object a save holds", "[unit][save][merge]")
{
    const SharedData data = rich_shared();
    SaveData save;
    save.shared = data;
    save.device.id = std::string(DEVICE_A);
    save.device.run = std::vector<std::int64_t>{1, 2, 3};
    save.device.run_player = "first";
    const auto save_text = serialise(save);
    const auto payload = serialise_shared(data);
    REQUIRE(save_text.has_value());
    REQUIRE(payload.has_value());

    const std::string member = shared_member_of(*save_text);
    REQUIRE(*payload == "{\"shared\":" + member + ",\"version\":2}");
    // Nothing of the device is in it.
    for (const std::string_view key :
         {"\"run\"", "\"run_player\"", "\"cloud_sync\"", "\"current_player\"", "\"next_seq\"", "\"sound\""})
    {
        REQUIRE(payload->find(key) == std::string::npos);
    }
}

TEST_CASE("a shared part that breaks a rule of the format is not written", "[unit][save][merge]")
{
    SharedData data = rich_shared();
    data.top10[0].score = 1; // now below the entries after it
    REQUIRE(serialise_shared(data) == std::unexpected(SaveError::TOP10_NOT_SORTED));

    data = rich_shared();
    data.players[2].id = "NOT AN ID";
    REQUIRE(serialise_shared(data) == std::unexpected(SaveError::PLAYER_ID_INVALID));

    data = rich_shared();
    data.players.push_back(data.players[0]);
    REQUIRE(serialise_shared(data) == std::unexpected(SaveError::PLAYER_ID_DUPLICATE));

    data = rich_shared();
    data.top10[1].name = "lower case";
    REQUIRE(serialise_shared(data) == std::unexpected(SaveError::NAME_INVALID));

    data = rich_shared();
    for (int i = 0; i < 20; ++i)
    {
        data.top10.push_back(entry("LOW", 1, DEVICE_C, 100 + i));
    }
    REQUIRE(serialise_shared(data) == std::unexpected(SaveError::TOP10_TOO_LONG));
}

TEST_CASE("a payload that cannot be read is damaged", "[unit][save][merge]")
{
    const std::string good = serialise_shared(rich_shared()).value();
    const auto damaged = [](std::string_view text, SaveError reason)
    {
        const SharedParseResult result = parse_shared(text);
        CAPTURE(text);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        REQUIRE(result.damage == reason);
        REQUIRE(result.data == SharedData{});
    };
    damaged("", SaveError::NOT_JSON);
    damaged("not json", SaveError::NOT_JSON);
    damaged("[]", SaveError::NOT_AN_OBJECT);
    damaged("{}", SaveError::MISSING_FIELD);
    damaged("{\"version\":\"2\"}", SaveError::WRONG_TYPE);
    damaged("{\"version\":2}", SaveError::MISSING_FIELD);
    damaged("{\"version\":2,\"shared\":[]}", SaveError::WRONG_TYPE);
    damaged("{\"version\":2,\"shared\":{}}", SaveError::MISSING_FIELD);
    damaged("{\"version\":2,\"shared\":{\"players\":[],\"top10\":5}}", SaveError::WRONG_TYPE);
    damaged("{\"version\":2.5,\"shared\":{\"players\":[],\"top10\":[]}}", SaveError::NOT_JSON);
    damaged(good + "x", SaveError::NOT_JSON);

    SECTION("a version below 2 does not exist as a payload")
    {
        damaged("{\"version\":1,\"shared\":{\"players\":[],\"top10\":[]}}", SaveError::BAD_VERSION);
        damaged("{\"version\":0,\"shared\":{\"players\":[],\"top10\":[]}}", SaveError::BAD_VERSION);
        damaged("{\"version\":-5,\"shared\":{\"players\":[],\"top10\":[]}}", SaveError::BAD_VERSION);
    }
    SECTION("data that fails validation is damaged, not partly read")
    {
        std::string text = good;
        const auto at = text.find("\"best\":900");
        REQUIRE(at != std::string::npos);
        text.replace(at, std::string_view("\"best\":900").size(), "\"best\":-1");
        damaged(text, SaveError::COUNTER_OUT_OF_RANGE);

        damaged("{\"shared\":{\"players\":[{\"best\":0,\"coins\":[],\"id\":\"zz\",\"name\":\"\",\"name_rev\":0,"
                "\"removed\":false,\"won_once\":false}],\"top10\":[]},\"version\":2}",
                SaveError::PLAYER_ID_INVALID);
        damaged("{\"shared\":{\"players\":[],\"top10\":[{\"device\":\"\",\"name\":\"A\",\"score\":0,\"seq\":1}]},"
                "\"version\":2}",
                SaveError::SCORE_INVALID);
    }
    SECTION("every prefix of a payload is damaged")
    {
        for (std::size_t length = 0; length < good.size(); ++length)
        {
            REQUIRE(parse_shared(std::string_view(good).substr(0, length)).status == LoadStatus::DAMAGED);
        }
        REQUIRE(parse_shared(good).status == LoadStatus::OK);
    }
}

TEST_CASE("a payload from a later build is newer and is not read", "[unit][save][merge]")
{
    const SharedParseResult plain = parse_shared("{\"version\":3,\"shared\":{\"players\":[],\"top10\":[]}}");
    REQUIRE(plain.status == LoadStatus::NEWER);
    REQUIRE(plain.file_version == 3);
    REQUIRE(plain.data == SharedData{});
    REQUIRE_FALSE(plain.damage.has_value());

    // Newer is decided by the number alone: a later build may have changed everything else.
    REQUIRE(parse_shared("{\"version\":99,\"shared\":5}").status == LoadStatus::NEWER);
    REQUIRE(parse_shared("{\"version\":9223372036854775807}").status == LoadStatus::NEWER);

    SECTION("a payload this build wrote under a higher schema number")
    {
        const SaveSchema later{3, {}};
        const auto text = serialise_shared(rich_shared(), Tuning::defaults(), later);
        REQUIRE(text.has_value());
        REQUIRE(parse_shared(*text).status == LoadStatus::NEWER);
        REQUIRE(parse_shared(*text, Tuning::defaults(), later).status == LoadStatus::OK);
        // And what a later build reads is not an earlier build's: it takes only its own version.
        const auto current = serialise_shared(rich_shared());
        REQUIRE(parse_shared(*current, Tuning::defaults(), later).status == LoadStatus::DAMAGED);
    }
}

TEST_CASE("a payload merges to the same result as the data it carries", "[property][save][merge]")
{
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        CAPTURE(seed);
        Rng rng(seed * 31);
        const SharedData a = generate_shared(rng);
        const SharedData b = generate_shared(rng);
        const SharedParseResult read_a = parse_shared(serialise_shared(a).value());
        const SharedParseResult read_b = parse_shared(serialise_shared(b).value());
        REQUIRE(read_a.status == LoadStatus::OK);
        REQUIRE(read_b.status == LoadStatus::OK);
        REQUIRE(read_a.data == a);
        REQUIRE(merge(read_a.data, read_b.data) == merge(a, b));
        // What a sync pushes after a merge is what the other device reads back.
        const auto result = merge(a, b);
        REQUIRE(result.has_value());
        REQUIRE(parse_shared(serialise_shared(*result).value()).data == *result);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// adopt_shared
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// A device with its own settings, a saved run, and two players.
SaveData local_save()
{
    SaveData data;
    data.device.id = std::string(DEVICE_A);
    data.device.sound = false;
    data.device.music = false;
    data.device.haptics = false;
    data.device.game_center = true;
    data.device.cloud_sync = true;
    data.device.next_seq = 12;
    data.device.current_player = std::string(PLAYER_2);
    data.device.run = std::vector<std::int64_t>{5, 4, 3, 2, 1};
    data.device.run_player = std::string(PLAYER_2);
    data.shared =
        shared_of({make_player("first", "ADA", 1), make_player(PLAYER_2, "BEN", 1)}, {entry("ADA", 70, DEVICE_A, 11)});
    return data;
}

// The device part with nothing the repair may touch changed, for comparing.
DeviceData settings_of(const DeviceData& device)
{
    DeviceData copy = device;
    copy.current_player.clear();
    copy.run.reset();
    copy.run_player.clear();
    return copy;
}

} // namespace

TEST_CASE("adopting a merged part that needs no repair replaces only the shared part", "[unit][save][merge]")
{
    SaveData local = local_save();
    const SaveData before = local;
    SharedData incoming = before.shared;
    incoming.players.push_back(make_player(PLAYER_3, "CY", 1));
    incoming.top10.push_back(entry("CY", 30, DEVICE_B, 1));
    incoming = canonical(incoming);

    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE(local.shared == incoming);
    REQUIRE(local.device == before.device); // the run, the current player and every switch are as they were
    REQUIRE(local.version == before.version);
    REQUIRE(validate(local).has_value());
}

TEST_CASE("adopting a merge that removed the current player picks the first active one", "[unit][save][merge]")
{
    SaveData local = local_save();
    local.device.run.reset(); // the run is a separate question: see below
    local.device.run_player.clear();
    SharedData incoming = local.shared;
    find_player(incoming, PLAYER_2)->removed = true;

    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE(local.device.current_player == "first");
    REQUIRE(local.shared == incoming); // no new player was needed
    REQUIRE(settings_of(local.device) == settings_of(local_save().device));
    REQUIRE(validate(local).has_value());
}

TEST_CASE("adopting a merge that left the current player missing picks the first active one", "[unit][save][merge]")
{
    SaveData local = local_save();
    local.device.current_player = std::string(PLAYER_4); // not in the merged list at all
    local.device.run.reset();
    local.device.run_player.clear();
    REQUIRE(adopt_shared(local, local.shared, hex_id(77)).has_value());
    REQUIRE(local.device.current_player == "first");
}

TEST_CASE("adopting a merge that removed every player makes a new one with the id given", "[unit][save][merge]")
{
    SaveData local = local_save();
    SharedData incoming = local.shared;
    for (Player& player : incoming.players)
    {
        player.removed = true;
    }

    REQUIRE(adopt_shared(local, incoming, std::string(PLAYER_5)).has_value());
    REQUIRE(local.shared.players.size() == 3);
    REQUIRE(local.device.current_player == PLAYER_5);
    REQUIRE(current_player(local) != nullptr);
    REQUIRE(*find_player(local.shared, PLAYER_5) == make_player(PLAYER_5));
    REQUIRE(active_player_count(local.shared) == 1);
    // Its run belonged to a player who is gone.
    REQUIRE_FALSE(local.device.run.has_value());
    REQUIRE(local.device.run_player.empty());
    REQUIRE(validate(local).has_value());
    REQUIRE(serialise(local).has_value());
}

TEST_CASE("adopting a merge that removed the run's player drops the run", "[unit][save][merge]")
{
    SaveData local = local_save();
    local.device.current_player = "first";
    SharedData incoming = local.shared;
    find_player(incoming, PLAYER_2)->removed = true;

    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE(local.device.current_player == "first"); // still active, so not changed
    REQUIRE_FALSE(local.device.run.has_value());
    REQUIRE(local.device.run_player.empty());
    REQUIRE(local.device.next_seq == 12);
    REQUIRE(local.device.cloud_sync);
    REQUIRE(validate(local).has_value());
}

TEST_CASE("adopting a merge in which the run's player is missing drops the run", "[unit][save][merge]")
{
    SaveData local = local_save();
    local.device.current_player = "first";
    SharedData incoming = shared_of({make_player("first", "ADA", 1)});
    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE_FALSE(local.device.run.has_value());
    REQUIRE(local.device.run_player.empty());
    REQUIRE(validate(local).has_value());
}

TEST_CASE("adopting keeps a run whose player is still active, and a device with no run stays without one",
          "[unit][save][merge]")
{
    SaveData local = local_save();
    local.device.current_player = "first"; // the run is another player's, and stays
    const SaveData before = local;
    REQUIRE(adopt_shared(local, before.shared, hex_id(77)).has_value());
    REQUIRE(local == before);

    local.device.run.reset();
    local.device.run_player.clear();
    SharedData incoming = local.shared;
    find_player(incoming, PLAYER_2)->removed = true;
    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE_FALSE(local.device.run.has_value());
}

TEST_CASE("a refused adoption leaves the local save as it was", "[unit][save][merge]")
{
    SaveData local = local_save();
    const SaveData before = local;
    SharedData incoming = local.shared;
    for (Player& player : incoming.players)
    {
        player.removed = true;
    }
    // The id for the new player is one a removed player already has, which would bring that player back in a merge.
    REQUIRE(adopt_shared(local, incoming, std::string(PLAYER_2)) == std::unexpected(PlayerError::ID_TAKEN));
    REQUIRE(local == before);
    REQUIRE(adopt_shared(local, incoming, "not an id") == std::unexpected(PlayerError::ID_INVALID));
    REQUIRE(local == before);
}

namespace
{

// The incoming shared part for the next_seq tests: the local players and these entries, in canonical form.
SharedData incoming_with(std::vector<ScoreEntry> top10)
{
    return canonical(shared_of(local_save().shared.players, std::move(top10)));
}

} // namespace

TEST_CASE("adopting raises next_seq past this device's own entries that are as new or newer", "[unit][save][merge]")
{
    // A device restored from a backup: its counter says 12, and the shared list holds its entries up to 14.
    SaveData local = local_save();
    const SaveData before = local;
    const SharedData incoming = incoming_with({entry("A", 90, DEVICE_A, 13), entry("B", 80, DEVICE_A, 14),
                                               entry("C", 70, DEVICE_A, 11), entry("D", 60, DEVICE_A, 3)});

    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE(local.device.next_seq == 15);
    REQUIRE(local.shared == incoming);
    DeviceData expected = before.device;
    expected.next_seq = 15;
    REQUIRE(local.device == expected); // nothing else on the device moved
    REQUIRE(validate(local).has_value());

    // The point of it: the next score takes a number nobody has, so a later merge keeps it.
    REQUIRE(record_score(local, PLAYER_2, "NEW", 85).has_value());
    const auto again = merge(local.shared, incoming);
    REQUIRE(again.has_value());
    REQUIRE(std::ranges::count_if(again->top10, [](const ScoreEntry& e) { return e.name == "NEW"; }) == 1);
    REQUIRE(local.shared.top10.size() == again->top10.size());
}

TEST_CASE("adopting leaves next_seq alone when it is already past this device's own entries", "[unit][save][merge]")
{
    SECTION("the newest own entry is the one just before it")
    {
        SaveData local = local_save(); // next_seq 12
        REQUIRE(
            adopt_shared(local, incoming_with({entry("A", 90, DEVICE_A, 11), entry("B", 80, DEVICE_A, 2)}), hex_id(77))
                .has_value());
        REQUIRE(local.device.next_seq == 12);
    }
    SECTION("it is never lowered, however old the entries are")
    {
        SaveData local = local_save();
        local.device.next_seq = 40;
        REQUIRE(adopt_shared(local, incoming_with({entry("A", 90, DEVICE_A, 5)}), hex_id(77)).has_value());
        REQUIRE(local.device.next_seq == 40);
    }
    SECTION("there are no entries of its own, or none at all")
    {
        SaveData local = local_save();
        REQUIRE(adopt_shared(local, incoming_with({}), hex_id(77)).has_value());
        REQUIRE(local.device.next_seq == 12);
    }
}

TEST_CASE("adopting ignores the entries of other devices when it raises next_seq", "[unit][save][merge]")
{
    SaveData local = local_save();
    const SharedData incoming = incoming_with({entry("A", 90, DEVICE_B, 99), entry("B", 80, DEVICE_C, 500),
                                               entry("C", 70, "", 700), entry("D", 60, DEVICE_A, 4)});
    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE(local.device.next_seq == 12); // the empty device is not this device, which has an id
}

TEST_CASE("adopting on a device with no id yet counts the entries with an empty device", "[unit][save][merge]")
{
    // The device has not been given an id, so the entries with no device are the ones `assign_device_id` will make its
    // own, and its numbers must clear them.
    SaveData local = local_save();
    local.device.id.clear();
    local.device.next_seq = 3;
    const SharedData incoming = incoming_with(
        {entry("A", 90, DEVICE_B, 99), entry("B", 80, "", 6), entry("C", 70, "", 2), entry("D", 60, DEVICE_C, 50)});
    REQUIRE(adopt_shared(local, incoming, hex_id(77)).has_value());
    REQUIRE(local.device.next_seq == 7);

    // With nothing of its own to clear, the counter stays.
    SaveData fresh = local_save();
    fresh.device.id.clear();
    fresh.device.next_seq = 1;
    REQUIRE(adopt_shared(fresh, incoming_with({entry("A", 90, DEVICE_B, 99)}), hex_id(77)).has_value());
    REQUIRE(fresh.device.next_seq == 1);
}

TEST_CASE("adopting refuses rather than wrap a counter, and leaves the local save as it was", "[unit][save][merge]")
{
    SaveData local = local_save();
    const SaveData before = local;
    REQUIRE(adopt_shared(local, incoming_with({entry("A", 90, DEVICE_A, MAX_SAVED_COUNTER)}), hex_id(77)) ==
            std::unexpected(PlayerError::COUNTER_FULL));
    REQUIRE(local == before);

    // One short of the bound is fine: the counter reaches it, and no further.
    REQUIRE(
        adopt_shared(local, incoming_with({entry("A", 90, DEVICE_A, MAX_SAVED_COUNTER - 1)}), hex_id(77)).has_value());
    REQUIRE(local.device.next_seq == MAX_SAVED_COUNTER);
    REQUIRE(validate(local).has_value());

    // An entry of another device at the bound is no business of this one's counter.
    SaveData other = local_save();
    REQUIRE(adopt_shared(other, incoming_with({entry("A", 90, DEVICE_B, MAX_SAVED_COUNTER)}), hex_id(77)).has_value());
    REQUIRE(other.device.next_seq == 12);
}

TEST_CASE("a merge then an adoption is a sync, whichever device runs it first", "[unit][save][merge]")
{
    // Two devices that played apart, each with its own run and current player.
    SaveData one = local_save();
    SaveData two = local_save();
    two.device.id = std::string(DEVICE_B);
    two.device.current_player = "first";
    two.device.run.reset();
    two.device.run_player.clear();
    REQUIRE(record_score(two, "first", "ADA", 400).has_value());
    REQUIRE(remove_player(two.shared, PLAYER_2).has_value());
    REQUIRE(record_wallet(two.shared.players[0], two.device.id, 25).has_value());

    const auto both = merge(one.shared, two.shared);
    REQUIRE(both.has_value());
    REQUIRE(adopt_shared(one, *both, hex_id(77)).has_value());
    REQUIRE(adopt_shared(two, *both, hex_id(78)).has_value());
    REQUIRE(one.shared == two.shared);
    // Device one was playing the player device two removed: it moves to the first, and its run goes.
    REQUIRE(one.device.current_player == "first");
    REQUIRE_FALSE(one.device.run.has_value());
    REQUIRE(player_wallet(one.shared.players[0]) == 25);
    REQUIRE(one.shared.top10.front().score == 400);
    REQUIRE(validate(one).has_value());
    REQUIRE(validate(two).has_value());
}
