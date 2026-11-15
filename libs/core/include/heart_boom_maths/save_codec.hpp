// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/save_data.hpp"

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace heart_boom_maths
{

/// One schema step: rewrites the document from version n to n + 1. The framework sets `version` afterwards. It returns
/// false when the document does not have the shape that version should have, and the save is then damaged.
using MigrationStep = bool (*)(json::Value& document);

/// The schema this build understands: the current version and the steps that lead to it. `steps[i]` takes version
/// i + 1 to i + 2, so there are `current_version - 1` of them. The release schema has version 2 and one step, from 1;
/// tests build their own to prove the chain without shipping a made-up migration.
struct SaveSchema
{
    int current_version = SAVE_VERSION;
    std::span<const MigrationStep> steps;

    [[nodiscard]] static SaveSchema release() noexcept;
};

/// The step from version 1 to version 2: one player, `first`, with the old wallet and win-bonus mark; the old top 10
/// numbered in rank order; the settings and the saved run on the device. It moves values as they are and leaves their
/// checking to the reader, so a damaged version 1 file reports the same rule it always did. Returns false only for a
/// document that is not an object.
[[nodiscard]] bool migrate_save_v1_to_v2(json::Value& document);

enum class LoadStatus
{
    OK,      // `data` is the saved game, or the defaults when there was no file
    DAMAGED, // cannot be read or fails validation; the caller renames the file to save.json.bad and starts on defaults
    NEWER,   // written by a later build; never overwrite it, and run this session on defaults
};

struct ParseResult
{
    LoadStatus status = LoadStatus::OK;
    SaveData data;                            // defaults unless status is OK
    std::optional<SaveError> damage;          // set when status is DAMAGED
    std::optional<std::int64_t> file_version; // the version the file claims, when it got far enough to say
};

/// The text of a save. Fails with the first rule the data breaks, so a save the next load would call damaged is never
/// written.
[[nodiscard]] std::expected<std::string, SaveError> serialise(const SaveData& data,
                                                              const Tuning& tuning = Tuning::defaults(),
                                                              const SaveSchema& schema = SaveSchema::release());

/// Reads a save with full validation. Never throws and never crashes, whatever the text holds.
[[nodiscard]] ParseResult parse_save(std::string_view text, const Tuning& tuning = Tuning::defaults(),
                                     const SaveSchema& schema = SaveSchema::release());

} // namespace heart_boom_maths
