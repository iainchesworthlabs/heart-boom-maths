// SPDX-License-Identifier: GPL-3.0-or-later
// The program of the game's performance tests: a QGuiApplication on the offscreen platform with the software renderer,
// like the ui tests, so the numbers are of the QML and the C++ and need no GPU. The budgets are for that renderer.
#include <catch2/catch_session.hpp>

#include <QGuiApplication>
#include <QString>
#include <QtLogging>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

// A Qt or QML warning fails a performance test as it fails a ui test: a screen that logs while it loads is not the
// screen being measured. The game's own category reports what it handles on purpose.
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
    const auto set_default = [](const char* name, const char* value)
    {
        if (!qEnvironmentVariableIsSet(name))
        {
            qputenv(name, value);
        }
    };
    set_default("QT_QPA_PLATFORM", "offscreen");
    set_default("QT_QUICK_BACKEND", "software");
    set_default("QSG_RENDER_LOOP", "basic");
    set_default("QT_QPA_FONTDIR", HBM_TEST_FONT_DIR);
    QGuiApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
