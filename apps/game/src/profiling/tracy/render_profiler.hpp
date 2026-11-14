// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gpu_context.hpp"
#include "profiling.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

class QQuickWindow;

namespace heart_boom_maths::game::profiling
{

/// One presented frame as the scene graph saw it.
struct FrameSample
{
    double interval_ms = 0.0; // since the previous presentation
    double work_ms = 0.0; // the render thread's time recording the frame, beforeRendering to afterRendering; not the
                          // wait for the display
    double gpu_ms = -1.0; // the GPU's time for the frame before, or below zero when the backend cannot say
    int batches = -1;     // draw batches the renderer made for it, or below zero when it cannot say
    int nodes = -1;       // scene graph nodes it drew, or below zero
};

/// The frames presented since the last take(). Written on the render thread, read on the main one.
class FrameLog
{
public:
    void add(const FrameSample& sample);
    [[nodiscard]] std::vector<FrameSample> take();

private:
    std::mutex m_Mutex;
    std::vector<FrameSample> m_Samples;
};

/// Brackets the phases of every frame of one QQuickWindow with Tracy zones: the synchronisation of the GUI thread with
/// the scene graph, the RHI frame, the scene graph's render and its main render pass, plus a frame mark and a plot of
/// the time between presentations. Where the graphics backend is Direct3D 11 a GPU zone is drawn around the pass; on
/// any backend that has timestamps the GPU's time for the last completed frame is plotted as "GPU frame time (ms)".
///
/// The handlers are connected directly, so they run on the scene graph's render thread, where a GPU context must live.
/// Installed from main() by profile_window(), only while the profiler runs.
class RenderProfiler : public QObject
{
    Q_OBJECT

public:
    explicit RenderProfiler(QQuickWindow& window);

    /// Milliseconds from the start of main() to the first presented frame, or below zero before there is one.
    [[nodiscard]] double first_frame_ms() const
    {
        return m_FirstFrameMs.load(std::memory_order_relaxed);
    }

    /// The frames since the last call.
    [[nodiscard]] std::vector<FrameSample> take_frames()
    {
        return m_Frames.take();
    }

private:
    void ensure_gpu_context();
    void before_synchronizing();
    void after_synchronizing();
    void before_frame_begin();
    void after_frame_end();
    void before_rendering();
    void after_rendering();
    void before_render_pass_recording();
    void after_render_pass_recording();
    void frame_swapped();
    [[nodiscard]] double completed_gpu_ms() const;

    QQuickWindow& m_Window;
    GpuContext* m_Gpu = nullptr;
    bool m_GpuZoneOpen = false;
    bool m_TriedGpu = false;
    profile::ZoneSpan m_SyncZone;
    profile::ZoneSpan m_RhiFrameZone;
    profile::ZoneSpan m_FrameZone;
    profile::ZoneSpan m_PassZone;
    QElapsedTimer m_Clock;
    std::int64_t m_LastSwapNs = 0;
    std::atomic<double> m_LastGpuMs{-1.0};
    std::atomic<double> m_LastWorkMs{0.0};
    std::int64_t m_FrameBeginNs = 0;
    std::atomic<double> m_FirstFrameMs{-1.0};
    FrameLog m_Frames;
};

} // namespace heart_boom_maths::game::profiling
