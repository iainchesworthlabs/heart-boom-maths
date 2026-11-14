// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "heart_boom_maths/tuning.hpp"

namespace heart_boom_maths
{

struct LoadResult
{
    LoadStatus status = LoadStatus::OK;
    SaveData data;                   // defaults unless status is OK
    std::optional<SaveError> damage; // set when status is DAMAGED
    bool file_existed = false;       // false on a first run, when OK means "nothing to read"
};

enum class SaveOutcome
{
    SAVED,
    NOT_LOADED,    // load() has not run yet, so the file has not been looked at
    REFUSED_NEWER, // the file on disk came from a later build; it is left alone
    INVALID,       // the data breaks a rule of the format, so nothing was written
    STORE_FAILED,
};

/// The save logic between the game and a `SaveStore`. The store stays a dumb text holder; this class decides what the
/// text means and when it may be replaced.
class SaveManager
{
public:
    explicit SaveManager(SaveStore& store, const Tuning& tuning = Tuning::defaults(),
                         const SaveSchema& schema = SaveSchema::release());

    /// Reads the store. A missing file is OK with the defaults. On DAMAGED the caller moves the file aside
    /// (save.json.bad) before the next save(), which would otherwise replace it. On NEWER every save() is refused.
    [[nodiscard]] LoadResult load();

    /// Writes the data, unless the last load found a newer file or no load has happened.
    [[nodiscard]] SaveOutcome save(const SaveData& data);

private:
    SaveStore* m_Store;
    Tuning m_Tuning;
    SaveSchema m_Schema;
    bool m_Loaded = false;
    bool m_NewerFile = false;
};

} // namespace heart_boom_maths
