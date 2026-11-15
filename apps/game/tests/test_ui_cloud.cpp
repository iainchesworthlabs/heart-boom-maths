// SPDX-License-Identifier: GPL-3.0-or-later
// The iCloud sync row of Settings and its path through the parental gate, through the whole app shell on a controller
// with recorded services and a fake cloud store.
#include "heart_boom_maths/save_codec.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QByteArray>
#include <QQuickItem>
#include <QTest>
#include <string>

using namespace hbm_test;
using namespace heart_boom_maths;
using namespace heart_boom_maths::game;

namespace
{

constexpr QSize PHONE{390, 844};
constexpr QSize SHORT_PHONE{320, 568};
constexpr QSize SMALLEST{320, 480}; // the least the window may be (Main.qml)
constexpr QSize TABLET{820, 1180};
constexpr QSize DESKTOP{1280, 800};

/// The app shell on a rig whose device can (or cannot) sync, make haptics and send scores.
struct Shell
{
    explicit Shell(QSize size, bool cloud_available = true, bool haptics_available = false,
                   bool scores_available = false)
        : rig(20261115, scores_available, {}, haptics_available, cloud_available),
          h(&rig.controller, QStringLiteral("AppShell.qml"), size), window_size(size)
    {
    }

    [[nodiscard]] bool visible(const char* name) const
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        return item != nullptr && item->isVisible();
    }

    [[nodiscard]] QQuickItem* item(const char* name) const
    {
        QQuickItem* found = h.find_item(QLatin1String(name));
        REQUIRE(found != nullptr);
        return found;
    }

    [[nodiscard]] QString text_of(const char* name) const
    {
        return property_of(h.find(QLatin1String(name)), "text").toString();
    }

    void tap(const char* name)
    {
        click(item(name));
        h.settle();
    }

    void keypad(const QString& digits)
    {
        for (const QChar digit : digits)
        {
            tap(QStringLiteral("key%1").arg(digit).toLatin1().constData());
        }
    }

    [[nodiscard]] bool inside(const QQuickItem* it) const
    {
        const QPointF top_left = it->mapToScene(QPointF(0, 0));
        const QPointF bottom_right = it->mapToScene(QPointF(it->width(), it->height()));
        return top_left.x() >= -0.5 && top_left.y() >= -0.5 && bottom_right.x() <= window_size.width() + 0.5 &&
               bottom_right.y() <= window_size.height() + 0.5;
    }

    [[nodiscard]] qreal top_of(const char* name) const
    {
        return item(name)->mapToScene(QPointF(0, 0)).y();
    }

    [[nodiscard]] qreal bottom_of(const char* name) const
    {
        return top_of(name) + item(name)->height();
    }

    [[nodiscard]] QString switch_word(const char* name) const
    {
        for (QQuickItem* child : item(name)->findChildren<QQuickItem*>())
        {
            if (child->objectName() == "switchLabel")
            {
                return property_of(child, "text").toString();
            }
        }
        return {};
    }

    UiRig rig;
    QmlHarness h;
    QSize window_size;
};

QString right_number(int nth)
{
    return QString::number(UiRig::gate_challenge(nth).number);
}

} // namespace

TEST_CASE("The iCloud sync row is not shown where the device cannot sync", "[ui][settings][cloud]")
{
    const QSize size = GENERATE(PHONE, TABLET, DESKTOP);
    Shell s(size, false);
    s.tap("settingsButton");
    REQUIRE(s.visible("settingsScreen"));
    CHECK_FALSE(s.rig.controller.cloud_sync_available());
    CHECK_FALSE(s.visible("cloudSyncRow"));
    CHECK_FALSE(s.visible("cloudSyncSwitch"));
    CHECK(s.visible("soundRow"));
    CHECK(s.visible("aboutButton"));
    CHECK(s.rig.cloud.calls() == 0);
}

TEST_CASE("The iCloud sync row sits after Game Center and before ABOUT, in both size classes", "[ui][settings][cloud]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
        bool haptics;
        bool game_center;
    };
    const Arrangement arrangement = GENERATE(
        Arrangement{"phone", PHONE, false, false}, Arrangement{"phone with everything", PHONE, true, true},
        Arrangement{"short phone", SHORT_PHONE, true, true}, Arrangement{"smallest window", SMALLEST, false, true},
        Arrangement{"tablet", TABLET, true, true}, Arrangement{"desktop", DESKTOP, false, false});
    CAPTURE(arrangement.name);
    Shell s(arrangement.size, true, arrangement.haptics, arrangement.game_center);
    s.tap("settingsButton");
    REQUIRE(s.visible("settingsScreen"));

    REQUIRE(s.visible("cloudSyncRow"));
    REQUIRE(s.visible("cloudSyncSwitch"));
    CHECK(s.top_of("musicRow") < s.top_of("cloudSyncRow"));
    if (arrangement.game_center)
    {
        CHECK(s.top_of("gameCenterRow") < s.top_of("cloudSyncRow"));
        CHECK(s.bottom_of("gameCenterRow") <= s.top_of("cloudSyncRow") + 0.5);
    }
    CHECK(s.top_of("cloudSyncRow") < s.top_of("aboutButton"));
    CHECK(s.bottom_of("cloudSyncRow") <= s.top_of("aboutButton") + 0.5);
    CHECK(s.bottom_of("aboutButton") <= s.top_of("buildFooter") + 0.5);

    // It is a row like the others: the same switch, a name a screen reader reads, OFF in words, and inside the window.
    CHECK(s.item("cloudSyncSwitch")->width() == s.item("musicSwitch")->width());
    if (arrangement.size.width() >= PHONE.width())
    {
        CHECK(s.item("cloudSyncSwitch")->height() >= 44);
    }
    CHECK(property_of(s.item("cloudSyncSwitch"), "accessibleName").toString() == "iCloud sync");
    CHECK(s.switch_word("cloudSyncSwitch") == "OFF");
    CHECK_FALSE(property_of(s.h.find("cloudSyncSwitch"), "checked").toBool());
    for (const char* name : {"soundRow", "musicRow", "cloudSyncRow", "aboutButton", "buildFooter", "backButton"})
    {
        INFO(name);
        CHECK(s.inside(s.item(name)));
    }
    if (arrangement.game_center)
    {
        CHECK(s.inside(s.item("gameCenterRow")));
    }
    if (arrangement.haptics)
    {
        CHECK(s.inside(s.item("hapticsRow")));
    }
}

TEST_CASE("The iCloud sync note says in plain words that a grown-up is needed", "[ui][settings][cloud]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    REQUIRE(s.visible("cloudSyncRow"));
    QString note;
    for (QQuickItem* child : s.item("cloudSyncRow")->findChildren<QQuickItem*>())
    {
        if (child->objectName() == "settingHint")
        {
            note = property_of(child, "text").toString();
        }
    }
    CHECK(note.contains("Needs a grown-up"));
    CHECK(note.contains("devices"));
}

TEST_CASE("Turning iCloud sync on goes through the parental gate and leaves it on", "[ui][settings][gate][cloud]")
{
    const QSize size = GENERATE(PHONE, DESKTOP);
    Shell s(size);
    s.tap("settingsButton");
    REQUIRE(s.visible("cloudSyncSwitch"));
    CHECK(s.rig.cloud.handlers_installed == 0); // nothing listens until the switch is on

    s.tap("cloudSyncSwitch");
    REQUIRE(s.rig.controller.gate_active());
    CHECK(s.rig.controller.gate_purpose() == GameController::GatePurpose::CloudSync);
    CHECK(s.visible("gateScreen"));
    CHECK_FALSE(s.rig.controller.cloud_sync_enabled());
    CHECK(s.rig.cloud.calls() == 0);
    CHECK(s.text_of("gatePromptText") == "To turn on iCloud sync, type this number:");
    CHECK(property_of(s.h.find("openButton"), "text").toString() == "TURN ON");
    CHECK_FALSE(property_of(s.h.find("openButton"), "enabled").toBool());

    s.keypad(right_number(0));
    CHECK(property_of(s.h.find("openButton"), "enabled").toBool());
    CHECK(s.rig.controller.gate_active()); // typing the right number is not yet a yes
    s.tap("openButton");

    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK(s.h.find("gateScreen") == nullptr);
    CHECK(s.visible("settingsScreen"));
    CHECK(s.rig.controller.cloud_sync_enabled());
    CHECK(property_of(s.h.find("cloudSyncSwitch"), "checked").toBool());
    CHECK(s.switch_word("cloudSyncSwitch") == "ON");
    CHECK(s.rig.cloud.handlers_installed == 1);
    CHECK(s.rig.links.opened.empty()); // a switch is not a link

    // Turning it off is immediate and asks nothing.
    s.tap("cloudSyncSwitch");
    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK_FALSE(s.rig.controller.cloud_sync_enabled());
    CHECK_FALSE(property_of(s.h.find("cloudSyncSwitch"), "checked").toBool());
    CHECK(s.switch_word("cloudSyncSwitch") == "OFF");
    CHECK(s.rig.cloud.handlers_removed == 1);
}

TEST_CASE("A wrong number or CANCEL at the gate leaves iCloud sync off and the store untouched",
          "[ui][settings][gate][cloud]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    s.tap("cloudSyncSwitch");
    REQUIRE(s.visible("gateScreen"));

    const int wrong = UiRig::gate_challenge(0).number == 999 ? 998 : UiRig::gate_challenge(0).number + 1;
    s.keypad(QString::number(wrong));
    CHECK(s.rig.controller.gate_active()); // a new number was dealt
    CHECK_FALSE(s.rig.controller.cloud_sync_enabled());

    s.tap("cancelButton");
    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK(s.h.find("gateScreen") == nullptr);
    CHECK_FALSE(s.rig.controller.cloud_sync_enabled());
    CHECK(s.visible("settingsScreen"));
    CHECK(s.rig.cloud.calls() == 0);
}

// ---- Screenshots of Settings with the iCloud row, for the designer to look at. Run by hand: the test does nothing
// unless HBM_SCREENSHOT_DIR names a folder, and it is hidden from the ordinary run (docs/testing.md).
TEST_CASE("Screenshots of Settings with the iCloud sync row", "[.][screenshots]")
{
    const QString dir = qEnvironmentVariable("HBM_SCREENSHOT_DIR");
    if (dir.isEmpty())
    {
        return;
    }
    struct Shape
    {
        const char* label;
        QSize size;
    };
    // An iPhone has haptics, Game Center and iCloud; the regular size is shown as an iPad would.
    for (const Shape shape : {Shape{"compact", PHONE}, Shape{"regular", DESKTOP}})
    {
        Shell s(shape.size, true, true, true);
        s.tap("settingsButton");
        REQUIRE(s.visible("cloudSyncRow"));
        CHECK(s.h.grab(QStringLiteral("%1/settings-icloud-%2.png").arg(dir, QLatin1String(shape.label))));
    }
}
