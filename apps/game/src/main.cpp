// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/version.hpp"
#include "platform/platform.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QTimer>
#include <cstdio>
#include <string>
#include <string_view>

namespace
{

// A smoke test must end on its own even when no frame is ever drawn, or a broken build would hang the test run.
constexpr int SMOKE_TEST_LIMIT_MS = 30000;

bool has_argument(int argc, char* argv[], std::string_view wanted)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view(argv[i]) == wanted)
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    // Answered before Qt starts, so it works with no window system.
    if (has_argument(argc, argv, "--version"))
    {
        std::printf("heart-boom-maths %s (%s)\n", std::string(heart_boom_maths::VERSION_STRING).c_str(),
                    std::string(heart_boom_maths::game::platform::platform_name()).c_str());
        return 0;
    }
    const bool smoke_test = has_argument(argc, argv, "--smoke-test");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("heart-boom-maths"));
    QGuiApplication::setApplicationVersion(QString::fromUtf8(
        heart_boom_maths::VERSION_STRING.data(), static_cast<qsizetype>(heart_boom_maths::VERSION_STRING.size())));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("HeartBoomMaths", "Main");

    if (smoke_test)
    {
        const auto roots = engine.rootObjects();
        auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
        if (window == nullptr)
        {
            return 1;
        }
        QObject::connect(window, &QQuickWindow::frameSwapped, &app, [] { QCoreApplication::quit(); });
        QTimer::singleShot(SMOKE_TEST_LIMIT_MS, &app, [] { QCoreApplication::exit(1); });
    }

    return QGuiApplication::exec();
}
