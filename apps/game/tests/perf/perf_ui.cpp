// SPDX-License-Identifier: GPL-3.0-or-later
// Performance tests of the screens on the offscreen platform with the SOFTWARE renderer: how long each screen takes
// from the moment it is asked for to its first polished frame, the frame time of the play screen at three sizes while a
// level plays with the held feedback animations, and whether switching between screens a hundred times leaks. The
// budgets are for the software renderer; a phone's GPU is a different machine (see perf_gpu.cpp for the optional GPU
// run).
#include "game_controller.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "perf_harness.hpp"
#include "process_memory.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QMetaObject>
#include <QObject>
#include <QQuickItem>
#include <QQuickView>
#include <QSize>
#include <QString>
#include <QTest>
#include <QVariantList>
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace hbm_perf;
using namespace hbm_test;
using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using State = GameController::State;

namespace
{

constexpr QSize PHONE{390, 844};
constexpr QSize TABLET{820, 1180};
constexpr QSize DESKTOP{1280, 800};

struct SizeCase
{
    const char* name;
    QSize size;
};

constexpr int REPETITIONS = 7;
constexpr double FRAME_BUDGET_US = 16'667.0; // one frame at 60 a second
constexpr int LONG_HOLD_MS = 60'000;         // a hold that only an explicit clear() ends, so a screen switch is on cue

/// The app shell with the answer holds under the test's control.
struct Shell
{
    Shell(QSize size, std::string save = {})
        : rig(20261114, false, std::move(save)), h(&rig.controller, QStringLiteral("AppShell.qml"), size)
    {
        feedback = h.find("answerFeedback");
        REQUIRE(feedback != nullptr);
    }

    [[nodiscard]] QString screen() const
    {
        return property_of(h.root(), "screen").toString();
    }

    void hold_feedback()
    {
        feedback->setProperty("rightHoldMs", LONG_HOLD_MS);
        feedback->setProperty("wrongHoldMs", LONG_HOLD_MS);
        feedback->setProperty("boomHoldMs", LONG_HOLD_MS);
    }

    void release_feedback()
    {
        QMetaObject::invokeMethod(feedback, "clear");
    }

    void call(const char* function)
    {
        QMetaObject::invokeMethod(h.root(), function);
    }

    void tap(const char* name)
    {
        click(h.find_item(QLatin1String(name)));
        h.settle();
    }

    UiRig rig;
    QmlHarness h;
    QObject* feedback = nullptr;
};

/// The nanoseconds `action` takes together with the settling of the scene after it: the frame in which the new screen
/// first appears, polished and drawn.
template <typename Action>
double time_to_frame(Shell& s, Action&& action)
{
    QElapsedTimer timer;
    timer.start();
    action();
    s.h.settle();
    return static_cast<double>(timer.nsecsElapsed());
}

std::string saved_top10_text()
{
    SaveData data;
    // Nine entries, so that a score of a game played for a moment still earns a place and the name screen shows.
    const char* names[] = {"BOMB SQUAD", "ROCKET", "DAD", "MUM", "SPARKY", "NINJA", "CAT", "ALEX", "SUNNY"};
    for (int i = 0; i < 9; ++i)
    {
        data.shared.top10.push_back({names[i], 5000 - 500 * i, "", i + 1});
    }
    return *serialise(data);
}

void report_screens(const std::map<std::string, std::vector<double>>& samples, const char* suffix)
{
    for (const auto& [screen, values] : samples)
    {
        check("app.screen." + screen + suffix, "ns_to_first_frame", "ns", stats_of(values));
    }
}

/// Plays frames paced at 60 a second, each one the work a real frame does (time advances, the controller ticks, events
/// run, the scene is polished, synchronised and drawn), with a right answer every 40 frames so the held feedback
/// animations run. Returns the work of each frame in microseconds.
std::vector<double> play_frames(Shell& s, int frames)
{
    constexpr int ANSWER_EVERY = 40;
    std::vector<double> times;
    times.reserve(static_cast<std::size_t>(frames));
    QElapsedTimer wall;
    wall.start();
    std::int64_t next_frame_ns = 0;
    for (int frame = 0; frame < frames; ++frame)
    {
        QElapsedTimer work;
        work.start();
        s.rig.clock.advance_ns(16'666'667);
        s.rig.controller.advance_time();
        if (frame % ANSWER_EVERY == ANSWER_EVERY - 1 && s.rig.controller.state() == State::Playing)
        {
            s.rig.answer_right();
        }
        QCoreApplication::processEvents();
        const QImage image = s.h.view().grabWindow();
        REQUIRE_FALSE(image.isNull());
        times.push_back(static_cast<double>(work.nsecsElapsed()) / 1000.0);

        // Wait for the next 60 Hz boundary, running the event loop so the holds and animations go on as in a real
        // window.
        next_frame_ns += 16'666'667;
        const std::int64_t late = next_frame_ns - wall.nsecsElapsed();
        if (late > 1'000'000)
        {
            QTest::qWait(static_cast<int>(late / 1'000'000));
        }
    }
    return times;
}

} // namespace

TEST_CASE("Every screen reaches its first frame fast on the software renderer", "[perf][app][ui]")
{
    std::map<std::string, std::vector<double>> samples;
    for (int rep = 0; rep < REPETITIONS + 1; ++rep)
    {
        const bool recorded = rep > 0; // the first pass warms the disk cache and the font engine
        const auto record = [&](const std::string& screen, double ns)
        {
            if (recorded)
            {
                samples[screen].push_back(ns);
            }
        };

        // Title: the window, the QML engine, the shell and the title screen, all cold, as at the start of the game.
        QElapsedTimer start_timer;
        start_timer.start();
        Shell s(PHONE, saved_top10_text());
        record("title.phone", static_cast<double>(start_timer.nsecsElapsed()));
        REQUIRE(s.screen() == "title");
        s.hold_feedback();

        // Play, level clear.
        record("play.phone", time_to_frame(s, [&] { s.rig.controller.startGame(); }));
        REQUIRE(s.screen() == "play");
        s.rig.play_level_right(); // the shell holds the play screen for the last answer
        record("level_clear.phone", time_to_frame(s, [&] { s.release_feedback(); }));
        REQUIRE(s.screen() == "levelClear");

        // Name entry: three right answers on level 2 and then the bomb.
        s.rig.controller.nextLevel();
        s.h.settle();
        for (int i = 0; i < 3; ++i)
        {
            s.rig.answer_right();
        }
        s.rig.play_for_ms(61'000);
        record("name_entry.phone", time_to_frame(s, [&] { s.release_feedback(); }));
        REQUIRE(s.screen() == "nameEntry");
        s.rig.controller.skipName();
        s.h.settle();

        // Top 10 and About, as pages over the screen.
        record("top10.phone", time_to_frame(s, [&] { s.call("showTop10"); }));
        REQUIRE(s.screen() == "top10");
        s.h.root()->setProperty("pages", QVariantList());
        s.h.settle();
        s.call("showSettings");
        s.h.settle();
        record("about.phone", time_to_frame(s, [&] { s.call("showAbout"); }));
        record("about_notices.phone", time_to_frame(s, [&] { s.tap("noticesButton"); }));
        REQUIRE(s.h.find_item(QStringLiteral("viewerText")) != nullptr);
    }
    report_screens(samples, ".software");
}

TEST_CASE("The play screen reaches its first frame fast at tablet and desktop size", "[perf][app][ui]")
{
    for (const SizeCase& c : {SizeCase{"tablet", TABLET}, SizeCase{"desktop", DESKTOP}})
    {
        const char* name = c.name;
        const QSize size = c.size;
        std::vector<double> values;
        for (int rep = 0; rep < REPETITIONS + 1; ++rep)
        {
            Shell s(size);
            const double ns = time_to_frame(s, [&] { s.rig.controller.startGame(); });
            REQUIRE(s.screen() == "play");
            if (rep > 0)
            {
                values.push_back(ns);
            }
        }
        check(std::string("app.screen.play.") + name + ".software", "ns_to_first_frame", "ns", stats_of(values));
    }
}

TEST_CASE("The play screen holds its frame time at every size while a level plays", "[perf][app][ui][frames]")
{
    constexpr int FRAMES = 200;
    constexpr int RUNS = 3;
    for (const SizeCase& c : {SizeCase{"phone", PHONE}, SizeCase{"tablet", TABLET}, SizeCase{"desktop", DESKTOP}})
    {
        const char* name = c.name;
        const QSize size = c.size;
        std::vector<double> p95_per_run;
        std::vector<double> all;
        for (int run = 0; run < RUNS + 1; ++run)
        {
            Shell s(size);
            s.rig.controller.startGame();
            s.h.settle();
            REQUIRE(s.screen() == "play");
            std::vector<double> frames = play_frames(s, FRAMES);
            if (run == 0)
            {
                continue; // the first run pays for the first use of every glyph and image
            }
            p95_per_run.push_back(stats_of(frames).p95);
            all.insert(all.end(), frames.begin(), frames.end());
        }
        const Stats pooled = stats_of(all);
        const auto slow =
            static_cast<double>(std::count_if(all.begin(), all.end(), [](double us) { return us > FRAME_BUDGET_US; }));
        check(std::string("app.frame.play.") + name + ".software.p95", "p95_frame_time", "us", stats_of(p95_per_run),
              {{"mean_us", pooled.mean},
               {"p99_us", pooled.p99},
               {"max_us", pooled.max},
               {"frames_over_16_7_ms", slow},
               {"frames", static_cast<double>(all.size())}});
    }
}

TEST_CASE("Switching between screens a hundred times leaves nothing behind", "[perf][app][ui][leak]")
{
    constexpr int CYCLES = 100;
    Shell s(PHONE, saved_top10_text());
    QQuickItem* root = s.h.root();

    // One cycle visits the title, the play screen with its pause overlay, the game-over card and three pages.
    const auto cycle = [&]
    {
        s.rig.controller.startGame();
        s.h.settle();
        s.rig.answer_right();
        s.h.settle();
        s.rig.controller.pause();
        s.h.settle();
        s.call("showSettings");
        s.h.settle();
        s.call("showAbout");
        s.h.settle();
        root->setProperty("pages", QVariantList());
        s.rig.controller.quit();
        s.h.settle();
        if (s.rig.controller.name_entry_pending())
        {
            s.rig.controller.skipName();
            s.h.settle();
        }
        s.call("showTitle");
        s.h.settle();
        s.call("showTop10");
        s.h.settle();
        root->setProperty("pages", QVariantList());
        s.h.settle();
    };
    const auto objects = [&]
    {
        // Every QObject below the root, which holds the items the screens made, the Loader's and the animations'.
        return static_cast<std::int64_t>(root->findChildren<QObject*>().size() +
                                         root->findChildren<QQuickItem*>().size());
    };

    for (int i = 0; i < 5; ++i)
    {
        cycle(); // warm: every component compiled, every cache filled
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const std::int64_t objects_before = objects();
    const std::int64_t process_before = process_memory_bytes();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < CYCLES; ++i)
    {
        cycle();
    }
    const double ns_per_cycle = static_cast<double>(timer.nsecsElapsed()) / CYCLES;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const std::int64_t object_growth = objects() - objects_before;
    const std::int64_t process_after = process_memory_bytes();

    check("app.leak.objects_after_100_cycles", "object_growth", "objects",
          exact(static_cast<double>(std::max<std::int64_t>(object_growth, 0))),
          {{"objects_before", static_cast<double>(objects_before)}});
    if (process_before >= 0 && process_after >= 0)
    {
        check("app.leak.process_bytes_after_100_cycles", "process_memory_growth", "bytes",
              exact(static_cast<double>(std::max<std::int64_t>(process_after - process_before, 0))),
              {{"process_bytes_before", static_cast<double>(process_before)}});
    }
    else
    {
        WARN("process memory is not measured on this platform; only the object count is checked");
    }
    note("app.screen_switch.cycle", "ns_per_cycle", "ns", ns_per_cycle);
}
