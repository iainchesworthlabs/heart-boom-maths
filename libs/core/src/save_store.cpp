// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_store.hpp"

namespace heart_boom_maths
{

MemorySaveStore::MemorySaveStore(std::string text) : m_Text(std::move(text)) {}

std::optional<std::string> MemorySaveStore::load()
{
    return m_Text;
}

bool MemorySaveStore::store(std::string_view text)
{
    if (m_StoreFails)
    {
        return false;
    }
    m_Text = std::string(text);
    ++m_StoreCount;
    return true;
}

} // namespace heart_boom_maths
