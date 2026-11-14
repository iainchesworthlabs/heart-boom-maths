// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_controller.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "heart_boom_maths/version.hpp"
#include "platform/browser.hpp"
#include "platform/haptics_platform.hpp"
#include "platform/platform.hpp"
#include "platform/score_platform.hpp"
#include "profile_build.hpp"
#include "profiling.hpp"
#include "profiling/profile_switch.hpp"
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

#include <QByteArray>
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
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace profiling = heart_boom_maths::game::profiling;

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

/// The whole number after `name`, or 0.
int argument_value(int argc, char* argv[], std::string_view name)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string_view(argv[i]) == name)
        {
            return std::max(0, QString::fromUtf8(argv[i + 1]).toInt());
        }
    }
    return 0;
}

void wait_for_the_profiler(const profiling::Session& profiler, const profiling::StartupDecision& decision)
{
    std::fputs("Tracy profiler started: connect a Tracy 0.14 profiler to 127.0.0.1:8086\n", stderr);
    if (!decision.wait)
    {
        return;
    }
    // A person is never left with a window that does not open: the wait ends on its own.
    constexpr std::chrono::seconds WAIT_LIMIT(30);
    std::fputs("Waiting up to 30 s for a profiler to connect (--profile-wait)\n", stderr);
    const bool connected = profiler.wait_for_profiler(WAIT_LIMIT);
    std::fputs(connected ? "A profiler connected\n" : "No profiler connected: starting anyway\n", stderr);
}

} // namespace

int main(int argc, char* argv[])
{
    profiling::note_main_started();
    // Answered before Qt starts, so it works with no window system.
    if (has_argument(argc, argv, "--version"))
    {
        std::printf("heart-boom-maths %s (%s)\n", std::string(heart_boom_maths::VERSION_STRING).c_str(),
                    std::string(heart_boom_maths::game::platform::platform_name()).c_str());
        return 0;
    }
    const bool smoke_test = has_argument(argc, argv, "--smoke-test");
    // A scripted profiling run (profiling builds only) plays games that are nobody's: they must not reach the player's
    // save.
    const bool scripted_run = profiling::ENABLED && has_argument(argc, argv, "--profile-run");

    // The profiler, if one is asked for, is the first thing started and the last stopped, so that it outlives every
    // thread that can be inside a zone. A build without Tracy has nothing to start, and this does nothing.
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    const QByteArray profile_environment = qgetenv(std::string(profiling::ENVIRONMENT_VARIABLE).c_str());
    const profiling::StartupDecision profile_decision = profiling::decide_startup(
        profiling::AUTOSTART, profile_environment.isNull() ? nullptr : profile_environment.constData(), arguments);
    const profiling::Session profiler(profile_decision);
    if (profiler.running())
    {
        wait_for_the_profiler(profiler, profile_decision);
    }
    profiling::prepare_scene_graph_log(arguments);
    HBM_ZONE_FN();
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
    if (!smoke_test && !scripted_run)
    {
        app_store = game::make_save_store(save_storage, game::FileSaveStore::default_directory());
    }
    SaveStore& store =
        smoke_test || scripted_run ? static_cast<SaveStore&>(memory_store) : static_cast<SaveStore&>(*app_store);
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
    {
        HBM_ZONE("startup.load_qml");
        engine.loadFromModule("HeartBoomMaths", "Main");
    }

    const auto roots = engine.rootObjects();
    auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
    if ((profiler.running() || scripted_run) && window != nullptr)
    {
        profiling::profile_window(*window);
    }
    if (scripted_run)
    {
        if (window == nullptr || !profiling::start_profile_run(controller, *window, arguments))
        {
            std::fputs("--profile-run needs a profiling build and a window\n", stderr);
            return 2;
        }
    }
    else if (std::ranges::find(arguments, profiling::RUN_FLAG) != arguments.end())
    {
        std::fputs("--profile-run needs a profiling build: configure a profile preset (docs/profiling.md)\n", stderr);
        return 2;
    }

    if (smoke_test)
    {
        if (window == nullptr)
        {
            return 1;
        }
        // The test that looks for an open socket asks the program to stay a while after its first frame
        // (--smoke-hold-ms).
        const int hold_ms = argument_value(argc, argv, "--smoke-hold-ms");
        QObject::connect(
            window, &QQuickWindow::frameSwapped, &app,
            [&app, hold_ms] { QTimer::singleShot(hold_ms, &app, &QCoreApplication::quit); }, Qt::SingleShotConnection);
        QTimer::singleShot(SMOKE_TEST_LIMIT_MS, &app, [] { QCoreApplication::exit(1); });
    }

    const int exit_code = QGuiApplication::exec();
    // The controller is about to go; a late visibility event must not reach it.
    game::platform::watch_page_visibility({});
    return exit_code;
}
