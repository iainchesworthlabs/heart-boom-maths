// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "services/cloud_sync.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;

namespace
{

constexpr std::string_view DEVICE_A = "00000000000000a1";
constexpr std::string_view DEVICE_B = "00000000000000b2";
constexpr std::uint64_t MADE_UP_PLAYER_BITS = 0xC0FFEE;

/// One device's save, as it is after the app has given the device its id.
SaveData make_device(std::string_view device_id)
{
    SaveData data;
    REQUIRE(assign_device_id(data, std::string(device_id)));
    return data;
}

/// What a sync of this device does, with a recorded count of the times it needed a player id made up.
struct Run
{
    CloudSyncResult result;
    int ids_asked = 0;
};

Run sync(CloudStore& store, SaveData& data, const Tuning& tuning = Tuning::defaults())
{
    Run run;
    run.result = sync_with_cloud(
        store, data,
        [&run](const SharedData& merged)
        {
            ++run.ids_asked;
            return new_player_id(merged, MADE_UP_PLAYER_BITS);
        },
        tuning);
    return run;
}

void earn(SaveData& data, std::int64_t coins)
{
    give_wallet(data, wallet_of(data) + coins);
}

std::string add(SaveData& data, std::uint64_t id_bits)
{
    std::string id = new_player_id(data.shared, id_bits);
    REQUIRE(add_player(data.shared, id));
    return id;
}

void score(SaveData& data, std::string_view player_id, std::string name, std::int64_t points)
{
    REQUIRE(record_score(data, player_id, std::move(name), points));
}

std::int64_t wallet_of_player(const SaveData& data, std::string_view id)
{
    const Player* player = find_player(data.shared, id);
    REQUIRE(player != nullptr);
    return player_wallet(*player);
}

std::string payload_of(const SharedData& shared, const Tuning& tuning = Tuning::defaults())
{
    const auto text = serialise_shared(shared, tuning);
    REQUIRE(text.has_value());
    return *text;
}

/// Runs device A and device B against one store in turn until a whole round changes nothing: no write and no local
/// change. Returns how many rounds it took, and fails if it never settles.
int settle(FakeCloudStore& store_a, FakeCloudStore& store_b, SaveData& a, SaveData& b)
{
    int settled_in = 0;
    for (int round = 1; round <= 6 && settled_in == 0; ++round)
    {
        const int writes_before = store_a.writes + store_b.writes;
        const bool changed_a = sync(store_a, a).result.local_changed();
        const bool changed_b = sync(store_b, b).result.local_changed();
        if (store_a.writes + store_b.writes == writes_before && !changed_a && !changed_b)
        {
            settled_in = round;
        }
    }
    // A check and not FAIL(): FAIL ends the function by throwing, so a return after it is unreachable code (C4702, an
    // error in the sanitizer build) and none is a missing return (C4715) in the release build.
    REQUIRE(settled_in > 0);
    return settled_in;
}

/// Two devices on one Apple Account.
struct TwoDevices
{
    TwoDevices()
        : account(std::make_shared<CloudAccount>()), store_a(account), store_b(account), a(make_device(DEVICE_A)),
          b(make_device(DEVICE_B))
    {
    }

    std::shared_ptr<CloudAccount> account;
    FakeCloudStore store_a;
    FakeCloudStore store_b;
    SaveData a;
    SaveData b;
};

} // namespace

TEST_CASE("With no payload the local shared part is written and nothing else changes", "[unit][cloud_sync]")
{
    FakeCloudStore store;
    SaveData local = make_device(DEVICE_A);
    earn(local, 40);
    const SaveData before = local;

    const Run run = sync(store, local);
    CHECK(run.result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    CHECK_FALSE(run.result.write_failed);
    CHECK(store.reads == 1);
    CHECK(store.writes == 1);
    CHECK(store.account->text == payload_of(before.shared));
    CHECK(local == before);
    CHECK(run.ids_asked == 0);
}

TEST_CASE("A payload from a later build is left alone: no merge and no write", "[unit][cloud_sync]")
{
    FakeCloudStore store;
    store.account->text = R"({"version":99,"shared":{"from":"a later build"}})";
    SaveData local = make_device(DEVICE_A);
    earn(local, 40);
    const SaveData before = local;

    const Run run = sync(store, local);
    CHECK(run.result.outcome == CloudSyncOutcome::SKIPPED_NEWER);
    CHECK(store.writes == 0);
    CHECK(store.account->text == R"({"version":99,"shared":{"from":"a later build"}})");
    CHECK(local == before);
}

TEST_CASE("A payload that cannot be read is replaced by the local shared part", "[unit][cloud_sync]")
{
    const std::string damaged[] = {
        "not json at all",
        "",
        "[1,2,3]",
        R"({"version":2})",             // the shared part is missing
        R"({"version":1,"shared":{}})", // below the version this build writes: there is no migration of payloads
        R"({"version":2,"shared":{"players":5,"top10":[]}})",
    };
    for (const std::string& text : damaged)
    {
        CAPTURE(text);
        FakeCloudStore store;
        store.account->text = text;
        SaveData local = make_device(DEVICE_A);
        earn(local, 7);
        const SaveData before = local;

        const Run run = sync(store, local);
        CHECK(run.result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
        CHECK(store.account->text == payload_of(before.shared));
        CHECK(local == before); // the local save is never damaged by what was in the cloud
    }
}

TEST_CASE("A sync of data already in step changes and writes nothing", "[unit][cloud_sync]")
{
    FakeCloudStore store;
    SaveData local = make_device(DEVICE_A);
    earn(local, 25);
    score(local, FIRST_PLAYER_ID, "ROCKET", 900);
    REQUIRE(sync(store, local).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    const int writes = store.writes;
    const SaveData before = local;

    for (int i = 0; i < 3; ++i)
    {
        const Run run = sync(store, local);
        CHECK(run.result.outcome == CloudSyncOutcome::NOTHING_TO_DO);
    }
    CHECK(store.writes == writes);
    CHECK(local == before);
}

TEST_CASE("What the cloud has and this device lacks changes only the local save", "[unit][cloud_sync]")
{
    TwoDevices two;
    earn(two.a, 60);
    const std::string friend_id = add(two.a, 0x11);
    REQUIRE(rename_player(two.a.shared, friend_id, "SAM"));
    score(two.a, FIRST_PLAYER_ID, "ROCKET", 1200);
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    const std::string in_cloud = *two.account->text;

    const Run run = sync(two.store_b, two.b);
    CHECK(run.result.outcome == CloudSyncOutcome::LOCAL_CHANGED);
    CHECK(two.store_b.writes == 0);
    CHECK(*two.account->text == in_cloud);
    CHECK(two.b.shared == two.a.shared);
    CHECK(wallet_of_player(two.b, FIRST_PLAYER_ID) == 60);
    // The device part stays this device's own.
    CHECK(two.b.device.id == DEVICE_B);
    CHECK(two.b.device.current_player == FIRST_PLAYER_ID);
}

TEST_CASE("What each side has and the other lacks changes both", "[unit][cloud_sync]")
{
    TwoDevices two;
    earn(two.a, 60);
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    earn(two.b, 15);
    (void)add(two.b, 0x22);

    const Run run = sync(two.store_b, two.b);
    CHECK(run.result.outcome == CloudSyncOutcome::BOTH_CHANGED);
    CHECK(two.store_b.writes == 1);
    CHECK(wallet_of_player(two.b, FIRST_PLAYER_ID) == 75);
    CHECK(active_player_count(two.b.shared) == 2);
    CHECK(*two.account->text == payload_of(two.b.shared));
}

TEST_CASE("What this device has and the cloud lacks changes only the cloud", "[unit][cloud_sync]")
{
    TwoDevices two;
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    earn(two.a, 12);
    const SaveData before = two.a;

    const Run run = sync(two.store_a, two.a);
    CHECK(run.result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    CHECK(two.a == before);
    CHECK(*two.account->text == payload_of(two.a.shared));
}

TEST_CASE("A merge that breaks a format limit keeps the local data, writes nothing and says why", "[unit][cloud_sync]")
{
    // A tuning that lets 40 players be added, so each side can hold 40 and the two together are more than the
    // format's 64.
    Tuning many = Tuning::defaults();
    many.max_players = 100;
    SaveData local = make_device(DEVICE_A);
    SharedData other;
    for (std::uint64_t i = 1; i <= 40; ++i)
    {
        REQUIRE(add_player(local.shared, new_player_id(local.shared, 0x1000 + i), many));
        REQUIRE(add_player(other, new_player_id(other, 0x2000 + i), many));
    }
    FakeCloudStore store;
    store.account->text = payload_of(other);
    const SaveData before = local;
    const std::string cloud_before = *store.account->text;

    const Run run = sync(store, local, many);
    CHECK(run.result.outcome == CloudSyncOutcome::REFUSED);
    CHECK_FALSE(run.result.reason.empty());
    CHECK(store.writes == 0);
    CHECK(*store.account->text == cloud_before);
    CHECK(local == before);
}

TEST_CASE("A payload too large to send is refused before anything changes", "[unit][cloud_sync]")
{
    // The limit is far above a real payload; the test lowers nothing, it fills the top 10 of a tuning that allows a
    // long list until the text passes the limit.
    Tuning huge = Tuning::defaults();
    huge.top10_size = 20'000;
    SaveData local = make_device(DEVICE_A);
    for (int i = 0; i < 20'000; ++i)
    {
        local.shared.top10.push_back(
            ScoreEntry{.name = "BIG LIST NAM", .score = 20'000 - i, .device = std::string(DEVICE_A), .seq = i + 1});
    }
    local.device.next_seq = 20'001;
    REQUIRE(payload_of(local.shared, huge).size() > CLOUD_PAYLOAD_LIMIT_BYTES);

    FakeCloudStore store;
    const SaveData before = local;
    // No payload in the cloud: the local part is too large to send.
    CHECK(sync(store, local, huge).result.outcome == CloudSyncOutcome::REFUSED);
    CHECK(store.writes == 0);
    CHECK(local == before);
}

TEST_CASE("A write the store does not take is reported, and the next sync sends it again", "[unit][cloud_sync]")
{
    FakeCloudStore store;
    SaveData local = make_device(DEVICE_A);
    earn(local, 9);
    store.fail_writes = true;

    const Run failed = sync(store, local);
    CHECK(failed.result.write_failed);
    CHECK(failed.result.outcome == CloudSyncOutcome::NOTHING_TO_DO);
    CHECK_FALSE(store.account->text.has_value());

    store.fail_writes = false;
    const Run again = sync(store, local);
    CHECK_FALSE(again.result.write_failed);
    CHECK(again.result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    CHECK(store.account->text == payload_of(local.shared));
}

TEST_CASE("A merged part with nobody left makes up one player, and sends that player too", "[unit][cloud_sync]")
{
    TwoDevices two;
    const std::string second = add(two.a, 0x33);
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    REQUIRE(sync(two.store_b, two.b).result.outcome == CloudSyncOutcome::LOCAL_CHANGED);
    // Apart, each removes the player the other keeps.
    REQUIRE(remove_player(two.a.shared, FIRST_PLAYER_ID));
    REQUIRE(remove_player(two.b.shared, second));
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);

    const Run run = sync(two.store_b, two.b);
    CHECK(run.ids_asked == 1);
    CHECK(run.result.outcome == CloudSyncOutcome::BOTH_CHANGED);
    CHECK(active_player_count(two.b.shared) == 1);
    CHECK(current_player(two.b) != nullptr);
    CHECK(current_player(two.b)->id.substr(4) == hex_id(MADE_UP_PLAYER_BITS).substr(4)); // after the 4 digit counter
    CHECK(*two.account->text == payload_of(two.b.shared)); // the made-up player is in the cloud copy, so A makes none

    const Run other = sync(two.store_a, two.a);
    CHECK(other.ids_asked == 0);
    CHECK(two.a.shared == two.b.shared);
}

TEST_CASE("Nobody's player id is asked for when the merge leaves somebody", "[unit][cloud_sync]")
{
    TwoDevices two;
    (void)add(two.a, 0x44);
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    CHECK(sync(two.store_b, two.b).ids_asked == 0);
}

TEST_CASE("The current player removed on another device is replaced, and the saved run of that player dropped",
          "[unit][cloud_sync]")
{
    TwoDevices two;
    const std::string second = add(two.a, 0x55);
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    REQUIRE(sync(two.store_b, two.b).result.outcome == CloudSyncOutcome::LOCAL_CHANGED);
    // B is playing as the second player and has a paused run; A removes that player.
    REQUIRE(set_current_player(two.b, second));
    give_run(two.b, {1, 2, 3});
    REQUIRE(remove_player(two.a.shared, second));
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);

    const Run run = sync(two.store_b, two.b);
    CHECK(run.result.local_changed());
    CHECK(two.b.device.current_player == FIRST_PLAYER_ID);
    CHECK_FALSE(two.b.device.run.has_value());
    CHECK(two.b.device.run_player.empty());
}

TEST_CASE("Two devices that were played apart settle on the same data, whatever each did", "[unit][cloud_sync]")
{
    TwoDevices two;
    // Both know a second player and a third before they part.
    const std::string second = add(two.a, 0x61);
    const std::string third = add(two.a, 0x62);
    REQUIRE(sync(two.store_a, two.a).result.outcome == CloudSyncOutcome::CLOUD_CHANGED);
    REQUIRE(sync(two.store_b, two.b).result.outcome == CloudSyncOutcome::LOCAL_CHANGED);
    REQUIRE(settle(two.store_a, two.store_b, two.a, two.b) == 1);
    const std::int64_t first_before = wallet_of_player(two.a, FIRST_PLAYER_ID);

    SECTION("coins earned on both devices, by the same player")
    {
        earn(two.a, 50);
        earn(two.b, 30);
        CHECK(settle(two.store_a, two.store_b, two.a, two.b) <= 3);
        CHECK(two.a.shared == two.b.shared);
        CHECK(wallet_of_player(two.a, FIRST_PLAYER_ID) == first_before + 80);
    }

    SECTION("players added on both devices")
    {
        const std::string from_a = add(two.a, 0x71);
        // B is full at four players only if a fourth were added; with three it has room for one more.
        const std::string from_b = add(two.b, 0x72);
        CHECK(settle(two.store_a, two.store_b, two.a, two.b) <= 3);
        CHECK(two.a.shared == two.b.shared);
        // More than max_players active players is kept (decision P6): nothing is lost.
        CHECK(active_player_count(two.a.shared) == 5);
        CHECK(find_player(two.a.shared, from_a) != nullptr);
        CHECK(find_player(two.a.shared, from_b) != nullptr);
    }

    SECTION("a rename on one device")
    {
        REQUIRE(rename_player(two.a.shared, second, "SAM"));
        CHECK(settle(two.store_a, two.store_b, two.a, two.b) <= 3);
        CHECK(find_player(two.b.shared, second)->name == "SAM");
        CHECK(two.a.shared == two.b.shared);
    }

    SECTION("a removal on one device")
    {
        REQUIRE(remove_player(two.a.shared, third));
        CHECK(settle(two.store_a, two.store_b, two.a, two.b) <= 3);
        CHECK(find_player(two.b.shared, third)->removed);
        CHECK(active_player_count(two.b.shared) == 2);
        CHECK(two.a.shared == two.b.shared);
    }

    SECTION("top 10 entries on both devices")
    {
        score(two.a, FIRST_PLAYER_ID, "AMY", 700);
        score(two.a, FIRST_PLAYER_ID, "AMY", 300);
        score(two.b, second, "BOB", 900);
        score(two.b, second, "BOB", 300);
        CHECK(settle(two.store_a, two.store_b, two.a, two.b) <= 3);
        CHECK(two.a.shared == two.b.shared);
        REQUIRE(two.a.shared.top10.size() == 4);
        CHECK(two.a.shared.top10[0].score == 900);
        CHECK(two.a.shared.top10[1].score == 700);
        // The two 300s are two entries, one from each device, and the same two on both.
        CHECK(two.a.shared.top10[2].score == 300);
        CHECK(two.a.shared.top10[3].score == 300);
        CHECK(two.a.shared.top10[2].device != two.a.shared.top10[3].device);
    }

    SECTION("all of it at once")
    {
        earn(two.a, 50);
        earn(two.b, 30);
        (void)add(two.a, 0x81);
        REQUIRE(rename_player(two.b.shared, second, "ZED"));
        REQUIRE(remove_player(two.b.shared, third));
        score(two.a, FIRST_PLAYER_ID, "AMY", 400);
        score(two.b, second, "BOB", 600);
        CHECK(settle(two.store_a, two.store_b, two.a, two.b) <= 3);
        CHECK(two.a.shared == two.b.shared);
        CHECK(wallet_of_player(two.a, FIRST_PLAYER_ID) == first_before + 80);
        CHECK(find_player(two.a.shared, second)->name == "ZED");
        CHECK(find_player(two.a.shared, third)->removed);
        CHECK(two.a.shared.top10.size() == 2);
    }
}
