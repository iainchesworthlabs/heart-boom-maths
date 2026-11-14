// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace heart_boom_maths::game::profiling
{

/// Tracy's GPU context for the graphics API the scene graph renders with, behind an opaque interface so that no other
/// file pays for the API's headers. Only Direct3D 11 has one: it is the Windows default of Qt Quick, and its
/// timestamps are issued on the device's immediate context, which Qt exposes (QSGRendererInterface). The other
/// backends get the GPU frame time plot only (docs/profiling.md says why). CMake compiles gpu_d3d11.cpp on Windows and
/// gpu_none.cpp elsewhere, as it does for every platform difference.
///
/// A context is created and driven on the thread whose command stream it brackets, the scene graph's render thread.
class GpuContext
{
public:
    GpuContext() = default;
    GpuContext(const GpuContext&) = delete;
    GpuContext& operator=(const GpuContext&) = delete;
    GpuContext(GpuContext&&) = delete;
    GpuContext& operator=(GpuContext&&) = delete;
    virtual ~GpuContext() = default;

    /// Inserts the begin timestamp of a zone into the command stream. `name` is a string literal.
    virtual void begin_zone(const char* name) noexcept = 0;
    /// Inserts the end timestamp. Does nothing when no zone is open.
    virtual void end_zone() noexcept = 0;
    /// Resolves the timestamps the GPU has finished. Once per frame, outside a zone.
    virtual void collect() noexcept = 0;
};

/// A Direct3D 11 context from the device and its immediate context (`void*` so that this header needs no Windows
/// header), or null when there is none or this platform has no Direct3D 11. It is never destroyed: it holds a reference
/// to the device, and destroying it would have to happen on the render thread after the window has gone.
[[nodiscard]] GpuContext* create_d3d11_context(void* device, void* device_context) noexcept;

} // namespace heart_boom_maths::game::profiling
