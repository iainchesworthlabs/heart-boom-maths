// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/tuning.hpp"
#include "save_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace heart_boom_maths;

namespace
{

constexpr auto INT64_MAX_VALUE = std::numeric_limits<std::int64_t>::max();
constexpr auto INT64_MIN_VALUE = std::numeric_limits<std::int64_t>::min();

constexpr std::string_view THIS_DEVICE = "9f1c2a7b3d4e5f60";
constexpr std::string_view OTHER_DEVICE = "0123456789abcdef";
constexpr std::string_view SECOND_PLAYER = "a1b2c3d4e5f60718";

SaveData sample_data()
{
    SaveData data;
    data.device.id = THIS_DEVICE;
    data.device.sound = false;
    data.device.music = true;
    data.device.game_center = true;
    data.device.haptics = false;
    data.device.cloud_sync = true;
    data.device.current_player = SECOND_PLAYER;
    data.device.next_seq = 5;
    data.device.run = std::vector<std::int64_t>{1, 2, 3, -4};
    data.device.run_player = SECOND_PLAYER;
    data.shared.players = {
        Player{.id = "first",
               .name = "ADA",
               .name_rev = 2,
               .won_once = true,
               .best = 900,
               .coins = {{std::string(THIS_DEVICE), 250, 50}, {std::string(OTHER_DEVICE), 10, 0}}},
        Player{.id = std::string(SECOND_PLAYER)},
        Player{.id = "00000000000000ff", .name = "OLD", .name_rev = 1, .removed = true, .best = 5},
    };
    data.shared.top10 = {{"ADA", 900, std::string(THIS_DEVICE), 1},
                         {"BEN", 500, std::string(OTHER_DEVICE), 2},
                         {"CY", 500, std::string(THIS_DEVICE), 3},
                         {"DI", 1, std::string(THIS_DEVICE), 4}};
    return data;
}

ParseResult parse(std::string_view text)
{
    return parse_save(text);
}

// Replaces (or, with no text, removes) the member at `path` of a document. A path step that is all digits indexes an
// array.
void edit_at(json::Value& node, std::span<const std::string_view> path, const std::optional<json::Value>& replacement)
{
    const std::string_view step = path.front();
    const bool last = path.size() == 1;
    if (json::Value::Array* items = node.get_array(); items != nullptr)
    {
        const std::size_t index = static_cast<std::size_t>(std::stoul(std::string(step)));
        REQUIRE(index < items->size());
        if (!last)
        {
            edit_at((*items)[index], path.subspan(1), replacement);
        }
        else if (replacement)
        {
            (*items)[index] = *replacement;
        }
        else
        {
            items->erase(items->begin() + static_cast<std::ptrdiff_t>(index));
        }
        return;
    }
    if (last)
    {
        if (replacement)
        {
            node.set(std::string(step), *replacement);
        }
        else
        {
            node.erase(step);
        }
        return;
    }
    const json::Value* child = node.find(step);
    REQUIRE(child != nullptr);
    json::Value copy = *child;
    edit_at(copy, path.subspan(1), replacement);
    node.set(std::string(step), std::move(copy));
}

std::string edited_text(std::string_view base, std::initializer_list<std::string_view> path,
                        std::optional<std::string_view> value_text)
{
    auto parsed = json::parse(base);
    REQUIRE(parsed.has_value());
    std::optional<json::Value> replacement;
    if (value_text)
    {
        auto value = json::parse(*value_text);
        REQUIRE(value.has_value());
        replacement = std::move(*value);
    }
    const std::vector<std::string_view> steps(path);
    edit_at(*parsed, steps, replacement);
    return json::write(*parsed);
}

// A complete valid document with one member replaced, so each damaged case changes exactly one thing.
std::string document_with(std::initializer_list<std::string_view> path, std::string_view value_text)
{
    return edited_text(serialise(sample_data()).value(), path, value_text);
}

std::string document_without(std::initializer_list<std::string_view> path)
{
    return edited_text(serialise(sample_data()).value(), path, std::nullopt);
}

void require_damaged(std::string_view text, SaveError reason)
{
    const ParseResult result = parse(text);
    REQUIRE(result.status == LoadStatus::DAMAGED);
    REQUIRE(result.damage == reason);
    REQUIRE(result.data == SaveData{});
}

} // namespace

TEST_CASE("save defaults are the SPEC defaults", "[unit][save]")
{
    const SaveData data;
    REQUIRE(data.version == 2);
    REQUIRE(SAVE_VERSION == 2);
    REQUIRE(data.device.id.empty());
    REQUIRE(data.device.sound);
    REQUIRE(data.device.music);
    REQUIRE_FALSE(data.device.game_center);
    REQUIRE(data.device.haptics);
    REQUIRE_FALSE(data.device.cloud_sync);
    REQUIRE(data.device.current_player == "first");
    REQUIRE(data.device.next_seq == 1);
    REQUIRE_FALSE(data.device.run.has_value());
    REQUIRE(data.device.run_player.empty());
    // A first run has one player with no name, who is the current player.
    REQUIRE(data.shared.players.size() == 1);
    REQUIRE(data.shared.players[0] == Player{.id = "first"});
    REQUIRE(data.shared.players[0].name.empty());
    REQUIRE(data.shared.players[0].coins.empty());
    REQUIRE(data.shared.top10.empty());
    REQUIRE(validate(data).has_value());
}

TEST_CASE("a save round-trips through text", "[unit][save]")
{
    const SaveData data = sample_data();
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    const ParseResult result = parse(*text);
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE_FALSE(result.damage.has_value());
    REQUIRE(result.data == data);
    REQUIRE(result.file_version == 2);
    REQUIRE(serialise(result.data).value() == *text);
}

TEST_CASE("the default save round-trips", "[unit][save]")
{
    const auto text = serialise(SaveData{});
    REQUIRE(text.has_value());
    REQUIRE(*text ==
            R"({"device":{"cloud_sync":false,"current_player":"first","game_center":false,"haptics":true,"id":"",)"
            R"("music":true,"next_seq":1,"sound":true},"shared":{"players":[{"best":0,"coins":[],"id":"first",)"
            R"("name":"","name_rev":0,"removed":false,"won_once":false}],"top10":[]},"version":2})");
    REQUIRE(parse(*text).data == SaveData{});
}

TEST_CASE("the serialised form is stable", "[unit][save]")
{
    SaveData data;
    data.device.id = THIS_DEVICE;
    data.device.next_seq = 3;
    data.device.run = std::vector<std::int64_t>{1, 2, 3};
    data.device.run_player = "first";
    data.shared.players[0].name = "ROCKET";
    data.shared.players[0].name_rev = 1;
    data.shared.players[0].best = 1640;
    data.shared.players[0].coins = {{std::string(THIS_DEVICE), 80, 19}};
    data.shared.top10 = {{"ROCKET", 1640, std::string(THIS_DEVICE), 2}};
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    REQUIRE(*text ==
            R"({"device":{"cloud_sync":false,"current_player":"first","game_center":false,"haptics":true,)"
            R"("id":"9f1c2a7b3d4e5f60","music":true,"next_seq":3,"run":[1,2,3],"run_player":"first","sound":true},)"
            R"("shared":{"players":[{"best":1640,"coins":[{"device":"9f1c2a7b3d4e5f60","earned":80,"spent":19}],)"
            R"("id":"first","name":"ROCKET","name_rev":1,"removed":false,"won_once":false}],)"
            R"("top10":[{"device":"9f1c2a7b3d4e5f60","name":"ROCKET","score":1640,"seq":2}]},"version":2})");
}

TEST_CASE("the text of a save is what the JSON writer gives for the document it holds", "[unit][save]")
{
    // The save is written without a tree of values in between, so this holds it to the writer's form: members in key
    // order, no spaces. A member written out of place would come back in its place when the text is read and written.
    SaveData extremes = sample_data();
    extremes.device.next_seq = MAX_SAVED_COUNTER;
    extremes.device.run = std::vector<std::int64_t>{INT64_MIN_VALUE, INT64_MAX_VALUE, 0, -1};
    extremes.shared.players[0].coins = {{std::string(THIS_DEVICE), MAX_SAVED_COINS, MAX_SAVED_COINS}};
    extremes.shared.top10 = {{"A", INT64_MAX_VALUE, std::string(THIS_DEVICE), MAX_SAVED_COUNTER}, {"B", 1, "", 0}};
    SaveData empty_run = sample_data();
    empty_run.device.run = std::vector<std::int64_t>{};
    SaveData no_players;
    no_players.shared.players.clear();
    no_players.device.run.reset();
    no_players.device.run_player.clear();

    for (const SaveData* data : {&extremes, &empty_run, &no_players})
    {
        const auto text = serialise(*data);
        REQUIRE(text.has_value());
        const auto document = json::parse(*text);
        REQUIRE(document.has_value());
        REQUIRE(json::write(*document) == *text);

        const auto payload = serialise_shared(data->shared);
        REQUIRE(payload.has_value());
        const auto payload_document = json::parse(*payload);
        REQUIRE(payload_document.has_value());
        REQUIRE(json::write(*payload_document) == *payload);
    }
}

TEST_CASE("a save holds 64-bit extremes and counters at their bounds", "[unit][save]")
{
    SaveData data = sample_data();
    data.device.next_seq = MAX_SAVED_COUNTER;
    data.device.run = std::vector<std::int64_t>{INT64_MIN_VALUE, INT64_MAX_VALUE, 0, -1};
    data.shared.players[0].name_rev = MAX_SAVED_COUNTER;
    data.shared.players[0].best = INT64_MAX_VALUE;
    data.shared.players[0].coins = {{std::string(THIS_DEVICE), MAX_SAVED_COINS, MAX_SAVED_COINS}};
    data.shared.top10 = {{"A", INT64_MAX_VALUE, std::string(THIS_DEVICE), MAX_SAVED_COUNTER},
                         {"B", INT64_MAX_VALUE, "", 0},
                         {"C", 1, std::string(OTHER_DEVICE), 1}};
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    REQUIRE(parse(*text).data == data);
}

TEST_CASE("the version 2 fixture file loads", "[unit][save]")
{
    const ParseResult result = parse(SAVE_V2_FIXTURE);
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE(result.file_version == 2);
    const SaveData& data = result.data;
    REQUIRE(data.version == 2);
    REQUIRE(data.device.id == "9f1c2a7b3d4e5f60");
    REQUIRE(data.device.sound);
    REQUIRE_FALSE(data.device.music);
    REQUIRE_FALSE(data.device.haptics);
    REQUIRE(data.device.game_center);
    REQUIRE(data.device.cloud_sync);
    REQUIRE(data.device.current_player == "a1b2c3d4e5f60718");
    REQUIRE(data.device.next_seq == 4);
    REQUIRE(data.device.run.has_value());
    REQUIRE(data.device.run->size() == 12);
    REQUIRE(data.device.run->back() == INT64_MAX_VALUE);
    REQUIRE(data.device.run_player == "a1b2c3d4e5f60718");

    REQUIRE(data.shared.players.size() == 3);
    const Player& ada = data.shared.players[0];
    REQUIRE(ada.id == "first");
    REQUIRE(ada.name == "ADA");
    REQUIRE(ada.name_rev == 2);
    REQUIRE_FALSE(ada.removed);
    REQUIRE(ada.won_once);
    REQUIRE(ada.best == 48210);
    REQUIRE(ada.coins == std::vector<CoinEntry>{{"9f1c2a7b3d4e5f60", 200, 63}, {"0123456789abcdef", 40, 0}});
    REQUIRE(player_wallet(ada) == 177);
    REQUIRE(data.shared.players[1].name.empty());
    REQUIRE(data.shared.players[1].coins.empty());
    REQUIRE(data.shared.players[2].removed);

    REQUIRE(data.shared.top10.size() == 4);
    REQUIRE(data.shared.top10[1] == ScoreEntry{"BEN", 31000, "0123456789abcdef", 7});
    REQUIRE(parse(serialise(data).value()).data == data);
}

TEST_CASE("the version 1 fixture migrates to one player, field by field", "[unit][save][migration]")
{
    const ParseResult result = parse(SAVE_V1_FIXTURE);
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE(result.file_version == 1);
    const SaveData& data = result.data;
    REQUIRE(data.version == 2);

    // The player: `first`, no name, the old wallet as earned under the empty device, the win-bonus mark carried, and
    // the best score of the old list.
    REQUIRE(data.shared.players.size() == 1);
    const Player& player = data.shared.players[0];
    REQUIRE(player.id == "first");
    REQUIRE(player.name.empty());
    REQUIRE(player.name_rev == 0);
    REQUIRE_FALSE(player.removed);
    REQUIRE(player.won_once);
    REQUIRE(player.best == 48210);
    REQUIRE(player.coins == std::vector<CoinEntry>{{"", 137, 0}});
    REQUIRE(player_wallet(player) == 137);

    // The top 10 keeps names and scores, takes the empty device and numbers in rank order.
    REQUIRE(data.shared.top10 ==
            std::vector<ScoreEntry>{
                {"ADA", 48210, "", 1}, {"BEN", 31000, "", 2}, {"CY", 31000, "", 3}, {"DI", 120, "", 4}});

    // The device: no id yet, the old switches, the new one off, the player, the next number, the run and its player.
    REQUIRE(data.device.id.empty());
    REQUIRE(data.device.sound);
    REQUIRE_FALSE(data.device.music);
    REQUIRE_FALSE(data.device.game_center);
    REQUIRE(data.device.haptics); // the fixture predates the haptics switch, so it reads as the default
    REQUIRE_FALSE(data.device.cloud_sync);
    REQUIRE(data.device.current_player == "first");
    REQUIRE(data.device.next_seq == 5);
    REQUIRE(data.device.run.has_value());
    REQUIRE(data.device.run->size() == 12);
    REQUIRE(data.device.run->back() == INT64_MAX_VALUE);
    REQUIRE(data.device.run_player == "first");

    // A build that writes this back out must read its own output as the same save.
    REQUIRE(parse(serialise(data).value()).data == data);
}

TEST_CASE("a version 1 save without a list or a run migrates to the empty case", "[unit][save][migration]")
{
    const ParseResult result =
        parse(R"({"version":1,"coins":0,"won_once":false,"sound":false,"music":true,"haptics":false,)"
              R"("game_center":true,"top10":[]})");
    REQUIRE(result.status == LoadStatus::OK);
    const SaveData& data = result.data;
    REQUIRE(data.shared.top10.empty());
    REQUIRE(data.shared.players[0].best == 0);
    REQUIRE(data.shared.players[0].coins == std::vector<CoinEntry>{{"", 0, 0}});
    REQUIRE(data.device.next_seq == 1);
    REQUIRE_FALSE(data.device.run.has_value());
    REQUIRE(data.device.run_player.empty());
    REQUIRE_FALSE(data.device.sound);
    REQUIRE(data.device.music);
    REQUIRE_FALSE(data.device.haptics); // an explicit haptics value moves across
    REQUIRE(data.device.game_center);
}

TEST_CASE("a damaged version 1 save reports the rule it always did", "[unit][save][migration]")
{
    const auto v1 = [](std::string_view coins, std::string_view top10)
    {
        return std::string(R"({"version":1,"coins":)") + std::string(coins) +
               R"(,"won_once":false,"sound":true,"music":true,"top10":)" + std::string(top10) + "}";
    };
    require_damaged(v1("-1", "[]"), SaveError::COINS_OUT_OF_RANGE);
    require_damaged(v1("2147483648", "[]"), SaveError::COINS_OUT_OF_RANGE);
    require_damaged(v1("\"5\"", "[]"), SaveError::WRONG_TYPE);
    require_damaged(v1("1", "{}"), SaveError::WRONG_TYPE);
    require_damaged(v1("1", "[5]"), SaveError::WRONG_TYPE);
    require_damaged(v1("1", R"([{"name":"a","score":5}])"), SaveError::NAME_INVALID);
    require_damaged(v1("1", R"([{"name":"A","score":0}])"), SaveError::SCORE_INVALID);
    require_damaged(v1("1", R"([{"name":"A","score":5},{"name":"B","score":6}])"), SaveError::TOP10_NOT_SORTED);
    require_damaged(v1("1", R"([{"name":"A"}])"), SaveError::MISSING_FIELD);
    require_damaged(R"({"version":1,"won_once":false,"sound":true,"music":true,"top10":[]})", SaveError::MISSING_FIELD);
    require_damaged(R"({"version":1,"coins":1,"won_once":false,"music":true,"top10":[]})", SaveError::MISSING_FIELD);
    require_damaged(R"({"version":1,"coins":1,"won_once":false,"sound":true,"music":true})", SaveError::MISSING_FIELD);
    require_damaged(v1("1", "[]").substr(0, v1("1", "[]").size() - 1) + R"(,"run":[1,"2"]})", SaveError::WRONG_TYPE);
}

TEST_CASE("game_center, haptics and cloud_sync are optional on reading", "[unit][save]")
{
    const ParseResult game_center = parse(document_without({"device", "game_center"}));
    REQUIRE(game_center.status == LoadStatus::OK);
    REQUIRE_FALSE(game_center.data.device.game_center);

    const ParseResult haptics = parse(document_without({"device", "haptics"}));
    REQUIRE(haptics.status == LoadStatus::OK);
    REQUIRE(haptics.data.device.haptics);

    const ParseResult cloud_sync = parse(document_without({"device", "cloud_sync"}));
    REQUIRE(cloud_sync.status == LoadStatus::OK);
    REQUIRE_FALSE(cloud_sync.data.device.cloud_sync);
}

TEST_CASE("each switch reads back off and on, and the writer always writes it", "[unit][save]")
{
    for (const char* key : {"sound", "music", "game_center", "haptics", "cloud_sync"})
    {
        DYNAMIC_SECTION(key)
        {
            const ParseResult off = parse(document_with({"device", key}, "false"));
            REQUIRE(off.status == LoadStatus::OK);
            const ParseResult on = parse(document_with({"device", key}, "true"));
            REQUIRE(on.status == LoadStatus::OK);
            REQUIRE_FALSE(off.data == on.data);
        }
    }
    SaveData data;
    data.device.haptics = false;
    data.device.cloud_sync = true;
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    REQUIRE(text->find(R"("haptics":false)") != std::string::npos);
    REQUIRE(text->find(R"("cloud_sync":true)") != std::string::npos);
    REQUIRE(text->find(R"("game_center":false)") != std::string::npos);
    const SaveData back = parse(*text).data;
    REQUIRE_FALSE(back.device.haptics);
    REQUIRE(back.device.cloud_sync);
}

TEST_CASE("unknown members are ignored", "[unit][save]")
{
    REQUIRE(parse(document_with({"something_new"}, R"({"a":[1,2]})")).data == sample_data());
    REQUIRE(parse(document_with({"device", "something_new"}, "7")).data == sample_data());
    REQUIRE(parse(document_with({"shared", "something_new"}, "[]")).data == sample_data());
    REQUIRE(parse(document_with({"shared", "players", "0", "something_new"}, "true")).data == sample_data());
    REQUIRE(parse(document_with({"shared", "top10", "0", "something_new"}, "\"x\"")).data == sample_data());
}

TEST_CASE("text that is not a save is damaged", "[unit][save]")
{
    require_damaged("", SaveError::NOT_JSON);
    require_damaged("   ", SaveError::NOT_JSON);
    require_damaged("not json", SaveError::NOT_JSON);
    require_damaged("{", SaveError::NOT_JSON);
    require_damaged(R"({"version":2,"device":{"next_seq":1.5}})", SaveError::NOT_JSON);
    require_damaged(R"({"version":2,"device":null})", SaveError::NOT_JSON);
    require_damaged(R"({"version":2} trailing)", SaveError::NOT_JSON);
    require_damaged(R"({"version":2,"version":2})", SaveError::NOT_JSON);
    require_damaged(std::string(20, '\0'), SaveError::NOT_JSON);
    require_damaged(std::string("\xFF\xFE\x00\x01", 4), SaveError::NOT_JSON);
    require_damaged(std::string(300 * 1024, ' '), SaveError::NOT_JSON);
    require_damaged("[]", SaveError::NOT_AN_OBJECT);
    require_damaged("5", SaveError::NOT_AN_OBJECT);
    require_damaged("\"save\"", SaveError::NOT_AN_OBJECT);
    require_damaged("true", SaveError::NOT_AN_OBJECT);
}

TEST_CASE("a missing field is damaged", "[unit][save]")
{
    SECTION("the document and the device")
    {
        for (const char* key : {"version", "device", "shared"})
        {
            DYNAMIC_SECTION(key)
            {
                require_damaged(document_without({key}), SaveError::MISSING_FIELD);
            }
        }
        for (const char* key : {"id", "sound", "music", "current_player", "next_seq"})
        {
            DYNAMIC_SECTION("device." << key)
            {
                require_damaged(document_without({"device", key}), SaveError::MISSING_FIELD);
            }
        }
        require_damaged(R"({"version":2})", SaveError::MISSING_FIELD);
    }
    SECTION("the shared part")
    {
        require_damaged(document_without({"shared", "players"}), SaveError::MISSING_FIELD);
        require_damaged(document_without({"shared", "top10"}), SaveError::MISSING_FIELD);
        for (const char* key : {"id", "name", "name_rev", "removed", "won_once", "best", "coins"})
        {
            DYNAMIC_SECTION("player." << key)
            {
                require_damaged(document_without({"shared", "players", "0", key}), SaveError::MISSING_FIELD);
            }
        }
        for (const char* key : {"device", "earned", "spent"})
        {
            DYNAMIC_SECTION("coins." << key)
            {
                require_damaged(document_without({"shared", "players", "0", "coins", "0", key}),
                                SaveError::MISSING_FIELD);
            }
        }
        for (const char* key : {"name", "score", "device", "seq"})
        {
            DYNAMIC_SECTION("top10." << key)
            {
                require_damaged(document_without({"shared", "top10", "0", key}), SaveError::MISSING_FIELD);
            }
        }
    }
}

TEST_CASE("a field of the wrong type is damaged", "[unit][save]")
{
    const auto wrong = [](std::initializer_list<std::string_view> path, std::string_view value)
    { require_damaged(document_with(path, value), SaveError::WRONG_TYPE); };

    wrong({"version"}, "\"2\"");
    wrong({"version"}, "true");
    wrong({"device"}, "[]");
    wrong({"device"}, "5");
    wrong({"shared"}, "[]");
    wrong({"device", "id"}, "5");
    wrong({"device", "sound"}, "\"on\"");
    wrong({"device", "music"}, "0");
    wrong({"device", "game_center"}, "1");
    wrong({"device", "haptics"}, "\"on\"");
    wrong({"device", "cloud_sync"}, "1");
    wrong({"device", "current_player"}, "5");
    wrong({"device", "next_seq"}, "\"3\"");
    wrong({"device", "next_seq"}, "true");
    wrong({"device", "run"}, "5");
    wrong({"device", "run"}, R"([1,"2"])");
    wrong({"device", "run"}, "[true]");
    wrong({"device", "run"}, "[[1]]");
    wrong({"device", "run_player"}, "5");
    wrong({"shared", "players"}, "{}");
    wrong({"shared", "players"}, "[5]");
    wrong({"shared", "players", "0", "id"}, "5");
    wrong({"shared", "players", "0", "name"}, "5");
    wrong({"shared", "players", "0", "name_rev"}, "\"1\"");
    wrong({"shared", "players", "0", "removed"}, "0");
    wrong({"shared", "players", "0", "won_once"}, "\"yes\"");
    wrong({"shared", "players", "0", "best"}, "false");
    wrong({"shared", "players", "0", "coins"}, "{}");
    wrong({"shared", "players", "0", "coins"}, "[5]");
    wrong({"shared", "players", "0", "coins", "0", "device"}, "5");
    wrong({"shared", "players", "0", "coins", "0", "earned"}, "\"5\"");
    wrong({"shared", "players", "0", "coins", "0", "spent"}, "[5]");
    wrong({"shared", "top10"}, "{}");
    wrong({"shared", "top10"}, "[5]");
    wrong({"shared", "top10", "0", "name"}, "5");
    wrong({"shared", "top10", "0", "score"}, "\"5\"");
    wrong({"shared", "top10", "0", "device"}, "5");
    wrong({"shared", "top10", "0", "seq"}, "\"1\"");
}

TEST_CASE("a value out of range is damaged", "[unit][save]")
{
    const auto coins = [](std::string_view path_leaf, std::string_view value)
    {
        require_damaged(document_with({"shared", "players", "0", "coins", "0", path_leaf}, value),
                        SaveError::COINS_OUT_OF_RANGE);
    };
    for (const char* leaf : {"earned", "spent"})
    {
        coins(leaf, "-1");
        coins(leaf, "2147483648");
        coins(leaf, "-9223372036854775808");
        coins(leaf, "9223372036854775807");
    }

    const auto counter = [](std::initializer_list<std::string_view> path, std::string_view value)
    { require_damaged(document_with(path, value), SaveError::COUNTER_OUT_OF_RANGE); };
    for (const char* value : {"-1", "2147483648", "9223372036854775807", "-9223372036854775808"})
    {
        counter({"device", "next_seq"}, value);
        counter({"shared", "players", "0", "name_rev"}, value);
        counter({"shared", "top10", "0", "seq"}, value);
    }
    counter({"shared", "players", "0", "best"}, "-1");
    counter({"shared", "players", "0", "best"}, "-9223372036854775808");

    require_damaged(document_with({"version"}, "0"), SaveError::BAD_VERSION);
    require_damaged(document_with({"version"}, "-3"), SaveError::BAD_VERSION);
    require_damaged(document_with({"version"}, "-9223372036854775808"), SaveError::BAD_VERSION);
}

TEST_CASE("bad ids are damaged", "[unit][save]")
{
    SECTION("the device's own id")
    {
        for (const char* id : {"\"9F1C2A7B3D4E5F60\"", "\"9f1c2a7b3d4e5f6\"", "\"9f1c2a7b3d4e5f600\"",
                               "\"9f1c2a7b3d4e5f6g\"", "\"first\"", "\" 9f1c2a7b3d4e5f6\""})
        {
            DYNAMIC_SECTION(id)
            {
                require_damaged(document_with({"device", "id"}, id), SaveError::DEVICE_ID_INVALID);
            }
        }
        // Empty is allowed: it is the state before the app assigns an id.
        REQUIRE(parse(document_with({"device", "id"}, "\"\"")).status == LoadStatus::OK);
    }
    SECTION("the device of an entry")
    {
        require_damaged(document_with({"shared", "players", "0", "coins", "0", "device"}, "\"ABCDEF0123456789\""),
                        SaveError::DEVICE_ID_INVALID);
        require_damaged(document_with({"shared", "players", "0", "coins", "0", "device"}, "\"first\""),
                        SaveError::DEVICE_ID_INVALID);
        require_damaged(document_with({"shared", "top10", "0", "device"}, "\"xyz\""), SaveError::DEVICE_ID_INVALID);
        REQUIRE(parse(document_with({"shared", "top10", "0", "device"}, "\"\"")).status == LoadStatus::OK);
    }
    SECTION("a player id, the current player and the run's player")
    {
        for (const char* id :
             {"\"\"", "\"First\"", "\"firs\"", "\"A1B2C3D4E5F60718\"", "\"a1b2c3d4e5f6071\"", "\"a1b2c3d4e5f6071g\""})
        {
            DYNAMIC_SECTION(id)
            {
                require_damaged(document_with({"shared", "players", "1", "id"}, id), SaveError::PLAYER_ID_INVALID);
                require_damaged(document_with({"device", "current_player"}, id), SaveError::PLAYER_ID_INVALID);
                require_damaged(document_with({"device", "run_player"}, id), SaveError::RUN_PLAYER_MISSING);
            }
        }
    }
    SECTION("a repeated player id")
    {
        require_damaged(document_with({"shared", "players", "1", "id"}, "\"first\""), SaveError::PLAYER_ID_DUPLICATE);
        require_damaged(document_with({"shared", "players", "2", "id"}, "\"a1b2c3d4e5f60718\""),
                        SaveError::PLAYER_ID_DUPLICATE);
    }
    SECTION("a current player that names nobody is not a format error")
    {
        // `ensure_current_player` repairs it, so the file is valid.
        REQUIRE(parse(document_with({"device", "current_player"}, "\"ffffffffffffffff\"")).status == LoadStatus::OK);
        REQUIRE(parse(document_with({"device", "current_player"}, "\"00000000000000ff\"")).status == LoadStatus::OK);
    }
}

TEST_CASE("bad players are damaged", "[unit][save]")
{
    SECTION("names")
    {
        for (const char* name : {"\"abc\"", "\"ABCDEFGHIJKLM\"", "\"A1\"", "\"A-B\"", "\"\\u00c9\"", "\"A\\nB\""})
        {
            DYNAMIC_SECTION(name)
            {
                require_damaged(document_with({"shared", "players", "0", "name"}, name),
                                SaveError::PLAYER_NAME_INVALID);
            }
        }
        // An empty name is a player who has not been named yet.
        REQUIRE(parse(document_with({"shared", "players", "0", "name"}, "\"\"")).status == LoadStatus::OK);
        REQUIRE(parse(document_with({"shared", "players", "0", "name"}, "\"ABCDEFGHIJKL\"")).status == LoadStatus::OK);
    }
    SECTION("the length of the list is bounded, and more than max_players active is not an error")
    {
        const auto list = [](std::size_t count)
        {
            SaveData data;
            data.shared.players.clear();
            for (std::size_t i = 0; i < count; ++i)
            {
                data.shared.players.push_back(Player{.id = hex_id(i + 1)});
            }
            data.device.current_player = hex_id(1);
            return data;
        };
        REQUIRE(validate(list(static_cast<std::size_t>(Tuning::defaults().max_players) + 3)).has_value());
        REQUIRE(validate(list(MAX_SAVED_PLAYERS)).has_value());
        REQUIRE(validate(list(MAX_SAVED_PLAYERS + 1)) == std::unexpected(SaveError::PLAYERS_TOO_MANY));
        // And through the text: a list over the bound is refused before any player is read.
        const auto text = serialise(list(MAX_SAVED_PLAYERS)).value();
        REQUIRE(parse(text).status == LoadStatus::OK);
        std::string many = R"({"version":2,"device":{"id":"","sound":true,"music":true,"current_player":"first",)"
                           R"("next_seq":1},"shared":{"top10":[],"players":[)";
        for (std::size_t i = 0; i <= MAX_SAVED_PLAYERS; ++i)
        {
            many += i == 0 ? "5" : ",5";
        }
        many += "]}}";
        require_damaged(many, SaveError::PLAYERS_TOO_MANY);
    }
    SECTION("no player at all is a valid file")
    {
        const ParseResult result = parse(document_with({"shared", "players"}, "[]"));
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE(result.data.shared.players.empty());
    }
    SECTION("coin entries")
    {
        SaveData data = sample_data();
        auto& coins = data.shared.players[0].coins;
        coins.clear();
        for (std::size_t i = 0; i < MAX_SAVED_COIN_ENTRIES; ++i)
        {
            coins.push_back({hex_id(i + 1), 1, 0});
        }
        REQUIRE(validate(data).has_value());
        REQUIRE(parse(serialise(data).value()).data == data);
        coins.push_back({hex_id(1000), 1, 0});
        REQUIRE(validate(data) == std::unexpected(SaveError::COIN_ENTRIES_TOO_MANY));
        REQUIRE_FALSE(serialise(data).has_value());

        std::string text = R"({"version":2,"device":{"id":"","sound":true,"music":true,"current_player":"first",)"
                           R"("next_seq":1},"shared":{"top10":[],"players":[{"id":"first","name":"","name_rev":0,)"
                           R"("removed":false,"won_once":false,"best":0,"coins":[)";
        for (std::size_t i = 0; i <= MAX_SAVED_COIN_ENTRIES; ++i)
        {
            text += i == 0 ? "5" : ",5";
        }
        text += "]}]}}";
        require_damaged(text, SaveError::COIN_ENTRIES_TOO_MANY);

        // Two entries for one device are not one entry per device.
        SaveData twice = sample_data();
        twice.shared.players[0].coins = {{std::string(THIS_DEVICE), 1, 0}, {std::string(THIS_DEVICE), 2, 0}};
        REQUIRE(validate(twice) == std::unexpected(SaveError::COIN_DEVICE_DUPLICATE));
        twice.shared.players[0].coins = {{"", 1, 0}, {"", 2, 0}};
        REQUIRE(validate(twice) == std::unexpected(SaveError::COIN_DEVICE_DUPLICATE));
    }
}

TEST_CASE("a bad leaderboard is damaged", "[unit][save]")
{
    const auto top10_with = [](std::string_view value) { return document_with({"shared", "top10"}, value); };

    SECTION("names")
    {
        require_damaged(top10_with(R"([{"name":"","score":5,"device":"","seq":0}])"), SaveError::NAME_INVALID);
        require_damaged(top10_with(R"([{"name":"ABCDEFGHIJKLM","score":5,"device":"","seq":0}])"),
                        SaveError::NAME_INVALID);
        require_damaged(top10_with(R"([{"name":"ada","score":5,"device":"","seq":0}])"), SaveError::NAME_INVALID);
        require_damaged(top10_with(R"([{"name":"A1","score":5,"device":"","seq":0}])"), SaveError::NAME_INVALID);
        require_damaged(top10_with(R"([{"name":"A-B","score":5,"device":"","seq":0}])"), SaveError::NAME_INVALID);
        require_damaged(top10_with(R"([{"name":"A\nB","score":5,"device":"","seq":0}])"), SaveError::NAME_INVALID);
        require_damaged(top10_with(R"([{"name":"<script>","score":5,"device":"","seq":0}])"), SaveError::NAME_INVALID);
    }
    SECTION("scores")
    {
        require_damaged(top10_with(R"([{"name":"A","score":0,"device":"","seq":0}])"), SaveError::SCORE_INVALID);
        require_damaged(top10_with(R"([{"name":"A","score":-5,"device":"","seq":0}])"), SaveError::SCORE_INVALID);
        require_damaged(top10_with(R"([{"name":"A","score":-9223372036854775808,"device":"","seq":0}])"),
                        SaveError::SCORE_INVALID);
    }
    SECTION("order and length")
    {
        require_damaged(top10_with(R"([{"name":"A","score":5,"device":"","seq":0},)"
                                   R"({"name":"B","score":6,"device":"","seq":1}])"),
                        SaveError::TOP10_NOT_SORTED);
        // Of equal scores the format checks no order: not by device, not by seq.
        REQUIRE(parse(top10_with(R"([{"name":"A","score":5,"device":"","seq":9},)"
                                 R"({"name":"B","score":5,"device":"","seq":1}])"))
                    .status == LoadStatus::OK);
        std::string eleven = "[";
        for (int i = 0; i < 11; ++i)
        {
            eleven += i == 0 ? "" : ",";
            eleven += R"({"name":"A","score":5,"device":"","seq":)" + std::to_string(i) + "}";
        }
        eleven += "]";
        require_damaged(top10_with(eleven), SaveError::TOP10_TOO_LONG);
        // Exactly ten is fine.
        const std::string ten = eleven.substr(0, eleven.rfind(",{")) + "]";
        REQUIRE(parse(top10_with(ten)).status == LoadStatus::OK);
    }
}

TEST_CASE("a saved run that is too long, or has no player, is damaged", "[unit][save]")
{
    std::string run = "[";
    for (std::size_t i = 0; i <= MAX_SAVED_RUN_VALUES; ++i)
    {
        run += i == 0 ? "0" : ",0";
    }
    run += "]";
    require_damaged(document_with({"device", "run"}, run), SaveError::RUN_TOO_LONG);

    std::string fits = "[";
    for (std::size_t i = 0; i < MAX_SAVED_RUN_VALUES; ++i)
    {
        fits += i == 0 ? "0" : ",0";
    }
    fits += "]";
    REQUIRE(parse(document_with({"device", "run"}, fits)).status == LoadStatus::OK);

    // The run and its player go together.
    require_damaged(document_without({"device", "run_player"}), SaveError::RUN_PLAYER_MISSING);
    require_damaged(document_without({"device", "run"}), SaveError::RUN_PLAYER_MISSING);
    const ParseResult neither =
        parse(edited_text(document_without({"device", "run"}), {"device", "run_player"}, std::nullopt));
    REQUIRE(neither.status == LoadStatus::OK);
    REQUIRE_FALSE(neither.data.device.run.has_value());
    REQUIRE(neither.data.device.run_player.empty());
}

TEST_CASE("a save from a newer build is reported and never read", "[unit][save]")
{
    for (const char* text : {R"({"version":3})", R"({"version":3,"device":"whatever","new_field":[1,2]})",
                             R"({"version":9223372036854775807,"anything":true})"})
    {
        DYNAMIC_SECTION(text)
        {
            const ParseResult result = parse(text);
            REQUIRE(result.status == LoadStatus::NEWER);
            REQUIRE_FALSE(result.damage.has_value());
            REQUIRE(result.data == SaveData{});
            REQUIRE(result.file_version.has_value());
            REQUIRE(*result.file_version > 2);
        }
    }
    // Newer is decided by the version alone, but a document that is not JSON at all is still just damaged.
    require_damaged(R"({"version":3)", SaveError::NOT_JSON);
}

TEST_CASE("serialise refuses data that the next load would call damaged", "[unit][save]")
{
    const auto breaks = [](auto&& change, SaveError reason)
    {
        SaveData data = sample_data();
        change(data);
        const auto text = serialise(data);
        REQUIRE_FALSE(text.has_value());
        REQUIRE(text.error() == reason);
    };
    breaks([](SaveData& d) { d.version = 1; }, SaveError::BAD_VERSION);
    breaks([](SaveData& d) { d.version = 3; }, SaveError::BAD_VERSION);
    breaks([](SaveData& d) { d.device.id = "XYZ"; }, SaveError::DEVICE_ID_INVALID);
    breaks([](SaveData& d) { d.device.current_player = ""; }, SaveError::PLAYER_ID_INVALID);
    breaks([](SaveData& d) { d.device.next_seq = -1; }, SaveError::COUNTER_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.device.next_seq = MAX_SAVED_COUNTER + 1; }, SaveError::COUNTER_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.device.run = std::vector<std::int64_t>(MAX_SAVED_RUN_VALUES + 1, 0); },
           SaveError::RUN_TOO_LONG);
    breaks([](SaveData& d) { d.device.run_player.clear(); }, SaveError::RUN_PLAYER_MISSING);
    breaks([](SaveData& d) { d.device.run.reset(); }, SaveError::RUN_PLAYER_MISSING); // the player is still set
    breaks([](SaveData& d) { d.shared.players[0].coins[0].earned = -1; }, SaveError::COINS_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.shared.players[0].coins[0].spent = MAX_SAVED_COINS + 1; },
           SaveError::COINS_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.shared.players[0].name = "lower"; }, SaveError::PLAYER_NAME_INVALID);
    breaks([](SaveData& d) { d.shared.players[0].name = std::string(13, 'A'); }, SaveError::PLAYER_NAME_INVALID);
    breaks([](SaveData& d) { d.shared.players[0].name_rev = -1; }, SaveError::COUNTER_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.shared.players[0].best = -1; }, SaveError::COUNTER_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.shared.players[1].id = "first"; }, SaveError::PLAYER_ID_DUPLICATE);
    breaks([](SaveData& d) { d.shared.top10[0].name = "lower"; }, SaveError::NAME_INVALID);
    breaks([](SaveData& d) { d.shared.top10[0].name = std::string(13, 'A'); }, SaveError::NAME_INVALID);
    breaks([](SaveData& d) { d.shared.top10[0].name.clear(); }, SaveError::NAME_INVALID);
    breaks([](SaveData& d) { d.shared.top10[0].score = 0; }, SaveError::SCORE_INVALID);
    breaks([](SaveData& d) { d.shared.top10[3].score = 9999; }, SaveError::TOP10_NOT_SORTED);
    breaks([](SaveData& d) { d.shared.top10[0].device = "nope"; }, SaveError::DEVICE_ID_INVALID);
    breaks([](SaveData& d) { d.shared.top10[0].seq = -1; }, SaveError::COUNTER_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.shared.top10.assign(11, ScoreEntry{"A", 1, "", 0}); }, SaveError::TOP10_TOO_LONG);
}

// Test-only migrations after the real one. Version 3 stands for a future build that turned a switch on, and a step that
// fails stands for a document that does not have the shape its version should.
namespace
{

bool fake_turn_game_center_on(json::Value& document)
{
    const json::Value* device = document.find("device");
    if (device == nullptr)
    {
        return false;
    }
    json::Value copy = *device;
    copy.set("game_center", json::Value::boolean(true));
    return document.set("device", std::move(copy));
}

bool fake_always_fails(json::Value&)
{
    return false;
}

constexpr std::array<MigrationStep, 2> FAKE_STEPS = {migrate_save_v1_to_v2, fake_turn_game_center_on};
constexpr std::array<MigrationStep, 2> MISSING_STEP = {migrate_save_v1_to_v2, nullptr};
constexpr std::array<MigrationStep, 2> FAILING_STEP = {migrate_save_v1_to_v2, fake_always_fails};
const SaveSchema FAKE_SCHEMA{3, FAKE_STEPS};

constexpr std::string_view OLD_V1 =
    R"({"version":1,"coins":50,"won_once":false,"sound":true,"music":false,"top10":[]})";

std::string v2_text()
{
    return serialise(SaveData{}).value();
}

} // namespace

TEST_CASE("migrations run in order from the file's version to the current one", "[unit][save][migration]")
{
    SECTION("from version 1 through both steps")
    {
        const ParseResult result = parse_save(OLD_V1, Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE(result.file_version == 1);
        REQUIRE(result.data.version == 3);
        REQUIRE(player_wallet(result.data.shared.players[0]) == 50);
        REQUIRE_FALSE(result.data.device.music);
        REQUIRE(result.data.device.game_center); // set by the second step
    }
    SECTION("from version 2 through the last step only")
    {
        const ParseResult result = parse_save(v2_text(), Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE(result.file_version == 2);
        REQUIRE(result.data.version == 3);
        REQUIRE(result.data.device.game_center);
    }
    SECTION("a file already at the current version is not migrated")
    {
        const std::string v3 = edited_text(v2_text(), {"version"}, "3");
        const ParseResult result = parse_save(v3, Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE(result.data.version == 3);
        REQUIRE_FALSE(result.data.device.game_center);
    }
    SECTION("a version 4 file is newer than this schema")
    {
        REQUIRE(parse_save(R"({"version":4})", Tuning::defaults(), FAKE_SCHEMA).status == LoadStatus::NEWER);
    }
    SECTION("a version 3 file is newer to the release build")
    {
        REQUIRE(parse(edited_text(v2_text(), {"version"}, "3")).status == LoadStatus::NEWER);
    }
    SECTION("what a migrated save writes back reads as the current version")
    {
        const ParseResult result = parse_save(OLD_V1, Tuning::defaults(), FAKE_SCHEMA);
        const auto text = serialise(result.data, Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(text.has_value());
        REQUIRE(text->find(R"("version":3)") != std::string::npos);
        REQUIRE(parse_save(*text, Tuning::defaults(), FAKE_SCHEMA).data == result.data);
        // And the release build, which knows version 2, treats it as a newer file.
        REQUIRE(parse(*text).status == LoadStatus::NEWER);
    }
}

TEST_CASE("a migration that fails damages the save", "[unit][save][migration]")
{
    SECTION("the step reports the document is the wrong shape")
    {
        const SaveSchema failing{3, FAILING_STEP};
        const ParseResult result = parse_save(v2_text(), Tuning::defaults(), failing);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        REQUIRE(result.damage == SaveError::MIGRATION_FAILED);
        REQUIRE(result.file_version == 2);
    }
    SECTION("a missing step")
    {
        const SaveSchema broken{3, MISSING_STEP};
        const ParseResult result = parse_save(OLD_V1, Tuning::defaults(), broken);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        REQUIRE(result.damage == SaveError::MIGRATION_FAILED);
    }
    SECTION("a schema with fewer steps than versions")
    {
        const SaveSchema short_schema{3, std::span<const MigrationStep>(FAKE_STEPS).first(1)};
        const ParseResult result = parse_save(OLD_V1, Tuning::defaults(), short_schema);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        REQUIRE(result.damage == SaveError::MIGRATION_FAILED);
    }
    SECTION("the real step refuses what is not an object")
    {
        json::Value not_an_object = json::Value::array();
        REQUIRE_FALSE(migrate_save_v1_to_v2(not_an_object));
    }
}

TEST_CASE("the release schema ships the step from version 1", "[unit][save][migration]")
{
    const SaveSchema release = SaveSchema::release();
    REQUIRE(release.current_version == SAVE_VERSION);
    REQUIRE(release.current_version == 2);
    REQUIRE(release.steps.size() == 1);
    REQUIRE(release.steps[0] == migrate_save_v1_to_v2);
}

TEST_CASE("names are normalised and checked", "[unit][save][scores]")
{
    REQUIRE(is_valid_name("A"));
    REQUIRE(is_valid_name("ABCDEFGHIJKL"));
    REQUIRE(is_valid_name("A B"));
    REQUIRE(is_valid_name(" "));
    REQUIRE_FALSE(is_valid_name(""));
    REQUIRE_FALSE(is_valid_name("ABCDEFGHIJKLM"));
    REQUIRE_FALSE(is_valid_name("abc"));
    REQUIRE_FALSE(is_valid_name("A1"));

    REQUIRE(normalise_name("ada") == "ADA");
    REQUIRE(normalise_name("a d-a 9!") == "A DA ");
    REQUIRE(normalise_name("abcdefghijklmnop") == "ABCDEFGHIJKL");
    REQUIRE(normalise_name("\xC3\xA9") == "");
    REQUIRE(normalise_name("") == "");
}

TEST_CASE("ids are 16 lowercase hex digits, or `first` for a player", "[unit][save][ids]")
{
    REQUIRE(is_valid_device_id("0123456789abcdef"));
    REQUIRE(is_valid_device_id("ffffffffffffffff"));
    REQUIRE_FALSE(is_valid_device_id(""));
    REQUIRE_FALSE(is_valid_device_id("first"));
    REQUIRE_FALSE(is_valid_device_id("0123456789ABCDEF"));
    REQUIRE_FALSE(is_valid_device_id("0123456789abcde"));
    REQUIRE_FALSE(is_valid_device_id("0123456789abcdef0"));
    REQUIRE_FALSE(is_valid_device_id("0123456789abcdeg"));
    REQUIRE_FALSE(is_valid_device_id(std::string_view("0123456789abcde\0", 16)));

    REQUIRE(is_valid_player_id("first"));
    REQUIRE(is_valid_player_id("0123456789abcdef"));
    REQUIRE_FALSE(is_valid_player_id(""));
    REQUIRE_FALSE(is_valid_player_id("First"));
    REQUIRE_FALSE(is_valid_player_id("firstt"));
    REQUIRE_FALSE(is_valid_player_id("0123456789ABCDEF"));

    REQUIRE(hex_id(0) == "0000000000000000");
    REQUIRE(hex_id(0x9f1c2a7b3d4e5f60ULL) == "9f1c2a7b3d4e5f60");
    REQUIRE(hex_id(std::numeric_limits<std::uint64_t>::max()) == "ffffffffffffffff");
    REQUIRE(hex_id(255) == "00000000000000ff");
    REQUIRE(is_valid_device_id(hex_id(0x1234567890abcdefULL)));
}

TEST_CASE("a score ranks by the SPEC leaderboard rules", "[unit][save][scores]")
{
    const Tuning& tuning = Tuning::defaults();
    std::vector<ScoreEntry> top10;

    SECTION("an empty list ranks any score above zero first")
    {
        REQUIRE(rank_for_score(top10, 1) == 1);
        REQUIRE(rank_for_score(top10, 500) == 1);
        REQUIRE_FALSE(rank_for_score(top10, 0).has_value());
        REQUIRE_FALSE(rank_for_score(top10, -5).has_value());
        REQUIRE(best_score(top10) == 0);
    }
    SECTION("equal scores rank behind the older entry")
    {
        top10 = {{"A", 900}, {"B", 500}, {"C", 500}, {"D", 100}};
        REQUIRE(rank_for_score(top10, 1000) == 1);
        REQUIRE(rank_for_score(top10, 900) == 2);
        REQUIRE(rank_for_score(top10, 700) == 2);
        REQUIRE(rank_for_score(top10, 500) == 4);
        REQUIRE(rank_for_score(top10, 100) == 5);
        REQUIRE(rank_for_score(top10, 99) == 5);
        REQUIRE(best_score(top10) == 900);
    }
    SECTION("a full list takes a score only if it beats the tenth")
    {
        for (int i = 0; i < tuning.top10_size; ++i)
        {
            top10.push_back({"A", 1000 - i * 100});
        }
        REQUIRE(top10.back().score == 100);
        REQUIRE_FALSE(rank_for_score(top10, 100).has_value()); // equal to the tenth: the older one stays
        REQUIRE_FALSE(rank_for_score(top10, 50).has_value());
        REQUIRE(rank_for_score(top10, 101) == 10);
        REQUIRE(rank_for_score(top10, 2000) == 1);
    }
    SECTION("a score of zero never qualifies, even with room")
    {
        top10 = {{"A", 5}};
        REQUIRE_FALSE(rank_for_score(top10, 0).has_value());
    }
    SECTION("the extremes of 64 bits")
    {
        top10 = {{"A", INT64_MAX_VALUE}};
        REQUIRE(rank_for_score(top10, INT64_MAX_VALUE) == 2);
        REQUIRE(rank_for_score(top10, INT64_MIN_VALUE) == std::nullopt);
    }
}

TEST_CASE("inserting a score keeps the list in order and at ten", "[unit][save][scores]")
{
    const Tuning& tuning = Tuning::defaults();
    std::vector<ScoreEntry> top10;

    REQUIRE(insert_score(top10, {"ADA", 500, "", 1}) == 1);
    REQUIRE(insert_score(top10, {"BEN", 900, "", 2}) == 1);
    REQUIRE(insert_score(top10, {"CY", 500, "", 3}) == 3); // behind the older 500
    REQUIRE(insert_score(top10, {"DI", 700, "", 4}) == 2);
    REQUIRE(top10 ==
            std::vector<ScoreEntry>{{"BEN", 900, "", 2}, {"DI", 700, "", 4}, {"ADA", 500, "", 1}, {"CY", 500, "", 3}});
    SaveData with_top10;
    with_top10.shared.top10 = top10;
    REQUIRE(validate(with_top10).has_value());

    SECTION("a refused score leaves the list alone")
    {
        const auto before = top10;
        REQUIRE(insert_score(top10, {"EVE", 0}) == std::unexpected(ScoreError::NOT_QUALIFYING));
        REQUIRE(insert_score(top10, {"eve", 600}) == std::unexpected(ScoreError::NAME_INVALID));
        REQUIRE(insert_score(top10, {"", 600}) == std::unexpected(ScoreError::NAME_INVALID));
        REQUIRE(insert_score(top10, {"ABCDEFGHIJKLM", 600}) == std::unexpected(ScoreError::NAME_INVALID));
        REQUIRE(top10 == before);
    }
    SECTION("the entry pushed past ten drops off")
    {
        for (int i = 0; i < 6; ++i)
        {
            REQUIRE(insert_score(top10, {"X", 100 + i}).has_value());
        }
        REQUIRE(top10.size() == static_cast<std::size_t>(tuning.top10_size));
        REQUIRE(top10.back().score == 100);
        REQUIRE(insert_score(top10, {"TOP", 5000}) == 1);
        REQUIRE(top10.size() == static_cast<std::size_t>(tuning.top10_size));
        REQUIRE(top10.front().name == "TOP");
        REQUIRE(top10.back().score == 101); // the first 100 fell off
        REQUIRE(insert_score(top10, {"LOW", 101}) == std::unexpected(ScoreError::NOT_QUALIFYING));
        SaveData full_list;
        full_list.shared.top10 = top10;
        REQUIRE(validate(full_list).has_value());
    }
}
