// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_manager.hpp"

#include "heart_boom_maths/profile.hpp"

namespace heart_boom_maths
{

SaveManager::SaveManager(SaveStore& store, const Tuning& tuning, const SaveSchema& schema)
    : m_Store(&store), m_Tuning(tuning), m_Schema(schema)
{
}

LoadResult SaveManager::load()
{
    HBM_ZONE("save_manager.load");
    LoadResult result;
    m_Loaded = true;
    m_NewerFile = false;

    const std::optional<std::string> text = m_Store->load();
    if (!text)
    {
        return result;
    }
    result.file_existed = true;

    ParseResult parsed = parse_save(*text, m_Tuning, m_Schema);
    result.status = parsed.status;
    result.damage = parsed.damage;
    if (parsed.status == LoadStatus::OK)
    {
        result.data = std::move(parsed.data);
    }
    m_NewerFile = parsed.status == LoadStatus::NEWER;
    return result;
}

SaveOutcome SaveManager::save(const SaveData& data)
{
    HBM_ZONE("save_manager.save");
    if (!m_Loaded)
    {
        return SaveOutcome::NOT_LOADED;
    }
    if (m_NewerFile)
    {
        return SaveOutcome::REFUSED_NEWER;
    }
    const auto text = serialise(data, m_Tuning, m_Schema);
    if (!text)
    {
        return SaveOutcome::INVALID;
    }
    return m_Store->store(*text) ? SaveOutcome::SAVED : SaveOutcome::STORE_FAILED;
}

} // namespace heart_boom_maths
