// SPDX-License-Identifier: GPL-3.0-or-later
#include "render_profiler.hpp"

#include <rhi/qrhi.h>

#include <QByteArray>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <string_view>
#include <utility>

namespace heart_boom_maths::game::profiling
{

namespace
{

const char* graphics_api_name(QSGRendererInterface::GraphicsApi api)
{
    switch (api)
    {
    case QSGRendererInterface::Software:
        return "software";
    case QSGRendererInterface::OpenVG:
        return "OpenVG";
    case QSGRendererInterface::OpenGL:
        return "OpenGL";
    case QSGRendererInterface::Direct3D11:
        return "Direct3D 11";
    case QSGRendererInterface::Vulkan:
        return "Vulkan";
    case QSGRendererInterface::Metal:
        return "Metal";
    case QSGRendererInterface::Direct3D12:
        return "Direct3D 12";
    case QSGRendererInterface::Null:
        return "null";
    case QSGRendererInterface::Unknown:
        break;
    }
    return "unknown";
}

} // namespace

void FrameLog::add(const FrameSample& sample)
{
    const std::scoped_lock lock(m_Mutex);
    m_Samples.push_back(sample);
}

std::vector<FrameSample> FrameLog::take()
{
    const std::scoped_lock lock(m_Mutex);
    return std::exchange(m_Samples, {});
}

RenderProfiler::RenderProfiler(QQuickWindow& window) : QObject(&window), m_Window(window)
{
    m_Clock.start();
    // Direct, not queued: the signals fire on the scene graph's render thread when it has one, and a zone must
    // timestamp what it brackets, not a queued copy of it.
    const auto direct = Qt::DirectConnection;
    connect(&window, &QQuickWindow::beforeSynchronizing, this, &RenderProfiler::before_synchronizing, direct);
    connect(&window, &QQuickWindow::afterSynchronizing, this, &RenderProfiler::after_synchronizing, direct);
    connect(&window, &QQuickWindow::beforeFrameBegin, this, &RenderProfiler::before_frame_begin, direct);
    connect(&window, &QQuickWindow::afterFrameEnd, this, &RenderProfiler::after_frame_end, direct);
    connect(&window, &QQuickWindow::beforeRendering, this, &RenderProfiler::before_rendering, direct);
    connect(&window, &QQuickWindow::afterRendering, this, &RenderProfiler::after_rendering, direct);
    connect(&window, &QQuickWindow::beforeRenderPassRecording, this, &RenderProfiler::before_render_pass_recording,
            direct);
    connect(&window, &QQuickWindow::afterRenderPassRecording, this, &RenderProfiler::after_render_pass_recording,
            direct);
    connect(&window, &QQuickWindow::frameSwapped, this, &RenderProfiler::frame_swapped, direct);
}

void profile_window(QQuickWindow& window)
{
    // Owned by the window, which is its parent.
    new RenderProfiler(window); // NOLINT(cppcoreguidelines-owning-memory)
}

void RenderProfiler::ensure_gpu_context()
{
    if (m_TriedGpu)
    {
        return;
    }
    m_TriedGpu = true;
    QSGRendererInterface* renderer = m_Window.rendererInterface();
    if (renderer == nullptr)
    {
        return;
    }
    // A trace says which graphics backend it measured: it changes what every number means.
    const QByteArray backend = QByteArray("graphics backend: ") + graphics_api_name(renderer->graphicsApi());
    HBM_MESSAGE_DYNAMIC((std::string_view{backend.constData(), static_cast<std::size_t>(backend.size())}));
    HBM_INFO((std::string_view{backend.constData(), static_cast<std::size_t>(backend.size())}));

    if (renderer->graphicsApi() == QSGRendererInterface::Direct3D11)
    {
        void* device = renderer->getResource(&m_Window, QSGRendererInterface::DeviceResource);
        void* device_context = renderer->getResource(&m_Window, QSGRendererInterface::DeviceContextResource);
        m_Gpu = create_d3d11_context(device, device_context);
    }
    if (m_Gpu == nullptr)
    {
        HBM_MESSAGE("render.gpu_zone_unavailable");
    }
}

void RenderProfiler::before_synchronizing()
{
    m_SyncZone.begin("render.sync");
}

void RenderProfiler::after_synchronizing()
{
    m_SyncZone.end();
}

void RenderProfiler::before_frame_begin()
{
    m_RhiFrameZone.begin("render.rhi_frame");
}

void RenderProfiler::after_frame_end()
{
    m_RhiFrameZone.end();
    // The RHI frame is over, so its command buffer reports the GPU time of the last frame that finished.
    m_LastGpuMs.store(completed_gpu_ms(), std::memory_order_relaxed);
}

void RenderProfiler::before_rendering()
{
    m_FrameBeginNs = m_Clock.nsecsElapsed();
    ensure_gpu_context();
    m_FrameZone.begin("render.frame");
    if (m_Gpu != nullptr)
    {
        m_Gpu->collect();
    }
}

void RenderProfiler::after_rendering()
{
    m_FrameZone.end();
    m_LastWorkMs.store(static_cast<double>(m_Clock.nsecsElapsed() - m_FrameBeginNs) / 1e6, std::memory_order_relaxed);
}

// The GPU zone is the scene graph's main render pass, begun and ended with the pass's own signals. The Direct3D 11 and
// OpenGL backends of Qt's RHI record their commands and replay them when the pass ends, so a timestamp issued from
// beforeRendering would run before the frame's commands and not around them. Inside the pass, beginExternalCommands()
// first replays what has been recorded, so the immediate-context timestamp lands in order.
void RenderProfiler::before_render_pass_recording()
{
    m_PassZone.begin("render.pass");
    if (m_Gpu != nullptr && !m_GpuZoneOpen)
    {
        m_Window.beginExternalCommands();
        m_Gpu->begin_zone("render.gpu");
        m_Window.endExternalCommands();
        m_GpuZoneOpen = true;
    }
}

void RenderProfiler::after_render_pass_recording()
{
    if (m_Gpu != nullptr && m_GpuZoneOpen)
    {
        m_Window.beginExternalCommands();
        m_Gpu->end_zone();
        m_Window.endExternalCommands();
        m_GpuZoneOpen = false;
    }
    m_PassZone.end();
}

double RenderProfiler::completed_gpu_ms() const
{
    // The RHI measures the GPU's time for the last frame it has finished, in seconds, when it was created with
    // timestamps (QSG_RHI_PROFILE=1) and the device has them. Below zero says it cannot.
    QRhi* rhi = m_Window.rhi();
    QRhiSwapChain* swap_chain = m_Window.swapChain();
    if (rhi == nullptr || swap_chain == nullptr || !rhi->isFeatureSupported(QRhi::Timestamps))
    {
        return -1.0;
    }
    QRhiCommandBuffer* buffer = swap_chain->currentFrameCommandBuffer();
    if (buffer == nullptr)
    {
        return -1.0;
    }
    const double seconds = buffer->lastCompletedGpuTime();
    return seconds > 0.0 ? seconds * 1000.0 : -1.0;
}

void RenderProfiler::frame_swapped()
{
    // The "UI" frame set: one mark per presentation, stamped at the swap itself.
    HBM_FRAME_MARK("UI");

    if (m_FirstFrameMs.load(std::memory_order_relaxed) < 0.0)
    {
        // What a person waits for: from the start of main() to the first picture.
        const double first = ms_since_main_started();
        m_FirstFrameMs.store(first, std::memory_order_relaxed);
        HBM_PLOT("startup.first_frame_ms", first);
    }
    FrameSample sample;
    const std::int64_t now = m_Clock.nsecsElapsed();
    if (m_LastSwapNs != 0)
    {
        sample.interval_ms = static_cast<double>(now - m_LastSwapNs) / 1e6;
        HBM_PLOT("render.frame_ms", sample.interval_ms);
    }
    m_LastSwapNs = now;
    sample.work_ms = m_LastWorkMs.load(std::memory_order_relaxed);
    sample.gpu_ms = m_LastGpuMs.load(std::memory_order_relaxed);
    if (sample.gpu_ms >= 0.0)
    {
        HBM_PLOT("GPU frame time (ms)", sample.gpu_ms);
    }
    if (sample.interval_ms > 0.0)
    {
        m_Frames.add(sample);
    }
}

} // namespace heart_boom_maths::game::profiling
