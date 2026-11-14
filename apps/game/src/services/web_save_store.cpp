// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/web_save_store.hpp"

#include <utility>

namespace heart_boom_maths::game
{

WebSaveStore::WebSaveStore(std::unique_ptr<KeyValueBackend> backend) : m_Backend(std::move(backend)) {}

std::optional<std::string> WebSaveStore::load()
{
    // A backend that cannot answer (blocked storage) answers "nothing", which is a first run: the game starts on
    // defaults and the first save fails the same way, without a crash.
    return m_Backend->get(SAVE_KEY);
}

bool WebSaveStore::store(std::string_view text)
{
    return m_Backend->set(SAVE_KEY, text);
}

bool WebSaveStore::set_damaged_aside()
{
    const std::optional<std::string> damaged = m_Backend->get(SAVE_KEY);
    if (!damaged.has_value())
    {
        return true; // nothing is there to lose
    }
    // The copy comes first: when it cannot be kept, the damaged text stays under its own key. Only the latest
    // damaged save is kept, as with the file.
    if (!m_Backend->set(DAMAGED_KEY, *damaged))
    {
        return false;
    }
    return m_Backend->remove(SAVE_KEY);
}

} // namespace heart_boom_maths::game
