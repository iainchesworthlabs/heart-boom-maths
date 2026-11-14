// SPDX-License-Identifier: GPL-3.0-or-later
// Direct3D 11 GPU context for the Tracy build on Windows. CMake compiles it in place of gpu_none.cpp.
//
// The device and its immediate context come from the window's QSGRendererInterface as void*, so this file needs no Qt.
// It is created and driven on the scene graph's render thread: begin_zone() puts a timestamp before the pass's draw
// calls, end_zone() after them, and collect() resolves what the GPU has finished.
#include "gpu_context.hpp"
#include "profiling.hpp"

#include <tracy/TracyD3D11.hpp>

#include <d3d11.h>
#include <windows.h>

#include <optional>

namespace heart_boom_maths::game::profiling
{

namespace
{

class D3D11GpuContext final : public GpuContext
{
public:
    D3D11GpuContext(ID3D11Device* device, ID3D11DeviceContext* device_context)
    {
        if (device_context == nullptr)
        {
            device->GetImmediateContext(&device_context);
        }
        if (device_context == nullptr)
        {
            return;
        }
        m_Context = TracyD3D11Context(device, device_context);
        if (m_Context != nullptr)
        {
            TracyD3D11ContextName(m_Context, "render", 6);
        }
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return m_Context != nullptr;
    }

    void begin_zone(const char* name) noexcept override
    {
        if (m_Context == nullptr)
        {
            return;
        }
        m_Location = tracy::SourceLocationData{name, __func__, __FILE__, 0, 0};
        m_Zone.emplace(m_Context, &m_Location, true);
    }

    void end_zone() noexcept override
    {
        m_Zone.reset();
    }

    void collect() noexcept override
    {
        if (m_Context != nullptr)
        {
            TracyD3D11Collect(m_Context);
        }
    }

private:
    TracyD3D11Ctx m_Context = nullptr;
    tracy::SourceLocationData m_Location{};
    std::optional<tracy::D3D11ZoneScope> m_Zone;
};

} // namespace

GpuContext* create_d3d11_context(void* device, void* device_context) noexcept
{
    auto* d3d_device = static_cast<ID3D11Device*>(device);
    if (d3d_device == nullptr || !profile::active())
    {
        return nullptr;
    }
    // Never destroyed on purpose (see gpu_context.hpp); one that did not come up is.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,bugprone-unhandled-exception-at-new)
    auto* context = new D3D11GpuContext(d3d_device, static_cast<ID3D11DeviceContext*>(device_context));
    if (!context->valid())
    {
        delete context; // NOLINT(cppcoreguidelines-owning-memory)
        return nullptr;
    }
    return context;
}

} // namespace heart_boom_maths::game::profiling
