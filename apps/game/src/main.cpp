// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_controller.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "heart_boom_maths/version.hpp"
#include "platform/browser.hpp"
#include "platform/haptics_platform.hpp"
#include "platform/platform.hpp"
#include "platform/score_platform.hpp"
#include "services/app_save_store.hpp"
#include "services/audio_factory.hpp"
#include "services/backed_haptics_service.hpp"
#include "services/clock.hpp"
#include "services/file_save_store.hpp"
#include "services/first_gesture_filter.hpp"
#include "services/haptic_backend.hpp"
#include "services/qt_haptic_timer.hpp"
#include "services/score_service.hpp"
#include "services/system_link_opener.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QSysInfo>
#include <QTimer>
#include <QVariant>
#include <QtLogging>
#include <cstdio>
#include <memory>
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

    // The services outlive the controller and the engine. A smoke test must not touch the player's save, so it keeps
    // its save in memory.
    using namespace heart_boom_maths;
    game::SteadyClock clock;
    // Sound is started by the player's first touch or key press on the web, and never in a test run or without a
    // device.
    game::AudioOptions audio_options;
    audio_options.smoke_test = smoke_test;
    const std::string qt_platform = QGuiApplication::platformName().toStdString();
    audio_options.qt_platform = qt_platform;
    audio_options.requires_unlock = QSysInfo::productType() == QStringLiteral("wasm");
    game::AudioSelection audio = game::make_audio_service(audio_options);
    qInfo("%s", std::string(game::describe(audio.choice)).c_str());
    game::FirstGestureFilter first_gesture(*audio.service);
    app.installEventFilter(&first_gesture);
    // The Game Center service on an Apple device, which stays idle until its Settings switch is on, and none elsewhere.
    const std::unique_ptr<game::ScoreService> scores = game::platform::make_score_service();
    // Touch feedback: a phone's motor, or the browser's vibrate function. The desktop and a test run have none, and the
    // switch in Settings is then not shown.
    const std::unique_ptr<game::HapticBackend> haptic_backend =
        smoke_test ? std::make_unique<game::NullHapticBackend>() : game::platform::make_haptic_backend();
    game::QtHapticTimer haptic_timer;
    game::BackedHapticsService haptics(*haptic_backend, clock, haptic_timer);
    game::SystemLinkOpener links;
    // The platform chooses where the save lives: a file, or the page's storage in a browser (src/platform/<os>/).
    const game::platform::SaveStorage save_storage = game::platform::save_storage();
    std::unique_ptr<game::AppSaveStore> app_store;
    MemorySaveStore memory_store;
    if (!smoke_test)
    {
        app_store = game::make_save_store(save_storage, game::FileSaveStore::default_directory());
    }
    SaveStore& store = smoke_test ? static_cast<SaveStore&>(memory_store) : static_cast<SaveStore&>(*app_store);
    game::GameServices services;
    services.store = &store;
    services.quarantine = app_store.get();
    services.audio = audio.service.get();
    services.scores = scores.get();
    services.haptics = &haptics;
    services.links = &links;
    services.clock = &clock;
    game::GameController controller(services, game::GameOptions::with_random_seed());
    // Leaving the Active state (background, lock screen, a window losing focus) pauses and saves.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &controller,
                     [&controller](Qt::ApplicationState state)
                     { controller.on_application_state(state == Qt::ApplicationActive); });
    // Qt's browser platform does not report a hidden tab as a change of application state, so the page's visibility
    // takes the same path. It does nothing off the web.
    game::platform::watch_page_visibility([&controller](bool visible) { controller.on_application_state(visible); });

    QQmlApplicationEngine engine;
    // The window takes the controller as a required property, so qmllint and the QML compiler know its type.
    engine.setInitialProperties({{QStringLiteral("game"), QVariant::fromValue(&controller)}});
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

    const int exit_code = QGuiApplication::exec();
    // The controller is about to go; a late visibility event must not reach it.
    game::platform::watch_page_visibility({});
    return exit_code;
}
