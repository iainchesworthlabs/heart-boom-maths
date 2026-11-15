// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_manager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace heart_boom_maths;

namespace
{

SaveData progress()
{
    SaveData data;
    data.device.id = "9f1c2a7b3d4e5f60";
    data.device.next_seq = 2;
    data.device.run = std::vector<std::int64_t>{5, 6, 7};
    data.device.run_player = "first";
    data.shared.players[0].coins = {{data.device.id, 40, 0}};
    data.shared.players[0].best = 300;
    data.shared.top10 = {{"ADA", 300, data.device.id, 1}};
    return data;
}

} // namespace

TEST_CASE("the manager starts a first run on defaults", "[unit][integration][save]")
{
    MemorySaveStore store;
    SaveManager manager(store);
    const LoadResult loaded = manager.load();
    REQUIRE(loaded.status == LoadStatus::OK);
    REQUIRE_FALSE(loaded.file_existed);
    REQUIRE(loaded.data == SaveData{});
    REQUIRE_FALSE(store.text().has_value());
}

TEST_CASE("the manager saves and loads a game", "[unit][integration][save]")
{
    MemorySaveStore store;
    {
        SaveManager manager(store);
        REQUIRE(manager.load().status == LoadStatus::OK);
        REQUIRE(manager.save(progress()) == SaveOutcome::SAVED);
        REQUIRE(store.store_count() == 1);
    }
    // A fresh manager, as after a restart, sees what the first one wrote.
    SaveManager second(store);
    const LoadResult loaded = second.load();
    REQUIRE(loaded.status == LoadStatus::OK);
    REQUIRE(loaded.file_existed);
    REQUIRE(loaded.data == progress());
}

TEST_CASE("the manager does not write before it has looked", "[unit][integration][save]")
{
    MemorySaveStore store(R"({"version":1,"coins":3,"won_once":false,"sound":true,"music":true,"top10":[]})");
    SaveManager manager(store);
    REQUIRE(manager.save(progress()) == SaveOutcome::NOT_LOADED);
    REQUIRE(store.store_count() == 0);
}

TEST_CASE("a damaged file loads as defaults and may then be replaced", "[unit][integration][save]")
{
    const std::string wreck = "{\"version\":1,\"coins\":";
    MemorySaveStore store(wreck);
    SaveManager manager(store);
    const LoadResult loaded = manager.load();
    REQUIRE(loaded.status == LoadStatus::DAMAGED);
    REQUIRE(loaded.file_existed);
    REQUIRE(loaded.damage == SaveError::NOT_JSON);
    REQUIRE(loaded.data == SaveData{});

    // The caller keeps the wreck as save.json.bad. Saving is then allowed, and the next load reads the new save.
    REQUIRE(store.text() == wreck);
    REQUIRE(manager.save(progress()) == SaveOutcome::SAVED);
    SaveManager next(store);
    REQUIRE(next.load().data == progress());
}

TEST_CASE("a newer file is never overwritten", "[unit][integration][save]")
{
    const std::string future = R"({"version":3,"coins":999,"shiny":[1,2,3]})";
    MemorySaveStore store(future);
    SaveManager manager(store);
    const LoadResult loaded = manager.load();
    REQUIRE(loaded.status == LoadStatus::NEWER);
    REQUIRE(loaded.file_existed);
    REQUIRE(loaded.data == SaveData{});

    REQUIRE(manager.save(progress()) == SaveOutcome::REFUSED_NEWER);
    REQUIRE(manager.save(SaveData{}) == SaveOutcome::REFUSED_NEWER);
    REQUIRE(store.store_count() == 0);
    REQUIRE(store.text() == future);

    // Loading again after the file is gone (or replaced by this build's own) lifts the refusal.
    MemorySaveStore fresh;
    SaveManager other(fresh);
    REQUIRE(other.load().status == LoadStatus::OK);
    REQUIRE(other.save(progress()) == SaveOutcome::SAVED);
}

TEST_CASE("a refusal follows the latest load", "[unit][integration][save]")
{
    MemorySaveStore store(R"({"version":3})");
    SaveManager manager(store);
    REQUIRE(manager.load().status == LoadStatus::NEWER);
    REQUIRE(manager.save(progress()) == SaveOutcome::REFUSED_NEWER);

    store.store(R"({"version":1,"coins":3,"won_once":false,"sound":true,"music":true,"top10":[]})");
    REQUIRE(manager.load().status == LoadStatus::OK);
    REQUIRE(manager.save(progress()) == SaveOutcome::SAVED);
}

TEST_CASE("the manager refuses data that breaks the format", "[unit][integration][save]")
{
    MemorySaveStore store;
    SaveManager manager(store);
    REQUIRE(manager.load().status == LoadStatus::OK);

    SaveData data = progress();
    data.shared.top10[0].name = "not allowed";
    REQUIRE(manager.save(data) == SaveOutcome::INVALID);
    REQUIRE(store.store_count() == 0);
    REQUIRE_FALSE(store.text().has_value());
}

TEST_CASE("a store that fails is reported and the old save stays", "[unit][integration][save]")
{
    MemorySaveStore store;
    SaveManager manager(store);
    REQUIRE(manager.load().status == LoadStatus::OK);
    REQUIRE(manager.save(progress()) == SaveOutcome::SAVED);
    const auto saved = store.text();

    store.set_store_fails(true);
    SaveData more = progress();
    more.shared.players[0].coins[0].earned = 90;
    REQUIRE(manager.save(more) == SaveOutcome::STORE_FAILED);
    REQUIRE(store.text() == saved);

    store.set_store_fails(false);
    REQUIRE(manager.save(more) == SaveOutcome::SAVED);
    SaveManager next(store);
    REQUIRE(player_wallet(next.load().data.shared.players[0]) == 90);
}

TEST_CASE("the manager takes the tuning's leaderboard size and name length", "[unit][integration][save]")
{
    Tuning tuning = Tuning::defaults();
    tuning.top10_size = 2;
    tuning.name_max_length = 3;

    MemorySaveStore store;
    SaveManager manager(store, tuning);
    REQUIRE(manager.load().status == LoadStatus::OK);

    SaveData data;
    data.shared.top10 = {{"ABC", 3, "", 1}, {"DEF", 2, "", 2}};
    REQUIRE(manager.save(data) == SaveOutcome::SAVED);
    data.shared.top10 = {{"ABCD", 3, "", 1}};
    REQUIRE(manager.save(data) == SaveOutcome::INVALID);
    data.shared.top10 = {{"ABC", 3, "", 1}, {"DEF", 2, "", 2}, {"GHI", 1, "", 3}};
    REQUIRE(manager.save(data) == SaveOutcome::INVALID);
}

TEST_CASE("an old file is read as it was and written back as the current version", "[unit][integration][save]")
{
    MemorySaveStore store(R"({"version":1,"coins":3,"won_once":true,"sound":false,"music":true,"top10":)"
                          R"([{"name":"ADA","score":300}]})");
    SaveManager manager(store);
    const LoadResult loaded = manager.load();
    REQUIRE(loaded.status == LoadStatus::OK);
    REQUIRE(loaded.file_existed);
    REQUIRE(player_wallet(loaded.data.shared.players[0]) == 3);
    REQUIRE(loaded.data.shared.players[0].won_once);
    REQUIRE_FALSE(loaded.data.device.sound);
    REQUIRE(loaded.data.shared.top10 == std::vector<ScoreEntry>{{"ADA", 300, "", 1}});
    // Loading does not write: the old file stays until the game saves.
    REQUIRE(store.store_count() == 0);

    REQUIRE(manager.save(loaded.data) == SaveOutcome::SAVED);
    REQUIRE(store.text()->find(R"("version":2)") != std::string::npos);
    SaveManager next(store);
    const LoadResult again = next.load();
    REQUIRE(again.status == LoadStatus::OK);
    REQUIRE(again.data == loaded.data);
}

TEST_CASE("a version 2 file with a damaged part is moved aside, not read", "[unit][integration][save]")
{
    const std::string wreck = serialise(progress()).value().replace(10, 6, "???");
    MemorySaveStore store(wreck);
    SaveManager manager(store);
    const LoadResult loaded = manager.load();
    REQUIRE(loaded.status == LoadStatus::DAMAGED);
    REQUIRE(loaded.data == SaveData{});
    REQUIRE(store.text() == wreck);
}
