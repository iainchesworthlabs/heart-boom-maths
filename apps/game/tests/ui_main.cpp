// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_session.hpp>

#include <QGuiApplication>
#include <QString>
#include <QtLogging>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

// A QML warning (a binding loop, an unknown property, a type that cannot load) fails a ui test. Like test_main.cpp, the
// message is printed first. The game's own logging category reports what it handles on purpose and does not fail one.
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
    // No window appears and no GPU is needed: the offscreen platform and the software renderer, unless the caller chose
    // others (a hand run that wants to see the real renderer). The fonts are the game's own, for there are no system
    // ones.
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
