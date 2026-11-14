// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/tuning.hpp"
#include "save_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

using namespace heart_boom_maths;

namespace
{

constexpr auto INT64_MAX_VALUE = std::numeric_limits<std::int64_t>::max();
constexpr auto INT64_MIN_VALUE = std::numeric_limits<std::int64_t>::min();

SaveData sample_data()
{
    SaveData data;
    data.coins = 250;
    data.won_once = true;
    data.sound = false;
    data.music = true;
    data.game_center = true;
    data.haptics = false;
    data.top10 = {{"ADA", 900}, {"BEN", 500}, {"CY", 500}, {"DI", 1}};
    data.run = std::vector<std::int64_t>{1, 2, 3, -4};
    return data;
}

ParseResult parse(std::string_view text)
{
    return parse_save(text);
}

// A complete valid document with one field replaced, so each damaged case changes exactly one thing.
std::string document_with(std::string_view key, std::string_view value_text)
{
    auto parsed = json::parse(serialise(sample_data()).value());
    REQUIRE(parsed.has_value());
    auto replacement = json::parse(value_text);
    REQUIRE(replacement.has_value());
    parsed->set(std::string(key), std::move(*replacement));
    return json::write(*parsed);
}

std::string document_without(std::string_view key)
{
    auto parsed = json::parse(serialise(sample_data()).value());
    REQUIRE(parsed.has_value());
    parsed->erase(key);
    return json::write(*parsed);
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
    REQUIRE(data.version == 1);
    REQUIRE(data.coins == 0);
    REQUIRE_FALSE(data.won_once);
    REQUIRE(data.sound);
    REQUIRE(data.music);
    REQUIRE_FALSE(data.game_center);
    REQUIRE(data.haptics);
    REQUIRE(data.top10.empty());
    REQUIRE_FALSE(data.run.has_value());
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
    REQUIRE(result.file_version == 1);
    REQUIRE(serialise(result.data).value() == *text);
}

TEST_CASE("the default save and a save without a run round-trip", "[unit][save]")
{
    const auto text = serialise(SaveData{});
    REQUIRE(text.has_value());
    REQUIRE(
        *text ==
        R"({"coins":0,"game_center":false,"haptics":true,"music":true,"sound":true,"top10":[],"version":1,"won_once":false})");
    REQUIRE(parse(*text).data == SaveData{});
}

TEST_CASE("the serialised form is stable", "[unit][save]")
{
    const auto text = serialise(sample_data());
    REQUIRE(text.has_value());
    REQUIRE(*text == R"({"coins":250,"game_center":true,"haptics":false,"music":true,"run":[1,2,3,-4],"sound":false,)"
                     R"("top10":[{"name":"ADA","score":900},{"name":"BEN","score":500},{"name":"CY","score":500},)"
                     R"({"name":"DI","score":1}],"version":1,"won_once":true})");
}

TEST_CASE("a save holds 64-bit extremes", "[unit][save]")
{
    SaveData data;
    data.coins = MAX_SAVED_COINS;
    data.top10 = {{"A", INT64_MAX_VALUE}, {"B", INT64_MAX_VALUE}, {"C", 1}};
    data.run = std::vector<std::int64_t>{INT64_MIN_VALUE, INT64_MAX_VALUE, 0, -1};
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    REQUIRE(parse(*text).data == data);
}

TEST_CASE("the version 1 fixture file loads", "[unit][save]")
{
    const ParseResult result = parse(SAVE_V1_FIXTURE);
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE(result.file_version == 1);
    REQUIRE(result.data.version == 1);
    REQUIRE(result.data.coins == 137);
    REQUIRE(result.data.won_once);
    REQUIRE(result.data.sound);
    REQUIRE_FALSE(result.data.music);
    REQUIRE_FALSE(result.data.game_center);
    // The fixture predates the haptics switch, so it reads as the default.
    REQUIRE(result.data.haptics);
    REQUIRE(result.data.top10.size() == 4);
    REQUIRE(result.data.top10[0] == ScoreEntry{"ADA", 48210});
    REQUIRE(result.data.top10[1] == ScoreEntry{"BEN", 31000});
    REQUIRE(result.data.top10[2] == ScoreEntry{"CY", 31000});
    REQUIRE(result.data.top10[3] == ScoreEntry{"DI", 120});
    REQUIRE(result.data.run.has_value());
    REQUIRE(result.data.run->size() == 12);
    REQUIRE(result.data.run->back() == INT64_MAX_VALUE);
    // A build that writes this fixture back out must read its own output as the same save.
    REQUIRE(parse(serialise(result.data).value()).data == result.data);
}

TEST_CASE("game_center is optional on reading and defaults to off", "[unit][save]")
{
    const ParseResult result = parse(document_without("game_center"));
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE_FALSE(result.data.game_center);
}

TEST_CASE("haptics is optional on reading and defaults to on", "[unit][save]")
{
    const ParseResult result = parse(document_without("haptics"));
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE(result.data.haptics);
}

TEST_CASE("haptics reads back off and on, and the writer always writes it", "[unit][save]")
{
    const ParseResult off = parse(document_with("haptics", "false"));
    REQUIRE(off.status == LoadStatus::OK);
    REQUIRE_FALSE(off.data.haptics);
    const ParseResult on = parse(document_with("haptics", "true"));
    REQUIRE(on.status == LoadStatus::OK);
    REQUIRE(on.data.haptics);

    SaveData data;
    data.haptics = false;
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    REQUIRE(text->find(R"("haptics":false)") != std::string::npos);
    REQUIRE_FALSE(parse(*text).data.haptics);
}

TEST_CASE("unknown members are ignored", "[unit][save]")
{
    const ParseResult result = parse(document_with("something_new", R"({"a":[1,2]})"));
    REQUIRE(result.status == LoadStatus::OK);
    REQUIRE(result.data == sample_data());
}

TEST_CASE("text that is not a save is damaged", "[unit][save]")
{
    require_damaged("", SaveError::NOT_JSON);
    require_damaged("   ", SaveError::NOT_JSON);
    require_damaged("not json", SaveError::NOT_JSON);
    require_damaged("{", SaveError::NOT_JSON);
    require_damaged(R"({"version":1,"coins":1.5})", SaveError::NOT_JSON);
    require_damaged(R"({"version":1,"coins":null})", SaveError::NOT_JSON);
    require_damaged(R"({"version":1} trailing)", SaveError::NOT_JSON);
    require_damaged(R"({"version":1,"version":1})", SaveError::NOT_JSON);
    require_damaged(std::string(sample_data().run->size(), '\0'), SaveError::NOT_JSON);
    require_damaged(std::string("\xFF\xFE\x00\x01", 4), SaveError::NOT_JSON);
    require_damaged(std::string(300 * 1024, ' '), SaveError::NOT_JSON);
    require_damaged("[]", SaveError::NOT_AN_OBJECT);
    require_damaged("5", SaveError::NOT_AN_OBJECT);
    require_damaged("\"save\"", SaveError::NOT_AN_OBJECT);
    require_damaged("true", SaveError::NOT_AN_OBJECT);
}

TEST_CASE("a missing field is damaged", "[unit][save]")
{
    for (const char* key : {"version", "coins", "won_once", "sound", "music", "top10"})
    {
        DYNAMIC_SECTION(key)
        {
            require_damaged(document_without(key), SaveError::MISSING_FIELD);
        }
    }
    require_damaged(R"({"coins":1})", SaveError::MISSING_FIELD);
    require_damaged(document_with("top10", R"([{"score":5}])"), SaveError::MISSING_FIELD);
    require_damaged(document_with("top10", R"([{"name":"A"}])"), SaveError::MISSING_FIELD);
}

TEST_CASE("a field of the wrong type is damaged", "[unit][save]")
{
    require_damaged(document_with("version", "\"1\""), SaveError::WRONG_TYPE);
    require_damaged(document_with("version", "true"), SaveError::WRONG_TYPE);
    require_damaged(document_with("coins", "\"5\""), SaveError::WRONG_TYPE);
    require_damaged(document_with("coins", "true"), SaveError::WRONG_TYPE);
    require_damaged(document_with("coins", "[5]"), SaveError::WRONG_TYPE);
    require_damaged(document_with("won_once", "1"), SaveError::WRONG_TYPE);
    require_damaged(document_with("sound", "\"on\""), SaveError::WRONG_TYPE);
    require_damaged(document_with("music", "0"), SaveError::WRONG_TYPE);
    require_damaged(document_with("game_center", "1"), SaveError::WRONG_TYPE);
    require_damaged(document_with("haptics", "1"), SaveError::WRONG_TYPE);
    require_damaged(document_with("haptics", "\"on\""), SaveError::WRONG_TYPE);
    require_damaged(document_with("top10", "{}"), SaveError::WRONG_TYPE);
    require_damaged(document_with("top10", "[5]"), SaveError::WRONG_TYPE);
    require_damaged(document_with("top10", R"([{"name":5,"score":5}])"), SaveError::WRONG_TYPE);
    require_damaged(document_with("top10", R"([{"name":"A","score":"5"}])"), SaveError::WRONG_TYPE);
    require_damaged(document_with("run", "5"), SaveError::WRONG_TYPE);
    require_damaged(document_with("run", R"([1,"2"])"), SaveError::WRONG_TYPE);
    require_damaged(document_with("run", "[true]"), SaveError::WRONG_TYPE);
    require_damaged(document_with("run", "[[1]]"), SaveError::WRONG_TYPE);
}

TEST_CASE("a value out of range is damaged", "[unit][save]")
{
    require_damaged(document_with("coins", "-1"), SaveError::COINS_OUT_OF_RANGE);
    require_damaged(document_with("coins", "2147483648"), SaveError::COINS_OUT_OF_RANGE);
    require_damaged(document_with("coins", "-9223372036854775808"), SaveError::COINS_OUT_OF_RANGE);
    require_damaged(document_with("coins", "9223372036854775807"), SaveError::COINS_OUT_OF_RANGE);
    require_damaged(document_with("version", "0"), SaveError::BAD_VERSION);
    require_damaged(document_with("version", "-3"), SaveError::BAD_VERSION);
    require_damaged(document_with("version", "-9223372036854775808"), SaveError::BAD_VERSION);
}

TEST_CASE("a bad leaderboard is damaged", "[unit][save]")
{
    SECTION("names")
    {
        require_damaged(document_with("top10", R"([{"name":"","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"ABCDEFGHIJKLM","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"ada","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"A1","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"A-B","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"A\nB","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"É","score":5}])"), SaveError::NAME_INVALID);
        require_damaged(document_with("top10", R"([{"name":"<script>","score":5}])"), SaveError::NAME_INVALID);
    }
    SECTION("scores")
    {
        require_damaged(document_with("top10", R"([{"name":"A","score":0}])"), SaveError::SCORE_INVALID);
        require_damaged(document_with("top10", R"([{"name":"A","score":-5}])"), SaveError::SCORE_INVALID);
        require_damaged(document_with("top10", R"([{"name":"A","score":-9223372036854775808}])"),
                        SaveError::SCORE_INVALID);
    }
    SECTION("order and length")
    {
        require_damaged(document_with("top10", R"([{"name":"A","score":5},{"name":"B","score":6}])"),
                        SaveError::TOP10_NOT_SORTED);
        std::string eleven = "[";
        for (int i = 0; i < 11; ++i)
        {
            eleven += i == 0 ? "" : ",";
            eleven += R"({"name":"A","score":5})";
        }
        eleven += "]";
        require_damaged(document_with("top10", eleven), SaveError::TOP10_TOO_LONG);
        // Exactly ten is fine.
        const std::string ten =
            eleven.substr(0, eleven.size() - std::string(R"(,{"name":"A","score":5}])").size()) + "]";
        REQUIRE(parse(document_with("top10", ten)).status == LoadStatus::OK);
    }
}

TEST_CASE("a saved run that is too long is damaged", "[unit][save]")
{
    std::string run = "[";
    for (std::size_t i = 0; i <= MAX_SAVED_RUN_VALUES; ++i)
    {
        run += i == 0 ? "0" : ",0";
    }
    run += "]";
    require_damaged(document_with("run", run), SaveError::RUN_TOO_LONG);

    std::string fits = "[";
    for (std::size_t i = 0; i < MAX_SAVED_RUN_VALUES; ++i)
    {
        fits += i == 0 ? "0" : ",0";
    }
    fits += "]";
    REQUIRE(parse(document_with("run", fits)).status == LoadStatus::OK);
}

TEST_CASE("a save from a newer build is reported and never read", "[unit][save]")
{
    for (const char* text : {R"({"version":2})", R"({"version":2,"coins":"whatever","new_field":[1,2]})",
                             R"({"version":9223372036854775807,"anything":true})"})
    {
        DYNAMIC_SECTION(text)
        {
            const ParseResult result = parse(text);
            REQUIRE(result.status == LoadStatus::NEWER);
            REQUIRE_FALSE(result.damage.has_value());
            REQUIRE(result.data == SaveData{});
            REQUIRE(result.file_version.has_value());
            REQUIRE(*result.file_version > 1);
        }
    }
    // Newer is decided by the version alone, but a document that is not JSON at all is still just damaged.
    require_damaged(R"({"version":2)", SaveError::NOT_JSON);
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
    breaks([](SaveData& d) { d.version = 2; }, SaveError::BAD_VERSION);
    breaks([](SaveData& d) { d.coins = -1; }, SaveError::COINS_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.coins = MAX_SAVED_COINS + 1; }, SaveError::COINS_OUT_OF_RANGE);
    breaks([](SaveData& d) { d.top10[0].name = "lower"; }, SaveError::NAME_INVALID);
    breaks([](SaveData& d) { d.top10[0].name = std::string(13, 'A'); }, SaveError::NAME_INVALID);
    breaks([](SaveData& d) { d.top10[0].name.clear(); }, SaveError::NAME_INVALID);
    breaks([](SaveData& d) { d.top10[0].score = 0; }, SaveError::SCORE_INVALID);
    breaks([](SaveData& d) { d.top10[3].score = 9999; }, SaveError::TOP10_NOT_SORTED);
    breaks([](SaveData& d) { d.top10.assign(11, ScoreEntry{"A", 1}); }, SaveError::TOP10_TOO_LONG);
    breaks([](SaveData& d) { d.run = std::vector<std::int64_t>(MAX_SAVED_RUN_VALUES + 1, 0); },
           SaveError::RUN_TOO_LONG);
}

// Test-only migrations. The release schema has none; these stand for a future version 2 that renamed `coins` to `gold`
// and a version 3 that added a switch.
namespace
{

bool fake_rename_gold_to_coins(json::Value& document)
{
    const json::Value* gold = document.find("gold");
    if (gold == nullptr)
    {
        return false;
    }
    json::Value moved = *gold;
    document.erase("gold");
    document.set("coins", std::move(moved));
    return true;
}

bool fake_turn_game_center_on(json::Value& document)
{
    return document.set("game_center", json::Value::boolean(true));
}

constexpr std::array<MigrationStep, 2> FAKE_STEPS = {fake_rename_gold_to_coins, fake_turn_game_center_on};
constexpr std::array<MigrationStep, 2> BROKEN_STEPS = {fake_rename_gold_to_coins, nullptr};
const SaveSchema FAKE_SCHEMA{3, FAKE_STEPS};

constexpr std::string_view OLD_V1 = R"({"version":1,"gold":50,"won_once":false,"sound":true,"music":false,"top10":[]})";
constexpr std::string_view OLD_V2 = R"({"version":2,"coins":7,"won_once":true,"sound":true,"music":true,"top10":[]})";

} // namespace

TEST_CASE("migrations run in order from the file's version to the current one", "[unit][save][migration]")
{
    SECTION("from version 1 through both steps")
    {
        const ParseResult result = parse_save(OLD_V1, Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE(result.file_version == 1);
        REQUIRE(result.data.version == 3);
        REQUIRE(result.data.coins == 50);
        REQUIRE_FALSE(result.data.music);
        REQUIRE(result.data.game_center); // set by the second step
    }
    SECTION("from version 2 through the last step only")
    {
        const ParseResult result = parse_save(OLD_V2, Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE(result.data.version == 3);
        REQUIRE(result.data.coins == 7);
        REQUIRE(result.data.game_center);
    }
    SECTION("a file already at the current version is not migrated")
    {
        const ParseResult result = parse_save(
            R"({"version":3,"coins":9,"won_once":false,"sound":true,"music":true,"game_center":false,"top10":[]})",
            Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::OK);
        REQUIRE_FALSE(result.data.game_center);
    }
    SECTION("a version 4 file is newer than this schema")
    {
        REQUIRE(parse_save(R"({"version":4})", Tuning::defaults(), FAKE_SCHEMA).status == LoadStatus::NEWER);
    }
    SECTION("the same version 2 file is newer to the release build")
    {
        REQUIRE(parse(OLD_V2).status == LoadStatus::NEWER);
    }
    SECTION("what a migrated save writes back reads as the current version")
    {
        const ParseResult result = parse_save(OLD_V1, Tuning::defaults(), FAKE_SCHEMA);
        const auto text = serialise(result.data, Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(text.has_value());
        REQUIRE(text->find(R"("version":3)") != std::string::npos);
        REQUIRE(parse_save(*text, Tuning::defaults(), FAKE_SCHEMA).data == result.data);
        // And a build that only knows version 1 treats it as a newer file.
        REQUIRE(parse(*text).status == LoadStatus::NEWER);
    }
}

TEST_CASE("a migration that fails damages the save", "[unit][save][migration]")
{
    SECTION("the step reports the document is the wrong shape")
    {
        const ParseResult result =
            parse_save(R"({"version":1,"coins":1,"won_once":false,"sound":true,"music":true,"top10":[]})",
                       Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        REQUIRE(result.damage == SaveError::MIGRATION_FAILED);
        REQUIRE(result.file_version == 1);
    }
    SECTION("a missing step")
    {
        const SaveSchema broken{3, BROKEN_STEPS};
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
    SECTION("the migrated document is still validated")
    {
        const ParseResult result =
            parse_save(R"({"version":1,"gold":-5,"won_once":false,"sound":true,"music":true,"top10":[]})",
                       Tuning::defaults(), FAKE_SCHEMA);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        REQUIRE(result.damage == SaveError::COINS_OUT_OF_RANGE);
    }
}

TEST_CASE("the release schema ships no migrations", "[unit][save][migration]")
{
    const SaveSchema release = SaveSchema::release();
    REQUIRE(release.current_version == SAVE_VERSION);
    REQUIRE(release.steps.empty());
    REQUIRE(SAVE_VERSION == 1);
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

    REQUIRE(insert_score(top10, "ADA", 500) == 1);
    REQUIRE(insert_score(top10, "BEN", 900) == 1);
    REQUIRE(insert_score(top10, "CY", 500) == 3); // behind the older 500
    REQUIRE(insert_score(top10, "DI", 700) == 2);
    REQUIRE(top10 == std::vector<ScoreEntry>{{"BEN", 900}, {"DI", 700}, {"ADA", 500}, {"CY", 500}});
    SaveData with_top10;
    with_top10.top10 = top10;
    REQUIRE(validate(with_top10).has_value());

    SECTION("a refused score leaves the list alone")
    {
        const auto before = top10;
        REQUIRE(insert_score(top10, "EVE", 0) == std::unexpected(ScoreError::NOT_QUALIFYING));
        REQUIRE(insert_score(top10, "eve", 600) == std::unexpected(ScoreError::NAME_INVALID));
        REQUIRE(insert_score(top10, "", 600) == std::unexpected(ScoreError::NAME_INVALID));
        REQUIRE(insert_score(top10, "ABCDEFGHIJKLM", 600) == std::unexpected(ScoreError::NAME_INVALID));
        REQUIRE(top10 == before);
    }
    SECTION("the entry pushed past ten drops off")
    {
        for (int i = 0; i < 6; ++i)
        {
            REQUIRE(insert_score(top10, "X", 100 + i).has_value());
        }
        REQUIRE(top10.size() == static_cast<std::size_t>(tuning.top10_size));
        REQUIRE(top10.back().score == 100);
        REQUIRE(insert_score(top10, "TOP", 5000) == 1);
        REQUIRE(top10.size() == static_cast<std::size_t>(tuning.top10_size));
        REQUIRE(top10.front().name == "TOP");
        REQUIRE(top10.back().score == 101); // the first 100 fell off
        REQUIRE(insert_score(top10, "LOW", 101) == std::unexpected(ScoreError::NOT_QUALIFYING));
        SaveData full_list;
        full_list.top10 = top10;
        REQUIRE(validate(full_list).has_value());
    }
}
