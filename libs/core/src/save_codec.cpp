// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_codec.hpp"

#include "heart_boom_maths/profile.hpp"

#include <array>
#include <utility>

namespace heart_boom_maths
{

namespace
{

using json::Value;

std::expected<bool, SaveError> read_bool(const Value& object, std::string_view key, std::optional<bool> fallback)
{
    const Value* field = object.find(key);
    if (field == nullptr)
    {
        if (fallback)
        {
            return *fallback;
        }
        return std::unexpected(SaveError::MISSING_FIELD);
    }
    const bool* value = field->get_boolean();
    if (value == nullptr)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    return *value;
}

std::expected<std::int64_t, SaveError> read_integer(const Value& object, std::string_view key)
{
    const Value* field = object.find(key);
    if (field == nullptr)
    {
        return std::unexpected(SaveError::MISSING_FIELD);
    }
    const std::int64_t* value = field->get_integer();
    if (value == nullptr)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    return *value;
}

// The string stays in the document, so the caller copies it once, into the place it is going to.
std::expected<const std::string*, SaveError> read_string(const Value& object, std::string_view key)
{
    const Value* field = object.find(key);
    if (field == nullptr)
    {
        return std::unexpected(SaveError::MISSING_FIELD);
    }
    const std::string* value = field->get_string();
    if (value == nullptr)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    return value;
}

std::expected<const Value::Array*, SaveError> read_array(const Value& object, std::string_view key)
{
    const Value* field = object.find(key);
    if (field == nullptr)
    {
        return std::unexpected(SaveError::MISSING_FIELD);
    }
    const Value::Array* items = field->get_array();
    if (items == nullptr)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    return items;
}

std::expected<const Value*, SaveError> read_object(const Value& object, std::string_view key)
{
    const Value* field = object.find(key);
    if (field == nullptr)
    {
        return std::unexpected(SaveError::MISSING_FIELD);
    }
    if (field->kind() != json::Kind::OBJECT)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    return field;
}

std::expected<std::vector<ScoreEntry>, SaveError> read_top10(const Value& shared, const Tuning& tuning)
{
    const auto items = read_array(shared, "top10");
    if (!items)
    {
        return std::unexpected(items.error());
    }
    // Checked before any entry is read, so an array of a million entries costs nothing.
    if ((*items)->size() > static_cast<std::size_t>(tuning.top10_size))
    {
        return std::unexpected(SaveError::TOP10_TOO_LONG);
    }
    std::vector<ScoreEntry> entries;
    entries.reserve((*items)->size());
    for (const Value& item : **items)
    {
        if (item.kind() != json::Kind::OBJECT)
        {
            return std::unexpected(SaveError::WRONG_TYPE);
        }
        // Each field is checked as it is read, in this order, so the first one at fault is the one reported.
        const auto name = read_string(item, "name");
        if (!name)
        {
            return std::unexpected(name.error());
        }
        const auto score = read_integer(item, "score");
        if (!score)
        {
            return std::unexpected(score.error());
        }
        const auto device = read_string(item, "device");
        if (!device)
        {
            return std::unexpected(device.error());
        }
        const auto seq = read_integer(item, "seq");
        if (!seq)
        {
            return std::unexpected(seq.error());
        }
        entries.push_back(ScoreEntry{**name, *score, **device, *seq});
    }
    return entries;
}

std::expected<std::vector<CoinEntry>, SaveError> read_coins(const Value& player)
{
    const auto items = read_array(player, "coins");
    if (!items)
    {
        return std::unexpected(items.error());
    }
    if ((*items)->size() > MAX_SAVED_COIN_ENTRIES)
    {
        return std::unexpected(SaveError::COIN_ENTRIES_TOO_MANY);
    }
    std::vector<CoinEntry> entries;
    entries.reserve((*items)->size());
    for (const Value& item : **items)
    {
        if (item.kind() != json::Kind::OBJECT)
        {
            return std::unexpected(SaveError::WRONG_TYPE);
        }
        const auto device = read_string(item, "device");
        if (!device)
        {
            return std::unexpected(device.error());
        }
        const auto earned = read_integer(item, "earned");
        if (!earned)
        {
            return std::unexpected(earned.error());
        }
        const auto spent = read_integer(item, "spent");
        if (!spent)
        {
            return std::unexpected(spent.error());
        }
        entries.push_back(CoinEntry{**device, *earned, *spent});
    }
    return entries;
}

std::expected<Player, SaveError> read_player(const Value& item)
{
    if (item.kind() != json::Kind::OBJECT)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    const auto id = read_string(item, "id");
    if (!id)
    {
        return std::unexpected(id.error());
    }
    const auto name = read_string(item, "name");
    if (!name)
    {
        return std::unexpected(name.error());
    }
    const auto name_rev = read_integer(item, "name_rev");
    if (!name_rev)
    {
        return std::unexpected(name_rev.error());
    }
    const auto removed = read_bool(item, "removed", std::nullopt);
    if (!removed)
    {
        return std::unexpected(removed.error());
    }
    const auto won_once = read_bool(item, "won_once", std::nullopt);
    if (!won_once)
    {
        return std::unexpected(won_once.error());
    }
    const auto best = read_integer(item, "best");
    if (!best)
    {
        return std::unexpected(best.error());
    }
    auto coins = read_coins(item);
    if (!coins)
    {
        return std::unexpected(coins.error());
    }
    return Player{**id, **name, *name_rev, *removed, *won_once, *best, std::move(*coins)};
}

std::expected<SharedData, SaveError> read_shared(const Value& root, const Tuning& tuning)
{
    const auto shared = read_object(root, "shared");
    if (!shared)
    {
        return std::unexpected(shared.error());
    }
    const auto items = read_array(**shared, "players");
    if (!items)
    {
        return std::unexpected(items.error());
    }
    if ((*items)->size() > MAX_SAVED_PLAYERS)
    {
        return std::unexpected(SaveError::PLAYERS_TOO_MANY);
    }
    // Built empty: the default holds the first player, and a file says which players there are.
    SharedData data{std::vector<Player>{}, std::vector<ScoreEntry>{}};
    data.players.reserve((*items)->size());
    for (const Value& item : **items)
    {
        auto player = read_player(item);
        if (!player)
        {
            return std::unexpected(player.error());
        }
        data.players.push_back(std::move(*player));
    }
    auto top10 = read_top10(**shared, tuning);
    if (!top10)
    {
        return std::unexpected(top10.error());
    }
    data.top10 = std::move(*top10);
    return data;
}

std::expected<std::optional<std::vector<std::int64_t>>, SaveError> read_run(const Value& device)
{
    const Value* field = device.find("run");
    if (field == nullptr)
    {
        return std::optional<std::vector<std::int64_t>>{};
    }
    const Value::Array* items = field->get_array();
    if (items == nullptr)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    if (items->size() > MAX_SAVED_RUN_VALUES)
    {
        return std::unexpected(SaveError::RUN_TOO_LONG);
    }
    std::vector<std::int64_t> values;
    values.reserve(items->size());
    for (const Value& item : *items)
    {
        const std::int64_t* value = item.get_integer();
        if (value == nullptr)
        {
            return std::unexpected(SaveError::WRONG_TYPE);
        }
        values.push_back(*value);
    }
    return std::optional<std::vector<std::int64_t>>{std::move(values)};
}

std::expected<DeviceData, SaveError> read_device(const Value& root)
{
    const auto field = read_object(root, "device");
    if (!field)
    {
        return std::unexpected(field.error());
    }
    const Value& device = **field;

    // As in read_player: each field is checked as it is read, so the first one at fault is the one reported.
    const auto id = read_string(device, "id");
    if (!id)
    {
        return std::unexpected(id.error());
    }
    const auto sound = read_bool(device, "sound", std::nullopt);
    if (!sound)
    {
        return std::unexpected(sound.error());
    }
    const auto music = read_bool(device, "music", std::nullopt);
    if (!music)
    {
        return std::unexpected(music.error());
    }
    // The switches added after the first release are the ones with a stated default, so a file that predates one still
    // loads: game_center is off, haptics is on as a new player has it, and cloud_sync is off.
    const auto game_center = read_bool(device, "game_center", false);
    if (!game_center)
    {
        return std::unexpected(game_center.error());
    }
    const auto haptics = read_bool(device, "haptics", true);
    if (!haptics)
    {
        return std::unexpected(haptics.error());
    }
    const auto cloud_sync = read_bool(device, "cloud_sync", false);
    if (!cloud_sync)
    {
        return std::unexpected(cloud_sync.error());
    }
    const auto current = read_string(device, "current_player");
    if (!current)
    {
        return std::unexpected(current.error());
    }
    const auto next_seq = read_integer(device, "next_seq");
    if (!next_seq)
    {
        return std::unexpected(next_seq.error());
    }

    DeviceData data;
    data.id = **id;
    data.sound = *sound;
    data.music = *music;
    data.game_center = *game_center;
    data.haptics = *haptics;
    data.cloud_sync = *cloud_sync;
    data.current_player = **current;
    data.next_seq = *next_seq;

    auto run = read_run(device);
    if (!run)
    {
        return std::unexpected(run.error());
    }
    data.run = std::move(*run);
    if (device.find("run_player") != nullptr)
    {
        const auto run_player = read_string(device, "run_player");
        if (!run_player)
        {
            return std::unexpected(run_player.error());
        }
        data.run_player = **run_player;
    }
    return data;
}

// Reads the fields of a document that is already at the current version.
std::expected<SaveData, SaveError> read_fields(const Value& root, const Tuning& tuning, int version)
{
    auto device = read_device(root);
    if (!device)
    {
        return std::unexpected(device.error());
    }
    auto shared = read_shared(root, tuning);
    if (!shared)
    {
        return std::unexpected(shared.error());
    }
    return SaveData{version, std::move(*device), std::move(*shared)};
}

ParseResult damaged(SaveError reason, std::optional<std::int64_t> file_version = std::nullopt)
{
    ParseResult result;
    result.status = LoadStatus::DAMAGED;
    result.damage = reason;
    result.file_version = file_version;
    return result;
}

// The writers put the text of the save straight into one string, with no `json::Value` tree between, because a tree
// costs an allocation or two for every member and every string. They write what `json::write` would write for the same
// document: objects with their keys in ascending order, no spaces. A test reads the text back and writes it again to
// hold them to that, so a key put out of order fails there.

void append_bool(std::string& out, bool value)
{
    out.append(value ? "true" : "false");
}

void append_coins(std::string& out, const std::vector<CoinEntry>& coins)
{
    out.push_back('[');
    for (std::size_t i = 0; i < coins.size(); ++i)
    {
        const CoinEntry& coin = coins[i];
        out.append(i == 0 ? "{\"device\":" : ",{\"device\":");
        json::append_string(out, coin.device);
        out.append(",\"earned\":");
        json::append_integer(out, coin.earned);
        out.append(",\"spent\":");
        json::append_integer(out, coin.spent);
        out.push_back('}');
    }
    out.push_back(']');
}

void append_players(std::string& out, const std::vector<Player>& players)
{
    out.push_back('[');
    for (std::size_t i = 0; i < players.size(); ++i)
    {
        const Player& player = players[i];
        out.append(i == 0 ? "{\"best\":" : ",{\"best\":");
        json::append_integer(out, player.best);
        out.append(",\"coins\":");
        append_coins(out, player.coins);
        out.append(",\"id\":");
        json::append_string(out, player.id);
        out.append(",\"name\":");
        json::append_string(out, player.name);
        out.append(",\"name_rev\":");
        json::append_integer(out, player.name_rev);
        out.append(",\"removed\":");
        append_bool(out, player.removed);
        out.append(",\"won_once\":");
        append_bool(out, player.won_once);
        out.push_back('}');
    }
    out.push_back(']');
}

void append_top10(std::string& out, const std::vector<ScoreEntry>& top10)
{
    out.push_back('[');
    for (std::size_t i = 0; i < top10.size(); ++i)
    {
        const ScoreEntry& entry = top10[i];
        out.append(i == 0 ? "{\"device\":" : ",{\"device\":");
        json::append_string(out, entry.device);
        out.append(",\"name\":");
        json::append_string(out, entry.name);
        out.append(",\"score\":");
        json::append_integer(out, entry.score);
        out.append(",\"seq\":");
        json::append_integer(out, entry.seq);
        out.push_back('}');
    }
    out.push_back(']');
}

void append_shared(std::string& out, const SharedData& data)
{
    out.append("{\"players\":");
    append_players(out, data.players);
    out.append(",\"top10\":");
    append_top10(out, data.top10);
    out.push_back('}');
}

void append_device(std::string& out, const DeviceData& device)
{
    out.append("{\"cloud_sync\":");
    append_bool(out, device.cloud_sync);
    out.append(",\"current_player\":");
    json::append_string(out, device.current_player);
    out.append(",\"game_center\":");
    append_bool(out, device.game_center);
    out.append(",\"haptics\":");
    append_bool(out, device.haptics);
    out.append(",\"id\":");
    json::append_string(out, device.id);
    out.append(",\"music\":");
    append_bool(out, device.music);
    out.append(",\"next_seq\":");
    json::append_integer(out, device.next_seq);
    if (device.run)
    {
        out.append(",\"run\":[");
        for (std::size_t i = 0; i < device.run->size(); ++i)
        {
            if (i > 0)
            {
                out.push_back(',');
            }
            json::append_integer(out, (*device.run)[i]);
        }
        out.append("],\"run_player\":");
        json::append_string(out, device.run_player);
    }
    out.append(",\"sound\":");
    append_bool(out, device.sound);
    out.push_back('}');
}

// Room for a typical save, so the text does not grow by halves on the way: about 80 bytes for a top 10 entry, 130 and
// 70 for a player and each of its coin entries, up to 20 for a number of the run.
std::size_t estimated_size(const SharedData& shared)
{
    std::size_t size = 64 + shared.top10.size() * 80;
    for (const Player& player : shared.players)
    {
        size += 130 + player.coins.size() * 70;
    }
    return size;
}

// The step from version 1: see the declaration. Everything is moved by value, with no check of its type.
const std::array<MigrationStep, 1> RELEASE_STEPS = {migrate_save_v1_to_v2};

} // namespace

bool migrate_save_v1_to_v2(Value& document)
{
    if (document.kind() != json::Kind::OBJECT)
    {
        return false;
    }

    Value shared = Value::object();
    std::int64_t best = 0;
    std::int64_t next_seq = 1;
    if (const Value* old_top10 = document.find("top10"); old_top10 != nullptr)
    {
        const Value::Array* items = old_top10->get_array();
        if (items == nullptr)
        {
            shared.set("top10", *old_top10); // the wrong type stays, and the reader says so
        }
        else
        {
            Value::Array entries;
            entries.reserve(items->size());
            for (const Value& item : *items)
            {
                Value entry = item;
                if (entry.kind() == json::Kind::OBJECT)
                {
                    entry.set("device", Value::string(""));
                    entry.set("seq", Value::integer(next_seq));
                }
                ++next_seq;
                entries.push_back(std::move(entry));
            }
            if (!items->empty() && (*items)[0].kind() == json::Kind::OBJECT)
            {
                if (const Value* top_score = (*items)[0].find("score"); top_score != nullptr)
                {
                    if (const std::int64_t* score = top_score->get_integer(); score != nullptr)
                    {
                        best = *score;
                    }
                }
            }
            shared.set("top10", Value::array(std::move(entries)));
        }
    }

    Value player = Value::object();
    player.set("id", Value::string(std::string(FIRST_PLAYER_ID)));
    player.set("name", Value::string(""));
    player.set("name_rev", Value::integer(0));
    player.set("removed", Value::boolean(false));
    player.set("best", Value::integer(best));
    if (const Value* won_once = document.find("won_once"); won_once != nullptr)
    {
        player.set("won_once", *won_once);
    }
    if (const Value* coins = document.find("coins"); coins != nullptr)
    {
        Value entry = Value::object();
        entry.set("device", Value::string(""));
        entry.set("earned", *coins);
        entry.set("spent", Value::integer(0));
        player.set("coins", Value::array({std::move(entry)}));
    }
    shared.set("players", Value::array({std::move(player)}));

    Value device = Value::object();
    device.set("id", Value::string(""));
    for (const char* key : {"sound", "music", "game_center", "haptics"})
    {
        if (const Value* value = document.find(key); value != nullptr)
        {
            device.set(key, *value);
        }
    }
    device.set("cloud_sync", Value::boolean(false));
    device.set("current_player", Value::string(std::string(FIRST_PLAYER_ID)));
    device.set("next_seq", Value::integer(next_seq));
    if (const Value* run = document.find("run"); run != nullptr)
    {
        device.set("run", *run);
        device.set("run_player", Value::string(std::string(FIRST_PLAYER_ID)));
    }

    Value migrated = Value::object();
    migrated.set("version", Value::integer(1)); // the framework sets the new number
    migrated.set("device", std::move(device));
    migrated.set("shared", std::move(shared));
    document = std::move(migrated);
    return true;
}

SaveSchema SaveSchema::release() noexcept
{
    return {SAVE_VERSION, RELEASE_STEPS};
}

std::expected<std::string, SaveError> serialise(const SaveData& data, const Tuning& tuning, const SaveSchema& schema)
{
    HBM_ZONE("save.serialise");
    if (auto valid = validate(data, tuning, schema.current_version); !valid)
    {
        return std::unexpected(valid.error());
    }

    std::string text;
    text.reserve(estimated_size(data.shared) + 160 + (data.device.run ? data.device.run->size() * 12 : 0));
    text.append("{\"device\":");
    append_device(text, data.device);
    text.append(",\"shared\":");
    append_shared(text, data.shared);
    text.append(",\"version\":");
    json::append_integer(text, data.version);
    text.push_back('}');
    return text;
}

ParseResult parse_save(std::string_view text, const Tuning& tuning, const SaveSchema& schema)
{
    HBM_ZONE("save.parse");
    const json::Limits limits;
    auto parsed = json::parse(text, limits);
    if (!parsed)
    {
        return damaged(SaveError::NOT_JSON);
    }
    Value root = std::move(*parsed);
    if (root.kind() != json::Kind::OBJECT)
    {
        return damaged(SaveError::NOT_AN_OBJECT);
    }

    const Value* version_field = root.find("version");
    if (version_field == nullptr)
    {
        return damaged(SaveError::MISSING_FIELD);
    }
    const std::int64_t* version_value = version_field->get_integer();
    if (version_value == nullptr)
    {
        return damaged(SaveError::WRONG_TYPE);
    }

    if (*version_value > schema.current_version)
    {
        ParseResult newer;
        newer.status = LoadStatus::NEWER;
        newer.file_version = *version_value;
        return newer;
    }
    if (*version_value < 1)
    {
        return damaged(SaveError::BAD_VERSION);
    }

    const int file_version = static_cast<int>(*version_value);
    if (schema.steps.size() + 1 < static_cast<std::size_t>(schema.current_version))
    {
        return damaged(SaveError::MIGRATION_FAILED, file_version); // a schema with a missing step is a build error
    }
    for (int version = file_version; version < schema.current_version; ++version)
    {
        const MigrationStep step = schema.steps[static_cast<std::size_t>(version - 1)];
        if (step == nullptr || !step(root))
        {
            return damaged(SaveError::MIGRATION_FAILED, file_version);
        }
        root.set("version", Value::integer(version + 1));
    }

    auto data = read_fields(root, tuning, schema.current_version);
    if (!data)
    {
        return damaged(data.error(), file_version);
    }
    if (auto valid = validate(*data, tuning, schema.current_version); !valid)
    {
        return damaged(valid.error(), file_version);
    }

    // Built in one go, so the default save a ParseResult would hold first is never made.
    return ParseResult{
        .status = LoadStatus::OK, .data = std::move(*data), .damage = std::nullopt, .file_version = file_version};
}

std::expected<std::string, SaveError> serialise_shared(const SharedData& data, const Tuning& tuning,
                                                       const SaveSchema& schema)
{
    HBM_ZONE("save.serialise_shared");
    // Checked as part of a save with a device that is valid by default, so the rules are the save's and not a copy.
    const SaveData whole{schema.current_version, DeviceData{}, data};
    if (auto valid = validate(whole, tuning, schema.current_version); !valid)
    {
        return std::unexpected(valid.error());
    }

    std::string text;
    text.reserve(estimated_size(data) + 32);
    text.append("{\"shared\":");
    append_shared(text, data);
    text.append(",\"version\":");
    json::append_integer(text, schema.current_version);
    text.push_back('}');
    return text;
}

SharedParseResult parse_shared(std::string_view text, const Tuning& tuning, const SaveSchema& schema)
{
    HBM_ZONE("save.parse_shared");
    const auto fail = [](SaveError reason, std::optional<std::int64_t> file_version = std::nullopt)
    {
        SharedParseResult result;
        result.status = LoadStatus::DAMAGED;
        result.damage = reason;
        result.file_version = file_version;
        return result;
    };

    const json::Limits limits;
    auto parsed = json::parse(text, limits);
    if (!parsed)
    {
        return fail(SaveError::NOT_JSON);
    }
    const Value root = std::move(*parsed);
    if (root.kind() != json::Kind::OBJECT)
    {
        return fail(SaveError::NOT_AN_OBJECT);
    }
    const Value* version_field = root.find("version");
    if (version_field == nullptr)
    {
        return fail(SaveError::MISSING_FIELD);
    }
    const std::int64_t* version_value = version_field->get_integer();
    if (version_value == nullptr)
    {
        return fail(SaveError::WRONG_TYPE);
    }
    if (*version_value > schema.current_version)
    {
        SharedParseResult newer;
        newer.status = LoadStatus::NEWER;
        newer.file_version = *version_value;
        return newer;
    }
    // A payload is only ever written by `serialise_shared`, so an older one is not one this build knows how to read.
    if (*version_value != schema.current_version)
    {
        return fail(SaveError::BAD_VERSION, *version_value);
    }

    auto shared = read_shared(root, tuning);
    if (!shared)
    {
        return fail(shared.error(), *version_value);
    }
    if (auto valid = validate(SaveData{schema.current_version, DeviceData{}, *shared}, tuning, schema.current_version);
        !valid)
    {
        return fail(valid.error(), *version_value);
    }

    SharedParseResult result;
    result.status = LoadStatus::OK;
    result.data = std::move(*shared);
    result.file_version = *version_value;
    return result;
}

} // namespace heart_boom_maths
