// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_session.hpp>

#include <QCoreApplication>
#include <QString>
#include <QtLogging>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

// A Qt warning in a test is a failure, as QT_FATAL_WARNINGS would make it, but with the text printed first. The game's
// own logging category (hbm.game) reports what it handles on purpose, such as a save that cannot be written, so it is
// printed and the test goes on.
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
    // The controller owns a QTimer, which needs an application object. No window system is involved.
    QCoreApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
