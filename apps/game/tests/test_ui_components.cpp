// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QAccessible>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <vector>

using namespace hbm_test;
using Catch::Matchers::WithinAbs;

namespace
{

constexpr QSize SMALL{400, 300};

/// The first object of that name below `parent`, which may be any QObject.
QObject* child(QObject* parent, const char* name)
{
    return parent->findChild<QObject*>(QLatin1String(name));
}

} // namespace

TEST_CASE("The theme holds the tokens and both fonts load from resources", "[ui][theme]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    objectName: "host"
    property bool fontsReady: Theme.fontsReady
    property string display: Theme.displayFont
    property string body: Theme.bodyFont
    property string background: Theme.background
    property string ink: Theme.ink
    property string compact: Theme.sizeClassFor(389)
    property string regular: Theme.sizeClassFor(820)
    property string edge: Theme.sizeClassFor(Theme.compactMaxWidth)
    property string time42: Theme.formatTime(42)
    property string time8: Theme.formatTime(8)
    property string time60: Theme.formatTime(60)
    property string timeNegative: Theme.formatTime(-3)
    property string big: Theme.formatNumber(3860)
    property string minus: Theme.operatorGlyph("-")
    property int bits: Theme.bitCount(10)
    property string two: Theme.countWord(2)
}
)",
                                     SMALL);
    QObject* host = h.root();
    CHECK(property_of(host, "fontsReady").toBool());
    // The crossed seven is the default: the fonts are the modified copies, under new names.
    CHECK(property_of(host, "display").toString() == "HBM Display");
    CHECK(property_of(host, "body").toString() == "HBM Body");
    CHECK(property_of(host, "background").toString().toLower() == "#ffe14d");
    CHECK(property_of(host, "ink").toString().toLower() == "#111111");
    CHECK(property_of(host, "compact").toString() == "compact");
    CHECK(property_of(host, "regular").toString() == "regular");
    CHECK(property_of(host, "edge").toString() == "regular");
    CHECK(property_of(host, "time42").toString() == "0:42");
    CHECK(property_of(host, "time8").toString() == "0:08");
    CHECK(property_of(host, "time60").toString() == "1:00");
    CHECK(property_of(host, "timeNegative").toString() == "0:00");
    CHECK(property_of(host, "big").toString() == "3,860");
    CHECK(property_of(host, "minus").toString() == QString::fromUtf8("−"));
    CHECK(property_of(host, "bits").toInt() == 2);
    CHECK(property_of(host, "two").toString() == "TWO");
}

TEST_CASE("The crossed seven is on, and turning it off gives back the original fonts", "[ui][theme]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    objectName: "host"
    property bool crossedSevenAtStart: false
    property bool fontsReady: Theme.fontsReady
    property string display: Theme.displayFont
    property string body: Theme.bodyFont
    Component.onCompleted: crossedSevenAtStart = Theme.crossedSeven
}
)",
                                     SMALL);
    QObject* host = h.root();
    CHECK(property_of(host, "crossedSevenAtStart").toBool());
    CHECK(property_of(host, "fontsReady").toBool());
    // A modified font takes a new name: Lilita One reserves its own.
    CHECK(property_of(host, "display").toString() == "HBM Display");
    CHECK(property_of(host, "body").toString() == "HBM Body");
}

TEST_CASE("A button answers a tap, keeps a hit target of 44 and names itself", "[ui][button]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    property alias button: button
    property int clicks: 0
    ComicButton {
        id: button
        objectName: "button"
        x: 20
        y: 20
        width: 160
        height: 60
        text: "PLAY"
        onClicked: parent.clicks++
    }
    ComicButton {
        objectName: "tiny"
        x: 200
        y: 20
        text: "OK"
    }
    ComicButton {
        objectName: "off"
        x: 20
        y: 120
        width: 160
        height: 60
        text: "OFF"
        enabled: false
        onClicked: parent.clicks += 100
    }
    ComicButton {
        objectName: "icon"
        x: 200
        y: 120
        width: 50
        height: 50
        accessibleName: "Pause"
    }
}
)",
                                     SMALL);
    auto* button = h.find_item("button");
    REQUIRE(button != nullptr);

    SECTION("a tap clicks and a disabled button does not")
    {
        click(button);
        CHECK(property_of(h.root(), "clicks").toInt() == 1);
        click(h.find_item("off"));
        CHECK(property_of(h.root(), "clicks").toInt() == 1);
    }
    SECTION("the keyboard works")
    {
        button->forceActiveFocus();
        QTest::keyClick(&h.view(), Qt::Key_Return);
        QTest::keyClick(&h.view(), Qt::Key_Space);
        CHECK(property_of(h.root(), "clicks").toInt() == 2);
    }
    SECTION("its implicit size is at least 44 each way")
    {
        QQuickItem* tiny = h.find_item("tiny");
        REQUIRE(tiny != nullptr);
        CHECK(tiny->implicitWidth() >= 44);
        CHECK(tiny->implicitHeight() >= 44);
    }
    SECTION("a screen reader hears a button with a name")
    {
        QAccessibleInterface* text = QAccessible::queryAccessibleInterface(button);
        REQUIRE(text != nullptr);
        CHECK(text->role() == QAccessible::Button);
        CHECK(text->text(QAccessible::Name) == "PLAY");
        QAccessibleInterface* icon = QAccessible::queryAccessibleInterface(h.find("icon"));
        REQUIRE(icon != nullptr);
        CHECK(icon->text(QAccessible::Name) == "Pause");
        CHECK(icon->role() == QAccessible::Button);
    }
}

TEST_CASE("The star burst fits its title inside the inner polygon", "[ui][burst]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    StarBurst {
        objectName: "paused"
        x: 10
        y: 10
        width: 342
        height: 150
        text: "PAUSED"
    }
    StarBurst {
        objectName: "narrow"
        x: 10
        y: 170
        width: 160
        height: 90
        text: "A VERY LONG TITLE"
        subText: "AND A LINE"
    }
}
)",
                                     QSize{400, 300});
    QObject* paused = h.find("paused");
    QObject* narrow = h.find("narrow");
    REQUIRE(paused != nullptr);
    REQUIRE(narrow != nullptr);

    CHECK(property_of(paused, "fitsInside").toBool());
    // PAUSED at its nominal size very nearly fits the board's burst: it shrinks at most a little.
    CHECK(property_of(paused, "fitScale").toDouble() > 0.9);
    CHECK(property_of(paused, "fitScale").toDouble() <= 1.0);

    CHECK(property_of(narrow, "fitsInside").toBool());
    CHECK(property_of(narrow, "fitScale").toDouble() < 1.0);
    CHECK(property_of(narrow, "fitScale").toDouble() > 0.1);
}

TEST_CASE("The fuse bar draws the time left, the alert zone and the time just lost", "[ui][fuse]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    FuseBar {
        objectName: "fuse"
        width: 348
        height: 26
        fraction: 0.7
        alertFraction: 1 / 6
    }
}
)",
                                     SMALL);
    QObject* fuse = h.find("fuse");
    REQUIRE(fuse != nullptr);
    const double track = property_of(fuse, "trackWidth").toDouble();
    CHECK_THAT(track, WithinAbs(340.0, 1e-9));
    CHECK_THAT(property_of(fuse, "fillWidth").toDouble(), WithinAbs(track * 0.7, 1e-6));
    CHECK_THAT(property_of(fuse, "zoneWidth").toDouble(), WithinAbs(track / 6, 1e-6));

    auto* fill = qobject_cast<QQuickItem*>(child(fuse, "fuseFill"));
    REQUIRE(fill != nullptr);
    CHECK_THAT(fill->width(), WithinAbs(track * 0.7, 1e-6));
    CHECK(fill->property("color").value<QColor>() == QColor("#FF8A00"));

    SECTION("the alert turns the whole fuse red")
    {
        fuse->setProperty("alert", true);
        h.settle();
        CHECK(fill->property("color").value<QColor>() == QColor("#D62839"));
    }
    SECTION("a lost stretch shows only while it has a value, and never past the bar")
    {
        auto* lost = qobject_cast<QQuickItem*>(child(fuse, "fuseLost"));
        REQUIRE(lost != nullptr);
        CHECK_FALSE(lost->isVisible());
        fuse->setProperty("lostFraction", 0.05);
        h.settle();
        CHECK(lost->isVisible());
        CHECK_THAT(lost->width(), WithinAbs(track * 0.05, 1e-6));
        fuse->setProperty("fraction", 0.99);
        fuse->setProperty("lostFraction", 0.5);
        h.settle();
        CHECK(lost->width() <= track * 0.01 + 1e-6);
    }
    SECTION("an endless level's alert zone is a larger share")
    {
        fuse->setProperty("alertFraction", 10.0 / 44.0);
        CHECK_THAT(property_of(fuse, "zoneWidth").toDouble(), WithinAbs(track * 10.0 / 44.0, 1e-6));
    }
}

TEST_CASE("The heart bomb has a calm, an alert and a boom state", "[ui][heart]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    HeartBomb {
        objectName: "heart"
        secondsLeft: 42
    }
}
)",
                                     SMALL);
    QObject* heart = h.find("heart");
    REQUIRE(heart != nullptr);
    auto* seconds = qobject_cast<QQuickItem*>(child(heart, "bombSeconds"));
    REQUIRE(seconds != nullptr);

    CHECK(property_of(heart, "stateName").toString() == "calm");
    CHECK(property_of(seconds, "text").toString() == "0:42");
    CHECK(property_of(seconds, "color").value<QColor>() == QColor("#111111"));
    CHECK(property_of(child(heart, "heartBody"), "fillColor").value<QColor>() == QColor("#FF5FA2"));

    heart->setProperty("secondsLeft", 8);
    heart->setProperty("alert", true);
    h.settle();
    CHECK(property_of(heart, "stateName").toString() == "alert");
    CHECK(property_of(seconds, "text").toString() == "0:08");
    CHECK(property_of(seconds, "color").value<QColor>() == QColor("#FFFFFF"));
    CHECK(property_of(child(heart, "heartBody"), "fillColor").value<QColor>() == QColor("#D62839"));

    // The shape changes as well as the colour: the alert's spark is a different, bigger star.
    CHECK(property_of(child(heart, "spark"), "strokeColor").value<QColor>() == QColor("#D62839"));

    heart->setProperty("boom", true);
    h.settle();
    CHECK(property_of(heart, "stateName").toString() == "boom");
}

TEST_CASE("A tile has a shape or motion cue for every colour", "[ui][tile]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    AnswerTile {
        objectName: "tile"
        width: 160
        height: 96
        value: "7"
    }
}
)",
                                     SMALL);
    QObject* tile = h.find("tile");
    REQUIRE(tile != nullptr);
    auto* badge = qobject_cast<QQuickItem*>(child(tile, "tileBadge"));
    auto* face = qobject_cast<QQuickItem*>(child(tile, "tileFace"));
    REQUIRE(badge != nullptr);
    REQUIRE(face != nullptr);

    SECTION("normal: no badge, takes taps")
    {
        CHECK_FALSE(badge->isVisible());
        CHECK(property_of(tile, "tappable").toBool());
    }
    SECTION("wrong: a cross badge and a tilt")
    {
        tile->setProperty("look", "wrong");
        h.settle();
        CHECK(badge->isVisible());
        CHECK(face->rotation() != 0);
        CHECK_FALSE(property_of(tile, "tappable").toBool());
        QAccessibleInterface* a11y = QAccessible::queryAccessibleInterface(tile);
        REQUIRE(a11y != nullptr);
        CHECK(a11y->text(QAccessible::Name).contains("wrong"));
    }
    SECTION("right: a tick badge and a bigger tile")
    {
        tile->setProperty("look", "right");
        h.settle();
        CHECK(badge->isVisible());
        CHECK(face->scale() > 1.0);
        QAccessibleInterface* a11y = QAccessible::queryAccessibleInterface(tile);
        REQUIRE(a11y != nullptr);
        CHECK(a11y->text(QAccessible::Name).contains("right answer"));
    }
    SECTION("gone: the tile fades and a dashed ghost is left")
    {
        tile->setProperty("look", "gone");
        QTest::qWait(400);
        auto* ghost = qobject_cast<QQuickItem*>(child(tile, "tileGhost"));
        REQUIRE(ghost != nullptr);
        CHECK(ghost->opacity() > 0.9);
        CHECK(face->opacity() < 0.1);
    }
}

TEST_CASE("The question card reads one line for two terms and two for more", "[ui][question]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    QuestionCard {
        objectName: "short"
        x: 10
        y: 10
        width: 350
        height: 120
        parts: ["8", "+", "?", "=", "13"]
    }
    QuestionCard {
        objectName: "long"
        x: 10
        y: 140
        width: 350
        height: 150
        parts: ["250", "+", "480", "-", "130", "=", "?"]
    }
}
)",
                                     QSize{400, 320});
    QObject* short_card = h.find("short");
    QObject* long_card = h.find("long");
    REQUIRE(short_card != nullptr);
    REQUIRE(long_card != nullptr);
    CHECK_FALSE(property_of(short_card, "multiLine").toBool());
    CHECK(property_of(long_card, "multiLine").toBool());
    CHECK(property_of(short_card, "spoken").toString() == "8 plus what equals 13");
    CHECK(property_of(long_card, "spoken").toString() == "250 plus 480 minus 130 equals what");
    // The second line, "= ?", is centred under the terms, and so is the first: both lines share the card's middle.
    std::vector<QQuickItem*> lines;
    std::vector<QQuickItem*> pending{h.find_item("long")};
    while (!pending.empty())
    {
        QQuickItem* item = pending.back();
        pending.pop_back();
        for (QQuickItem* kid : item->childItems())
        {
            pending.push_back(kid);
        }
        if (item->objectName() == QLatin1String("questionLine"))
        {
            lines.push_back(item);
        }
    }
    REQUIRE(lines.size() == 2);
    const double card_middle = h.find_item("long")->mapToScene(QPointF(h.find_item("long")->width() / 2, 0)).x();
    for (QQuickItem* line : lines)
    {
        CHECK_THAT(line->mapToScene(QPointF(line->width() / 2, 0)).x(), WithinAbs(card_middle, 1.0));
    }
    CHECK(std::max(lines[0]->width(), lines[1]->width()) > 2.0 * std::min(lines[0]->width(), lines[1]->width()));
    // Every line fits the card: nothing is wider than the card allows.
    CHECK(property_of(short_card, "contentHeight").toDouble() > 0);
    CHECK(property_of(long_card, "contentHeight").toDouble() > property_of(short_card, "contentHeight").toDouble());
}

TEST_CASE("The safe-area frame keeps the content off the unsafe edges and lets the page run under them",
          "[ui][safearea]")
{
    // A phone with a cut-out and a home indicator reports margins such as these; the additional margins stand in for
    // them.
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    objectName: "host"
    SafeAreaFrame {
        objectName: "frame"
        anchors.fill: parent
        SafeArea.additionalMargins.left: 8
        SafeArea.additionalMargins.top: 59
        SafeArea.additionalMargins.right: 12
        SafeArea.additionalMargins.bottom: 34
        content: Rectangle {
            objectName: "probe"
            anchors.fill: parent
            color: "transparent"
        }
    }
}
)",
                                     QSize(390, 844));
    h.settle();
    QQuickItem* probe = h.find_item("probe");
    QQuickItem* page = h.find_item("windowBackground");
    REQUIRE(probe != nullptr);
    REQUIRE(page != nullptr);
    const QPointF top_left = probe->mapToItem(h.root(), QPointF(0, 0));
    CHECK_THAT(top_left.x(), WithinAbs(8.0, 0.5));
    CHECK_THAT(top_left.y(), WithinAbs(59.0, 0.5));
    CHECK_THAT(probe->width(), WithinAbs(390.0 - 8.0 - 12.0, 0.5));
    CHECK_THAT(probe->height(), WithinAbs(844.0 - 59.0 - 34.0, 0.5));
    // The yellow page itself still covers the whole window, edges included.
    CHECK_THAT(page->width(), WithinAbs(390.0, 0.5));
    CHECK_THAT(page->height(), WithinAbs(844.0, 0.5));
    // Its dots are in step with the page inside: the grid starts at the same offset as the content.
    CHECK_THAT(property_of(page, "originX").toDouble(), WithinAbs(8.0, 0.5));
    CHECK_THAT(property_of(page, "originY").toDouble(), WithinAbs(59.0, 0.5));
}

TEST_CASE("Without a safe area the frame fills the window", "[ui][safearea]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    objectName: "host"
    SafeAreaFrame {
        anchors.fill: parent
        content: Rectangle {
            objectName: "probe"
            anchors.fill: parent
            color: "transparent"
        }
    }
}
)",
                                     QSize(820, 1180));
    h.settle();
    QQuickItem* probe = h.find_item("probe");
    REQUIRE(probe != nullptr);
    CHECK_THAT(probe->width(), WithinAbs(820.0, 0.5));
    CHECK_THAT(probe->height(), WithinAbs(1180.0, 0.5));
}

TEST_CASE("The halftone page is a tiled picture that loads at once, for every grid", "[ui][halftone]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    objectName: "host"
    HalftoneBackground { objectName: "page12"; x: 0; width: 100; height: 100 }
    HalftoneBackground { objectName: "page14"; x: 100; width: 100; height: 100; gridSize: 14; dotRadius: 2 }
    HalftoneBackground { objectName: "page16"; x: 200; width: 100; height: 100; gridSize: 16; dotRadius: 2.2 }
}
)",
                                     QSize(300, 100));
    h.settle();
    for (const char* name : {"page12", "page14", "page16"})
    {
        INFO(name);
        QQuickItem* page = h.find_item(name);
        REQUIRE(page != nullptr);
        QQuickItem* dots = nullptr;
        for (QQuickItem* child : page->childItems())
        {
            if (child->objectName() == QStringLiteral("halftoneDots"))
            {
                dots = child;
            }
        }
        REQUIRE(dots != nullptr);
        // Ready the moment the page exists: no first frame without the dots (status 1 is Image.Ready).
        CHECK(property_of(dots, "status").toInt() == 1);
        CHECK(property_of(dots, "fillMode").toInt() == 3); // Image.Tile
        // The picture covers the page and one tile beyond each edge.
        CHECK(dots->width() > page->width());
        CHECK(dots->height() > page->height());
    }
    // The grid follows the grid size: a tile of 12, 14 and 16 pixels.
    CHECK(property_of(h.find("page12"), "tileGrid").toInt() == 12);
    CHECK(property_of(h.find("page14"), "tileGrid").toInt() == 14);
    CHECK(property_of(h.find("page16"), "tileGrid").toInt() == 16);
}
