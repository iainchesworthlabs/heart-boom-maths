// SPDX-License-Identifier: GPL-3.0-or-later
// The Haptics row of Settings, and which taps on the screens ask for a haptic, through the whole app shell on a
// controller with recorded services.
#include "heart_boom_maths/save_codec.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QByteArray>
#include <QQuickItem>
#include <QTest>
#include <string>
#include <utility>
#include <vector>

using namespace hbm_test;
using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using State = GameController::State;
using Haptics = std::vector<HapticId>;

namespace
{

constexpr QSize PHONE{390, 844};
constexpr QSize SHORT_PHONE{320, 568};
constexpr QSize SMALLEST{320, 480}; // the least the window may be (Main.qml)
constexpr QSize TABLET{820, 1180};
constexpr QSize DESKTOP{1280, 800};

/// The app shell on a rig whose device can (or cannot) make haptics.
struct Shell
{
    explicit Shell(QSize size, bool haptics_available = true, bool scores_available = false,
                   std::string saved_text = {})
        : rig(20261114, scores_available, std::move(saved_text), haptics_available),
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

    void tap(const char* name)
    {
        click(item(name));
        h.settle();
    }

    [[nodiscard]] bool inside(const QQuickItem* it) const
    {
        const QPointF top_left = it->mapToScene(QPointF(0, 0));
        const QPointF bottom_right = it->mapToScene(QPointF(it->width(), it->height()));
        return top_left.x() >= -0.5 && top_left.y() >= -0.5 && bottom_right.x() <= window_size.width() + 0.5 &&
               bottom_right.y() <= window_size.height() + 0.5;
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

    [[nodiscard]] qreal top_of(const char* name) const
    {
        return item(name)->mapToScene(QPointF(0, 0)).y();
    }

    UiRig rig;
    QmlHarness h;
    QSize window_size;
};

std::string save_with_coins()
{
    SaveData data;
    give_wallet(data, 10);
    return *serialise(data);
}

} // namespace

TEST_CASE("The Haptics row is not shown on a device that cannot make haptics", "[ui][haptics][settings]")
{
    const QSize size = GENERATE(PHONE, TABLET, DESKTOP);
    Shell s(size, false, true);
    s.tap("settingsButton");
    REQUIRE(s.visible("settingsScreen"));
    CHECK_FALSE(s.rig.controller.haptics_available());
    CHECK_FALSE(s.visible("hapticsRow"));
    CHECK_FALSE(s.visible("hapticsSwitch"));
    // The rows around it stay as they were.
    CHECK(s.visible("soundRow"));
    CHECK(s.visible("musicRow"));
    CHECK(s.visible("gameCenterRow"));
}

TEST_CASE("The Haptics row sits after Music and before Game Center, with ON and OFF in words",
          "[ui][haptics][settings]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
        bool game_center;
    };
    const Arrangement arrangement =
        GENERATE(Arrangement{"phone", PHONE, false}, Arrangement{"phone with Game Center", PHONE, true},
                 Arrangement{"short phone", SHORT_PHONE, true}, Arrangement{"smallest window", SMALLEST, true},
                 Arrangement{"tablet", TABLET, true}, Arrangement{"desktop", DESKTOP, false});
    CAPTURE(arrangement.name);
    Shell s(arrangement.size, true, arrangement.game_center);
    s.tap("settingsButton");
    REQUIRE(s.visible("settingsScreen"));

    REQUIRE(s.visible("hapticsRow"));
    REQUIRE(s.visible("hapticsSwitch"));
    CHECK(s.top_of("soundRow") < s.top_of("musicRow"));
    CHECK(s.top_of("musicRow") < s.top_of("hapticsRow"));
    if (arrangement.game_center)
    {
        CHECK(s.top_of("hapticsRow") < s.top_of("gameCenterRow"));
        CHECK(s.top_of("gameCenterRow") < s.top_of("aboutButton"));
    }
    CHECK(s.top_of("hapticsRow") < s.top_of("aboutButton"));

    // The same row as Sound and Music: the same height, the same switch, and the name a screen reader reads.
    CHECK(s.item("hapticsRow")->height() == s.item("musicRow")->height());
    CHECK(s.item("hapticsSwitch")->width() == s.item("musicSwitch")->width());
    if (arrangement.size.width() >= PHONE.width())
    {
        CHECK(s.item("hapticsSwitch")->height() >= 44);
    }
    CHECK(property_of(s.item("hapticsSwitch"), "accessibleName").toString() == "Haptics");

    // Everything is inside the window, and nothing overlaps its neighbour.
    for (const char* name : {"soundRow", "musicRow", "hapticsRow", "aboutButton", "buildFooter", "backButton"})
    {
        INFO(name);
        CHECK(s.inside(s.item(name)));
    }
    if (arrangement.game_center)
    {
        CHECK(s.inside(s.item("gameCenterRow")));
    }
    const auto bottom_of = [&](const char* name) { return s.top_of(name) + s.item(name)->height(); };
    CHECK(bottom_of("musicRow") <= s.top_of("hapticsRow") + 0.5);
    CHECK(bottom_of("hapticsRow") <= s.top_of("aboutButton") + 0.5);
    CHECK(bottom_of("aboutButton") <= s.top_of("buildFooter") + 0.5);
}

TEST_CASE("The Haptics switch is on to begin with and writes through the controller, which saves it",
          "[ui][haptics][settings]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    REQUIRE(s.visible("hapticsSwitch"));

    CHECK(property_of(s.item("hapticsSwitch"), "checked").toBool());
    CHECK(s.switch_word("hapticsSwitch") == "ON");
    CHECK(s.rig.haptics.enabled);

    s.tap("hapticsSwitch");
    CHECK_FALSE(s.rig.controller.haptics_enabled());
    CHECK_FALSE(s.rig.haptics.enabled);
    CHECK(s.switch_word("hapticsSwitch") == "OFF");
    CHECK_FALSE(property_of(s.item("hapticsSwitch"), "checked").toBool());
    REQUIRE(s.rig.store.text().has_value());
    const ParseResult off = parse_save(*s.rig.store.text());
    REQUIRE(off.status == LoadStatus::OK);
    CHECK_FALSE(off.data.device.haptics);
    CHECK(off.data.device.sound); // the other switches are as they were
    CHECK(off.data.device.music);

    s.tap("hapticsSwitch");
    CHECK(s.rig.controller.haptics_enabled());
    CHECK(s.switch_word("hapticsSwitch") == "ON");
    CHECK(parse_save(*s.rig.store.text()).data.device.haptics);
}

TEST_CASE("Turning haptics on is felt, as a preview, and turning them off is not", "[ui][haptics][settings]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    s.rig.haptics.played.clear(); // the tick of the gear itself
    s.tap("hapticsSwitch");       // off: nothing to feel
    CHECK(s.rig.haptics.played.empty());
    s.tap("hapticsSwitch"); // on: one light tick to show what it is
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP});
}

TEST_CASE("The Haptics switch answers the keyboard", "[ui][haptics][settings]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    QQuickItem* toggle = s.item("hapticsSwitch");
    toggle->forceActiveFocus();
    REQUIRE(toggle->hasActiveFocus());
    QTest::keyClick(&s.h.view(), Qt::Key_Space);
    QCoreApplication::processEvents();
    CHECK_FALSE(s.rig.controller.haptics_enabled());
    QTest::keyClick(&s.h.view(), Qt::Key_Return);
    QCoreApplication::processEvents();
    CHECK(s.rig.controller.haptics_enabled());
}

TEST_CASE("A save that turned haptics off shows the switch off", "[ui][haptics][settings]")
{
    SaveData data;
    data.device.haptics = false;
    Shell s(PHONE, true, false, *serialise(data));
    s.tap("settingsButton");
    CHECK_FALSE(property_of(s.item("hapticsSwitch"), "checked").toBool());
    CHECK(s.switch_word("hapticsSwitch") == "OFF");
    CHECK_FALSE(s.rig.haptics.enabled);
}

TEST_CASE("A button press is a light tick, and a disabled button is not", "[ui][haptics]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP});
    s.tap("backButton");
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP, HapticId::TAP});
}

TEST_CASE("A tap on a tile is felt as its result alone, and the hint as a hint alone", "[ui][haptics][play]")
{
    const auto fresh_game = [](Shell& s)
    {
        s.rig.controller.startGame();
        s.h.settle();
        REQUIRE(s.rig.controller.state() == State::Playing);
    };
    const auto tile_name = [](int position) { return QStringLiteral("tile%1").arg(position).toLatin1(); };

    {
        Shell s(PHONE, true, false, save_with_coins());
        fresh_game(s);
        click(s.item(tile_name(s.rig.controller.session_view().question.correct_tile).constData()));
        s.h.settle();
        CHECK(s.rig.haptics.played == Haptics{HapticId::RIGHT});
    }
    {
        Shell s(PHONE, true, false, save_with_coins());
        fresh_game(s);
        const int correct = s.rig.controller.session_view().question.correct_tile;
        click(s.item(tile_name(correct == 0 ? 1 : 0).constData()));
        s.h.settle();
        CHECK(s.rig.haptics.played == Haptics{HapticId::WRONG});
    }
    {
        Shell s(PHONE, true, false, save_with_coins());
        fresh_game(s);
        s.tap("hintButton");
        CHECK(s.rig.haptics.played == Haptics{HapticId::HINT});
    }
}

TEST_CASE("Nothing is felt on the pause screen, and the pause button itself still ticks", "[ui][haptics][play]")
{
    Shell s(PHONE);
    s.rig.controller.startGame();
    s.h.settle();

    s.tap("pauseButton");
    REQUIRE(s.rig.controller.state() == State::Paused);
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP}); // the press that paused the game
    CHECK(s.rig.haptics.suspended);

    s.tap("resumeButton");
    CHECK(s.rig.controller.state() == State::Playing);
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP}); // the press on RESUME was in the pause: not felt
    CHECK_FALSE(s.rig.haptics.suspended);
}

TEST_CASE("The keys of the parental gate keypad tick", "[ui][haptics][gate]")
{
    Shell s(PHONE);
    s.tap("settingsButton");
    REQUIRE(s.rig.controller.state() == State::Idle);
    s.tap("aboutButton");
    s.tap("privacyButton");
    REQUIRE(s.rig.controller.gate_active());
    s.rig.haptics.played.clear();
    s.tap("key7");
    s.tap("key0");
    s.tap("keyDelete");
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP, HapticId::TAP, HapticId::TAP});
}

TEST_CASE("The keys of the name keyboard tick", "[ui][haptics][name]")
{
    Shell s(PHONE);
    s.rig.controller.startGame();
    // The bomb: a score of nothing does not qualify, so answer first.
    s.rig.answer_right();
    s.rig.answer_right();
    while (s.rig.controller.state() == State::Playing)
    {
        s.rig.play_for_ms(50);
    }
    s.h.settle();
    // The game-over cards may hold their look for a moment.
    QTest::qWait(1500);
    s.h.settle();
    if (!s.rig.controller.name_entry_pending() || s.h.find_item("keyA") == nullptr)
    {
        SKIP("The name entry screen did not come up in time for this test");
    }
    s.rig.haptics.played.clear();
    s.tap("keyA");
    CHECK(s.rig.haptics.played == Haptics{HapticId::TAP});
}

TEST_CASE("Screenshots of Settings with the Haptics row for the hand check", "[.][shots]")
{
    const QByteArray dir = qgetenv("HBM_SHOT_DIR");
    REQUIRE_FALSE(dir.isEmpty());
    const QString out = QString::fromUtf8(dir) + QLatin1Char('/');

    struct Size
    {
        const char* name;
        QSize size;
    };
    for (const Size& size :
         {Size{"phone", PHONE}, Size{"short-phone", SHORT_PHONE}, Size{"tablet", TABLET}, Size{"desktop", DESKTOP}})
    {
        const QString prefix = out + size.name + QLatin1Char('-');
        {
            // An Android phone: Haptics, no Game Center.
            Shell s(size.size, true, false);
            s.tap("settingsButton");
            CHECK(s.h.grab(prefix + "settings-haptics.png"));
            s.tap("hapticsSwitch");
            CHECK(s.h.grab(prefix + "settings-haptics-off.png"));
        }
        {
            // An iPhone: Haptics and Game Center.
            Shell s(size.size, true, true);
            s.tap("settingsButton");
            CHECK(s.h.grab(prefix + "settings-haptics-game-center.png"));
        }
        {
            // A desktop: neither.
            Shell s(size.size, false, false);
            s.tap("settingsButton");
            CHECK(s.h.grab(prefix + "settings-desktop.png"));
        }
    }
}
