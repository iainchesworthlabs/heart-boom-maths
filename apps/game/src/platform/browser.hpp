// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

namespace heart_boom_maths::game::platform
{

/// Where this build keeps the save. Every directory under src/platform/<os>/ answers it, and only the browser build
/// answers anything but LOCAL_FILE.
enum class SaveStorage
{
    LOCAL_FILE, // a file in the application's data folder (iOS, Android and the desktop)
    BROWSER,    // the page's localStorage, through QSettings
    SESSION,    // the browser lets the page keep nothing, so the save lives until the tab closes
};

/// Which storage to use. In a browser this tries the page's storage once (a tiny write and delete) and answers SESSION
/// when it throws, because Qt's own settings code would let that exception run through the game.
[[nodiscard]] SaveStorage save_storage();

/// Calls `on_change(false)` when the page is hidden (another tab, a minimised window, a locked phone) and
/// `on_change(true)` when it is shown again, and once at the start when the page opens hidden. Qt's browser platform
/// does not report a hidden tab as a change of application state, so this is the only signal. It does nothing outside
/// a browser, where Qt reports the state itself.
void watch_page_visibility(std::function<void(bool visible)> on_change);

} // namespace heart_boom_maths::game::platform
