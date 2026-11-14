// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_codec.hpp"

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

std::expected<std::vector<ScoreEntry>, SaveError> read_top10(const Value& object, const Tuning& tuning)
{
    const Value* field = object.find("top10");
    if (field == nullptr)
    {
        return std::unexpected(SaveError::MISSING_FIELD);
    }
    const Value::Array* items = field->get_array();
    if (items == nullptr)
    {
        return std::unexpected(SaveError::WRONG_TYPE);
    }
    // Checked before any entry is read, so an array of a million entries costs nothing.
    if (items->size() > static_cast<std::size_t>(tuning.top10_size))
    {
        return std::unexpected(SaveError::TOP10_TOO_LONG);
    }
    std::vector<ScoreEntry> entries;
    entries.reserve(items->size());
    for (const Value& item : *items)
    {
        if (item.kind() != json::Kind::OBJECT)
        {
            return std::unexpected(SaveError::WRONG_TYPE);
        }
        const Value* name = item.find("name");
        if (name == nullptr)
        {
            return std::unexpected(SaveError::MISSING_FIELD);
        }
        const std::string* name_text = name->get_string();
        if (name_text == nullptr)
        {
            return std::unexpected(SaveError::WRONG_TYPE);
        }
        auto score = read_integer(item, "score");
        if (!score)
        {
            return std::unexpected(score.error());
        }
        entries.push_back(ScoreEntry{*name_text, *score});
    }
    return entries;
}

std::expected<std::optional<std::vector<std::int64_t>>, SaveError> read_run(const Value& object)
{
    const Value* field = object.find("run");
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

// Reads the fields of a document that is already at the current version.
std::expected<SaveData, SaveError> read_fields(const Value& root, const Tuning& tuning, int version)
{
    SaveData data;
    data.version = version;

    const auto coins = read_integer(root, "coins");
    const auto won_once = read_bool(root, "won_once", std::nullopt);
    const auto sound = read_bool(root, "sound", std::nullopt);
    const auto music = read_bool(root, "music", std::nullopt);
    // game_center is the one switch with a stated default, so a file that predates it still loads.
    const auto game_center = read_bool(root, "game_center", false);
    if (!coins)
    {
        return std::unexpected(coins.error());
    }
    if (!won_once)
    {
        return std::unexpected(won_once.error());
    }
    if (!sound)
    {
        return std::unexpected(sound.error());
    }
    if (!music)
    {
        return std::unexpected(music.error());
    }
    if (!game_center)
    {
        return std::unexpected(game_center.error());
    }
    data.coins = *coins;
    data.won_once = *won_once;
    data.sound = *sound;
    data.music = *music;
    data.game_center = *game_center;

    auto top10 = read_top10(root, tuning);
    if (!top10)
    {
        return std::unexpected(top10.error());
    }
    data.top10 = std::move(*top10);

    auto run = read_run(root);
    if (!run)
    {
        return std::unexpected(run.error());
    }
    data.run = std::move(*run);
    return data;
}

ParseResult damaged(SaveError reason, std::optional<std::int64_t> file_version = std::nullopt)
{
    ParseResult result;
    result.status = LoadStatus::DAMAGED;
    result.damage = reason;
    result.file_version = file_version;
    return result;
}

} // namespace

std::expected<std::string, SaveError> serialise(const SaveData& data, const Tuning& tuning, const SaveSchema& schema)
{
    if (auto valid = validate(data, tuning, schema.current_version); !valid)
    {
        return std::unexpected(valid.error());
    }

    Value root = Value::object();
    root.set("version", Value::integer(data.version));
    root.set("coins", Value::integer(data.coins));
    root.set("won_once", Value::boolean(data.won_once));
    root.set("sound", Value::boolean(data.sound));
    root.set("music", Value::boolean(data.music));
    root.set("game_center", Value::boolean(data.game_center));

    Value::Array entries;
    entries.reserve(data.top10.size());
    for (const ScoreEntry& entry : data.top10)
    {
        Value item = Value::object();
        item.set("name", Value::string(entry.name));
        item.set("score", Value::integer(entry.score));
        entries.push_back(std::move(item));
    }
    root.set("top10", Value::array(std::move(entries)));

    if (data.run)
    {
        Value::Array values;
        values.reserve(data.run->size());
        for (const std::int64_t value : *data.run)
        {
            values.push_back(Value::integer(value));
        }
        root.set("run", Value::array(std::move(values)));
    }
    return json::write(root);
}

ParseResult parse_save(std::string_view text, const Tuning& tuning, const SaveSchema& schema)
{
    json::Limits limits;
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

    ParseResult result;
    result.status = LoadStatus::OK;
    result.data = std::move(*data);
    result.file_version = file_version;
    return result;
}

} // namespace heart_boom_maths
