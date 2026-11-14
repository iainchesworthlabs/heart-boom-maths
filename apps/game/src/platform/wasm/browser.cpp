// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/browser.hpp"

#include <emscripten/html5.h>

#include <emscripten.h>

#include <utility>

namespace heart_boom_maths::game::platform
{

namespace
{

// A browser can refuse a page its storage in several ways: cookies blocked, a private window, a sandboxed frame, a full
// quota. Reading `window.localStorage` or writing to it then throws a JavaScript exception. That is not a C++
// exception, and Qt's settings code has no handler for it, so it would run through the game uncaught. The probe is
// JavaScript so that it can catch it. EM_JS is compiled into the page's script, not evaluated, so the page's content
// security policy needs no 'unsafe-eval'.
// clang-format off
EM_JS(int, hbm_page_storage_works, (), {
    try
    {
        // Qt's settings fall back to a temporary file when cookies are off, which nothing would keep either.
        if (navigator.cookieEnabled === false)
        {
            return 0;
        }
        const probe = "heart-boom-maths-probe";
        window.localStorage.setItem(probe, "1");
        const kept = window.localStorage.getItem(probe) === "1";
        window.localStorage.removeItem(probe);
        return kept ? 1 : 0;
    }
    catch (error)
    {
        return 0;
    }
});
// clang-format on

using VisibilityCallback = std::function<void(bool visible)>;

VisibilityCallback& visibility_callback()
{
    static VisibilityCallback callback;
    return callback;
}

bool on_visibility_change(int /*event_type*/, const EmscriptenVisibilityChangeEvent* event, void* /*user_data*/)
{
    if (visibility_callback())
    {
        visibility_callback()(event->hidden == 0);
    }
    return false; // the page's own handling is not suppressed
}

} // namespace

SaveStorage save_storage()
{
    return hbm_page_storage_works() != 0 ? SaveStorage::BROWSER : SaveStorage::SESSION;
}

void watch_page_visibility(std::function<void(bool visible)> on_change)
{
    visibility_callback() = std::move(on_change);
    // The event is on the document, which is what changes when a tab is hidden; the window's blur is not reliable for
    // it.
    emscripten_set_visibilitychange_callback(nullptr, false, on_visibility_change);
    // A page opened in a background tab never sees a change to hidden.
    EmscriptenVisibilityChangeEvent status{};
    if (emscripten_get_visibility_status(&status) == EMSCRIPTEN_RESULT_SUCCESS && status.hidden != 0 &&
        visibility_callback())
    {
        visibility_callback()(false);
    }
}

} // namespace heart_boom_maths::game::platform
