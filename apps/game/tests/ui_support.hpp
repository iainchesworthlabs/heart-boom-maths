// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the ui tests share: a controller on fake services that a test drives by hand, and a harness that loads a QML
// component of the module into an offscreen window.

#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "parental_gate.hpp"
#include "services/clock.hpp"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QQuickItem>
#include <QQuickView>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>
#include <memory>
#include <string>

namespace hbm_test
{

/// A controller with every service recorded and a clock that moves only when told to.
struct UiRig
{
    /// `scores_available` makes the score service report a leaderboard, which shows Game Center in Settings.
    /// `saved_text` is the save file the store starts with: a controller reads it at once, so a saved run shows on
    /// Title.
    explicit UiRig(std::uint64_t seed = 20261114, bool scores_available = false, std::string saved_text = {});

    /// The words and the number the parental gate asks for the n-th time it deals one (from 0). The rig's gate seeds
    /// are fixed, so a test knows what to type.
    [[nodiscard]] static heart_boom_maths::game::GateChallenge gate_challenge(int nth);

    /// `haptics_available` makes the haptics service report a device that can, which shows the Haptics switch in
    /// Settings.
    UiRig(std::uint64_t seed, bool scores_available, std::string saved_text, bool haptics_available);

    /// `cloud_available` makes the cloud store report an iCloud account, which shows the iCloud sync row in Settings.
    UiRig(std::uint64_t seed, bool scores_available, std::string saved_text, bool haptics_available,
          bool cloud_available);

    /// A rig whose device already holds a save, such as a top 10 with entries in it.
    UiRig(std::uint64_t seed, std::string save_text);

    /// Plays real time in 50 ms frames, the length of one tick, as a running level would.
    void play_for_ms(int milliseconds);
    void answer_right();
    void answer_wrong();
    void play_level_right();

    heart_boom_maths::game::ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingHaptics haptics;
    RecordingLinks links;
    heart_boom_maths::MemorySaveStore store;
    FakeCloudStore cloud;
    int gate_seeds_used = 0;
    heart_boom_maths::game::GameController controller;
};

/// A component of the HeartBoomMaths module in an offscreen window. Loading fails the test when QML cannot be loaded,
/// and a QML warning aborts it (ui_main.cpp).
class QmlHarness
{
public:
    /// `component` is a file name of the module such as "AppShell.qml". Its root item needs a `game` property when
    /// `game` is given, and gets `extra_name` set to `extra` too when one is given (for example the play screen's
    /// `feedback`).
    QmlHarness(heart_boom_maths::game::GameController* game, const QString& component, QSize size,
               const QString& extra_name = {}, QObject* extra = nullptr);

    /// A harness around QML written inline. It may import HeartBoomMaths. A controller, when given, is the context
    /// property `hostGame`, so the inline QML can hand it to a component (`AppRoot { game: hostGame }`).
    static QmlHarness from_source(const QByteArray& qml, QSize size,
                                  heart_boom_maths::game::GameController* game = nullptr);

    [[nodiscard]] QQuickItem* root() const;
    [[nodiscard]] QQuickView& view();

    /// The first item or object of that objectName in the tree, or null.
    [[nodiscard]] QObject* find(const QString& object_name) const;
    [[nodiscard]] QQuickItem* find_item(const QString& object_name) const;

    /// Lets the event loop run and the scene polish and update, so a change is on screen before the test looks.
    void settle();

    void resize(QSize size);

    /// Writes what the window shows to a PNG. Returns false when it could not.
    bool grab(const QString& path);

    /// What the window shows, as an image. Null when it could not be rendered.
    QImage grab_image();

private:
    struct FromSource
    {
    };
    QmlHarness(FromSource, const QByteArray& qml, QSize size, heart_boom_maths::game::GameController* game);

    void finish_loading(const QString& what);

    std::unique_ptr<QTemporaryDir> m_Dir; // holds inline QML
    std::unique_ptr<QQuickView> m_View;
};

/// What a phone's cut-out, rounded corners and home indicator keep clear, as the safe-area frame is told by tests.
struct Margins
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

/// The window as the application builds it (AppRoot: the shell inside the safe area, the alert layer over all of it) on
/// a controller, with the safe-area margins a device would report simulated by `margins`.
[[nodiscard]] QmlHarness app_root(heart_boom_maths::game::GameController* game, QSize size, Margins margins = {});

/// The `ui` tests read a property of a QML object by name.
[[nodiscard]] QVariant property_of(const QObject* object, const char* name);

/// A string list from a QML `var` property or a QStringList one.
[[nodiscard]] QStringList string_list(const QVariant& value);

/// Clicks the middle of an item, as a mouse or a finger would.
void click(QQuickItem* item);

} // namespace hbm_test
