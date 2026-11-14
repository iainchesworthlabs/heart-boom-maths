// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the ui tests share: a controller on fake services that a test drives by hand, and a harness that loads a QML
// component of the module into an offscreen window.

#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "services/clock.hpp"

#include <QByteArray>
#include <QObject>
#include <QQuickItem>
#include <QQuickView>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>
#include <memory>

namespace hbm_test
{

/// A controller with every service recorded and a clock that moves only when told to.
struct UiRig
{
    explicit UiRig(std::uint64_t seed = 20261114);

    /// Plays real time in 50 ms frames, the length of one tick, as a running level would.
    void play_for_ms(int milliseconds);
    void answer_right();
    void answer_wrong();
    void play_level_right();

    heart_boom_maths::game::ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    heart_boom_maths::MemorySaveStore store;
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

    /// A harness around QML written inline, for a component that needs no controller. It may import HeartBoomMaths.
    static QmlHarness from_source(const QByteArray& qml, QSize size);

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

private:
    struct FromSource
    {
    };
    QmlHarness(FromSource, const QByteArray& qml, QSize size);

    void finish_loading(const QString& what);

    std::unique_ptr<QTemporaryDir> m_Dir; // holds inline QML
    std::unique_ptr<QQuickView> m_View;
};

/// The `ui` tests read a property of a QML object by name.
[[nodiscard]] QVariant property_of(const QObject* object, const char* name);

/// A string list from a QML `var` property or a QStringList one.
[[nodiscard]] QStringList string_list(const QVariant& value);

/// Clicks the middle of an item, as a mouse or a finger would.
void click(QQuickItem* item);

} // namespace hbm_test
