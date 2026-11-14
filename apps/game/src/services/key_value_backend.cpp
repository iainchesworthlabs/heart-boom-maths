// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/key_value_backend.hpp"

namespace heart_boom_maths::game
{

std::optional<std::string> MemoryKeyValueBackend::get(std::string_view key)
{
    const auto found = m_Values.find(key);
    if (found == m_Values.end())
    {
        return std::nullopt;
    }
    return found->second;
}

bool MemoryKeyValueBackend::set(std::string_view key, std::string_view value)
{
    m_Values.insert_or_assign(std::string(key), std::string(value));
    return true;
}

bool MemoryKeyValueBackend::remove(std::string_view key)
{
    const auto found = m_Values.find(key);
    if (found != m_Values.end())
    {
        m_Values.erase(found);
    }
    return true;
}

} // namespace heart_boom_maths::game
