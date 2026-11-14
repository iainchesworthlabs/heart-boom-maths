// SPDX-License-Identifier: GPL-3.0-or-later
// The optional GPU frame-time test: the play screen drawn by the machine's real renderer into a real window, with the
// time the GPU took for each frame read from QRhi::lastCompletedGpuTime(). It reports and never fails (a GPU, its
// driver and the display change the numbers more than any change of this code), and it skips itself with a message
// unless
//
//   - HBM_PERF_GPU=1 is set,
//   - the platform is a window system (not offscreen) and Qt Quick runs on the GPU (not the software renderer), and
//   - the graphics API gives timestamps (QRhi::Timestamps).
//
// Run it:   HBM_PERF_GPU=1 ctest --preset <perf preset> -R app.gpu --verbose
#include "game_controller.hpp"
#include "perf_harness.hpp"
#include "ui_support.hpp"

#include <rhi/qrhi.h>

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickGraphicsConfiguration>
#include <QQuickView>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTest>
#include <QUrl>
#include <QVariantMap>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace hbm_perf;
using namespace hbm_test;

TEST_CASE("The play screen's GPU frame time on this machine's renderer", "[perf-gpu]")
{
    if (qEnvironmentVariable("HBM_PERF_GPU") != QLatin1String("1"))
    {
        SKIP("optional: set HBM_PERF_GPU=1 to run it on a machine with a window system and a GPU");
    }
    if (QGuiApplication::platformName() == QLatin1String("offscreen") || QGuiApplication::platformName().isEmpty())
    {
        SKIP("no window system: the platform is '" << QGuiApplication::platformName().toStdString()
                                                   << "'. Unset QT_QPA_PLATFORM on a desktop session.");
    }

    UiRig rig;
    QQuickView view;
    QQuickGraphicsConfiguration graphics;
    graphics.setTimestamps(true);
    view.setGraphicsConfiguration(graphics);
    view.setInitialProperties({{QStringLiteral("game"), QVariant::fromValue(&rig.controller)}});
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(390, 844);
    view.setSource(QUrl(QStringLiteral("qrc:/qt/qml/HeartBoomMaths/AppShell.qml")));
    REQUIRE(view.status() == QQuickView::Ready);

    // The renderer exists once the scene graph has started; its GPU time is read after each frame.
    QRhi* rhi = nullptr;
    std::vector<double> gpu_ms;
    std::vector<double> cpu_us;
    int frames_drawn = 0;
    QObject::connect(
        &view, &QQuickWindow::sceneGraphInitialized, &view, [&]
        { rhi = static_cast<QRhi*>(view.rendererInterface()->getResource(&view, QSGRendererInterface::RhiResource)); },
        Qt::DirectConnection);
    // The command buffer of the frame being drawn holds the GPU time of the last frame that finished.
    QObject::connect(
        &view, &QQuickWindow::afterRendering, &view,
        [&]
        {
            ++frames_drawn;
            auto* swap_chain = static_cast<QRhiSwapChain*>(
                view.rendererInterface()->getResource(&view, QSGRendererInterface::RhiSwapchainResource));
            QRhiCommandBuffer* commands = swap_chain != nullptr ? swap_chain->currentFrameCommandBuffer() : nullptr;
            if (commands != nullptr)
            {
                const double seconds = commands->lastCompletedGpuTime();
                if (seconds > 0)
                {
                    gpu_ms.push_back(seconds * 1000.0);
                }
            }
        },
        Qt::DirectConnection);
    view.show();
    for (int i = 0; i < 20 && rhi == nullptr; ++i)
    {
        QTest::qWait(50);
    }
    if (rhi == nullptr)
    {
        SKIP("Qt Quick did not start on a QRhi backend: it may be on the software renderer (unset QT_QUICK_BACKEND)");
    }
    if (!rhi->isFeatureSupported(QRhi::Timestamps))
    {
        SKIP("the graphics API '" << rhi->backendName() << "' on '" << rhi->driverInfo().deviceName.toStdString()
                                  << "' gives no GPU timestamps");
    }

    rig.controller.startGame();
    constexpr int FRAMES = 300;
    QElapsedTimer wall;
    wall.start();
    std::int64_t next_ns = 0;
    for (int frame = 0; frame < FRAMES; ++frame)
    {
        QElapsedTimer work;
        work.start();
        rig.clock.advance_ns(16'666'667);
        rig.controller.advance_time();
        if (frame % 40 == 39 && rig.controller.state() == GameController::State::Playing)
        {
            rig.answer_right();
        }
        view.requestUpdate();
        QCoreApplication::processEvents();
        cpu_us.push_back(static_cast<double>(work.nsecsElapsed()) / 1000.0);
        next_ns += 16'666'667;
        const std::int64_t wait_ns = next_ns - wall.nsecsElapsed();
        if (wait_ns > 1'000'000)
        {
            QTest::qWait(static_cast<int>(wait_ns / 1'000'000));
        }
    }

    const std::string backend = rhi->backendName();
    const std::string device = rhi->driverInfo().deviceName.toStdString();
    INFO("backend " << backend << ", device " << device);
    INFO(frames_drawn << " frames drawn");
    REQUIRE_FALSE(gpu_ms.empty()); // timestamps are supported, so the GPU must have reported some frames
    const Stats gpu = stats_of(gpu_ms);
    const Stats cpu = stats_of(cpu_us);
    std::printf("GPU %s on %s: %zu frames timed\n", backend.c_str(), device.c_str(), gpu_ms.size());
    note("app.gpu.frame.play.phone.median", "gpu_frame_time", "ms", gpu.median);
    note("app.gpu.frame.play.phone.p95", "gpu_frame_time", "ms", gpu.p95);
    note("app.gpu.frame.play.phone.max", "gpu_frame_time", "ms", gpu.max);
    note("app.gpu.frame.play.phone.cpu_p95", "cpu_frame_time", "us", cpu.p95);
}
