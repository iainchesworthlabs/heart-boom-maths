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
    data.coins = 40;
    data.top10 = {{"ADA", 300}};
    data.run = std::vector<std::int64_t>{5, 6, 7};
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
    const std::string future = R"({"version":2,"coins":999,"shiny":[1,2,3]})";
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
    MemorySaveStore store(R"({"version":2})");
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
    data.top10[0].name = "not allowed";
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
    more.coins = 90;
    REQUIRE(manager.save(more) == SaveOutcome::STORE_FAILED);
    REQUIRE(store.text() == saved);

    store.set_store_fails(false);
    REQUIRE(manager.save(more) == SaveOutcome::SAVED);
    SaveManager next(store);
    REQUIRE(next.load().data.coins == 90);
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
    data.top10 = {{"ABC", 3}, {"DEF", 2}};
    REQUIRE(manager.save(data) == SaveOutcome::SAVED);
    data.top10 = {{"ABCD", 3}};
    REQUIRE(manager.save(data) == SaveOutcome::INVALID);
    data.top10 = {{"ABC", 3}, {"DEF", 2}, {"GHI", 1}};
    REQUIRE(manager.save(data) == SaveOutcome::INVALID);
}
