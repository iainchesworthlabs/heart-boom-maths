// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_data.hpp"

#include <algorithm>

namespace heart_boom_maths
{

namespace
{

bool is_name_character(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') || c == ' ';
}

} // namespace

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
    if (data.coins < 0 || data.coins > MAX_SAVED_COINS)
    {
        return std::unexpected(SaveError::COINS_OUT_OF_RANGE);
    }
    if (data.top10.size() > static_cast<std::size_t>(tuning.top10_size))
    {
        return std::unexpected(SaveError::TOP10_TOO_LONG);
    }
    for (std::size_t i = 0; i < data.top10.size(); ++i)
    {
        const ScoreEntry& entry = data.top10[i];
        if (!is_valid_name(entry.name, tuning))
        {
            return std::unexpected(SaveError::NAME_INVALID);
        }
        if (entry.score < 1)
        {
            return std::unexpected(SaveError::SCORE_INVALID);
        }
        if (i > 0 && entry.score > data.top10[i - 1].score)
        {
            return std::unexpected(SaveError::TOP10_NOT_SORTED);
        }
    }
    if (data.run && data.run->size() > MAX_SAVED_RUN_VALUES)
    {
        return std::unexpected(SaveError::RUN_TOO_LONG);
    }
    return {};
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

std::expected<int, ScoreError> insert_score(std::vector<ScoreEntry>& top10, std::string name, std::int64_t score,
                                            const Tuning& tuning)
{
    const std::optional<int> rank = rank_for_score(top10, score, tuning);
    if (!rank)
    {
        return std::unexpected(ScoreError::NOT_QUALIFYING);
    }
    if (!is_valid_name(name, tuning))
    {
        return std::unexpected(ScoreError::NAME_INVALID);
    }
    top10.insert(top10.begin() + (*rank - 1), ScoreEntry{std::move(name), score});
    if (top10.size() > static_cast<std::size_t>(tuning.top10_size))
    {
        top10.resize(static_cast<std::size_t>(tuning.top10_size));
    }
    return *rank;
}

} // namespace heart_boom_maths
