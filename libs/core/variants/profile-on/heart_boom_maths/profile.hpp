// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The profiling marks of the whole game, over Tracy. This file and its sibling in profile-off are the only places that
// include Tracy; the core and the app layer use the macros below and never Tracy's own, so the dependency stays here.
// CMake puts one of the two directories on the include path (it follows ICL_ENABLE_TRACY), so no source asks which it
// got and nothing in the code is conditional on it.
//
// The Tracy client is built for manual lifetime (vcpkg-overlay-ports/tracy): it does not exist until start() runs,
// which the app layer does only when asked (apps/game/src/profiling/profile_switch.hpp). Until then every mark is one
// relaxed load and a branch that is not taken: no thread, no socket, no allocation. With on-demand mode, once started,
// nothing is collected until a viewer connects, and the listener is bound to the loopback interface (only-localhost).
//
// Nothing here reads or changes a game rule: a mark observes, the state of a session is integers and is not touched, so
// a replay gives the same events and hashes with the profiler off, started, or not built in.
//
// Names are string literals: Tracy keeps the pointer.

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

#include <atomic>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace heart_boom_maths::profile
{

/// True here, false in the profile-off sibling. A test pins it against the CMake option.
inline constexpr bool COMPILED_IN = true;

namespace detail
{

// The state every mark reads. With manual lifetime Tracy's own state cannot be touched before it starts.
inline std::atomic<bool> g_Running{false}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
inline std::atomic<int> g_StartCount{0};   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace detail

/// True between start() and stop().
[[nodiscard]] inline bool active() noexcept
{
    return detail::g_Running.load(std::memory_order_relaxed);
}

/// How many times start() has started the client in this process. Zero unless someone asked; one when someone did. A
/// test reads it to show that a build nobody asked to profile never starts Tracy.
[[nodiscard]] inline int start_count() noexcept
{
    return detail::g_StartCount.load(std::memory_order_relaxed);
}

/// True once a viewer has connected. Tracy records nothing before that.
[[nodiscard]] inline bool connected() noexcept
{
    return active() && tracy::GetProfiler().IsConnected();
}

/// Starts the Tracy client: its worker thread, the loopback listener and the discovery broadcast. Call it once, before
/// the threads that should be profiled start. Does nothing the second time.
inline void start() noexcept
{
    if (!detail::g_Running.load(std::memory_order_acquire) && detail::g_StartCount.load() == 0)
    {
        tracy::StartupProfiler();
        detail::g_StartCount.fetch_add(1);
        detail::g_Running.store(true, std::memory_order_release);
    }
}

/// Stops the client and flushes what it holds. Only when no other thread can be inside a zone any more.
inline void stop() noexcept
{
    if (detail::g_Running.exchange(false, std::memory_order_acq_rel))
    {
        tracy::ShutdownProfiler();
    }
}

inline void message_literal(const char* text)
{
    if (active())
    {
        TracyMessageL(text);
    }
}

/// A message with text built at run time; Tracy copies it.
inline void message_dynamic(std::string_view text)
{
    if (active())
    {
        TracyMessage(text.data(), text.size());
    }
}

inline void app_info(std::string_view text)
{
    if (active())
    {
        TracyAppInfo(text.data(), text.size());
    }
}

inline void frame_mark(const char* name)
{
    if (active())
    {
        tracy::Profiler::SendFrameMark(name);
    }
}

/// Names the calling thread, once per thread: every call adds a table entry.
inline void thread_name(const char* name)
{
    if (active())
    {
        tracy::SetThreadName(name);
    }
}

/// Tracy's plot is overloaded on int64_t, float and double, so an int is ambiguous between all three. Integers stay
/// integers and anything floating becomes a double.
template <typename T>
void plot(const char* name, T value)
{
    static_assert(std::is_arithmetic_v<T>, "a plot takes a number");
    if (!active())
    {
        return;
    }
    if constexpr (std::is_floating_point_v<T>)
    {
        TracyPlot(name, static_cast<double>(value));
    }
    else
    {
        TracyPlot(name, static_cast<std::int64_t>(value));
    }
}

/// A zone whose begin and end are not one C++ scope: the render zones open in one scene graph signal and close in
/// another. The destructor closes an open zone and end() does nothing twice.
class ZoneSpan
{
public:
    ZoneSpan() = default;
    ~ZoneSpan()
    {
        end();
    }
    ZoneSpan(const ZoneSpan&) = delete;
    ZoneSpan& operator=(const ZoneSpan&) = delete;
    ZoneSpan(ZoneSpan&&) = delete;
    ZoneSpan& operator=(ZoneSpan&&) = delete;

    void begin(const char* name) noexcept
    {
        if (m_Open)
        {
            return;
        }
        m_Location = ___tracy_source_location_data{
            .name = name,
            .function = __func__,
            .file = __FILE__,
            .line = 0,
            .color = 0,
        };
        m_Context = ___tracy_emit_zone_begin(&m_Location, active() ? 1 : 0);
        m_Open = true;
    }

    void end() noexcept
    {
        if (m_Open)
        {
            m_Open = false;
            ___tracy_emit_zone_end(m_Context);
        }
    }

private:
    ___tracy_source_location_data m_Location{};
    TracyCZoneCtx m_Context{};
    bool m_Open = false;
};

} // namespace heart_boom_maths::profile

// Macros, not functions: a Tracy zone is an object that lives in the caller's scope, its name is a literal whose
// address it keeps, and the profile-off sibling makes every one of these vanish without evaluating its arguments.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// A zone for the rest of the C++ scope, named by a string literal.
#define HBM_ZONE(name) SuppressVarShadowWarning(ZoneNamedN(hbm_zone, name, ::heart_boom_maths::profile::active()))
/// The same, named after the function.
#define HBM_ZONE_FN() SuppressVarShadowWarning(ZoneNamed(hbm_zone, ::heart_boom_maths::profile::active()))
/// Attaches a number to the zone of the enclosing HBM_ZONE or HBM_ZONE_FN.
#define HBM_ZONE_VALUE(number) hbm_zone.Value(static_cast<std::uint64_t>(number))
/// A number on a timeline. `name` is a string literal; `value` is an integer or floating-point expression.
#define HBM_PLOT(name, value) ::heart_boom_maths::profile::plot(name, value)
/// A discrete event on the timeline. `text` is a string literal.
#define HBM_MESSAGE(text) ::heart_boom_maths::profile::message_literal(text)
/// The same for text built at run time (Tracy copies it). Pass a variable, not a braced initialiser.
#define HBM_MESSAGE_DYNAMIC(text) ::heart_boom_maths::profile::message_dynamic(text)
/// One per real-time frame, for the frame view. `name` is a string literal.
#define HBM_FRAME_MARK(name) ::heart_boom_maths::profile::frame_mark(name)
/// Names the calling thread, once.
#define HBM_THREAD_NAME(name) ::heart_boom_maths::profile::thread_name(name)
/// Free text for the trace's information window.
#define HBM_INFO(text) ::heart_boom_maths::profile::app_info(text)

// NOLINTEND(cppcoreguidelines-macro-usage)
