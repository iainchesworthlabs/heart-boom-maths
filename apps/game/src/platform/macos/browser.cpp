// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/browser.hpp"

namespace heart_boom_maths::game::platform
{

SaveStorage save_storage()
{
    return SaveStorage::LOCAL_FILE;
}

void watch_page_visibility(std::function<void(bool visible)> /*on_change*/)
{
    // Not a browser: Qt reports the application state itself.
}

} // namespace heart_boom_maths::game::platform
