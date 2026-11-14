// SPDX-License-Identifier: GPL-3.0-or-later
// The program of the optional GPU performance test. Unlike the others it keeps the platform and the renderer the
// machine has: a real window system and the GPU's own graphics API. The test skips itself unless HBM_PERF_GPU=1.
#include <catch2/catch_session.hpp>

#include <QGuiApplication>
#include <QString>
#include <QtLogging>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

void abort_on_warning(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtDebugMsg || type == QtInfoMsg)
    {
        return;
    }
    if (context.category != nullptr && std::strcmp(context.category, "hbm.game") == 0)
    {
        std::fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
        return;
    }
    std::fprintf(stderr, "Qt message that fails the test: %s\n", message.toLocal8Bit().constData());
    std::fflush(stderr);
    std::abort();
}

} // namespace

int main(int argc, char* argv[])
{
    qInstallMessageHandler(abort_on_warning);
    // One thread draws, so the frame callbacks below run where the test reads them. The platform and the renderer are
    // left to Qt, or to whoever set QT_QPA_PLATFORM, QSG_RHI_BACKEND or QT_QUICK_BACKEND.
    if (!qEnvironmentVariableIsSet("QSG_RENDER_LOOP"))
    {
        qputenv("QSG_RENDER_LOOP", "basic");
    }
    QGuiApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
