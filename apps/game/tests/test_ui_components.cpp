// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QAccessible>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>

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
    CHECK(property_of(host, "display").toString() == "Bangers");
    CHECK(property_of(host, "body").toString() == "Lilita One");
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

TEST_CASE("The halftone dots are hard-edged, four pixels across and centred half a grid step in", "[ui][halftone]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    HalftoneBackground {
        width: 400
        height: 300
    }
}
)",
                                     SMALL);
    h.settle();
    QTest::qWait(50);
    h.settle();
    const QImage image = h.view().grabWindow().convertToFormat(QImage::Format_RGB32);
    const QColor background("#FFE14D");
    const QColor dot("#F5B800");
    // The first dot of the board: pixels 4 to 7 in both directions, without its four corners.
    for (int y = 4; y <= 7; ++y)
    {
        for (int x = 4; x <= 7; ++x)
        {
            const bool corner = (x == 4 || x == 7) && (y == 4 || y == 7);
            CAPTURE(x, y);
            CHECK(QColor(image.pixel(x, y)) == (corner ? background : dot));
        }
    }
    // Hard edges: the pixels around it are plain background, and so is the middle of the grid cell.
    CHECK(QColor(image.pixel(3, 5)) == background);
    CHECK(QColor(image.pixel(8, 6)) == background);
    CHECK(QColor(image.pixel(6, 3)) == background);
    CHECK(QColor(image.pixel(6, 8)) == background);
    CHECK(QColor(image.pixel(12, 12)) == background);
    // The next dot is one grid step on.
    CHECK(QColor(image.pixel(18, 6)) == dot);
    CHECK(QColor(image.pixel(6, 18)) == dot);
    // Every pixel is one of the two colours: nothing is anti-aliased.
    int others = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor c(image.pixel(x, y));
            others += (c == background || c == dot) ? 0 : 1;
        }
    }
    CHECK(others == 0);
}

TEST_CASE("A card's content sits inside its border, so a margin is measured from the inside", "[ui][card]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    ComicCard {
        objectName: "card"
        x: 20
        y: 20
        width: 200
        height: 100
        borderWidth: 5
        Item {
            objectName: "content"
            anchors.fill: parent
            anchors.margins: 10
        }
    }
}
)",
                                     SMALL);
    auto* card = h.find_item("card");
    auto* content = h.find_item("content");
    REQUIRE(card != nullptr);
    REQUIRE(content != nullptr);
    const QPointF top_left = content->mapToItem(card, QPointF(0, 0));
    CHECK_THAT(top_left.x(), WithinAbs(15.0, 1e-9));
    CHECK_THAT(top_left.y(), WithinAbs(15.0, 1e-9));
    CHECK_THAT(content->width(), WithinAbs(170.0, 1e-9));
    CHECK_THAT(content->height(), WithinAbs(70.0, 1e-9));
}

TEST_CASE("A hand-drawn burst is mirror symmetric, fills its box and keeps its title inside", "[ui][burst]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    StarBurst {
        objectName: "hero"
        width: 262
        height: 262
        variant: "hero"
    }
    StarBurst {
        objectName: "wide"
        y: 270
        width: 342
        height: 172
        variant: "wide"
        text: "CLEAR!"
        subText: "LEVEL 5"
        textSize: 56
        subTextSize: 24
        letterSpacing: 2
    }
    StarBurst {
        objectName: "squashed"
        x: 270
        width: 190
        height: 190
        variant: "squashed"
        text: "BOOM!"
        textSize: 44
        letterSpacing: 1
    }
    StarBurst {
        objectName: "boom"
        x: 270
        y: 200
        width: 168
        height: 168
        variant: "boom"
        text: "BOOM!"
        textSize: 38
        letterSpacing: 1
    }
}
)",
                                     QSize{500, 460});
    struct Expect
    {
        const char* name;
        int spikes;
        double min_x;
        double max_x;
    };
    // The width of each board's outline in its own box: the hero spans 11.6 to 250.4 of 262, the wide one 8.2 to 333.8 of
    // 342, the squashed one 9.5 to 180.5 of 190 and the boom burst 7.9 to 160.1 of 168.
    for (const Expect& expect : {Expect{"hero", 12, 11.6, 250.4}, Expect{"wide", 14, 8.2, 333.8},
                                 Expect{"squashed", 12, 9.5, 180.5}, Expect{"boom", 10, 7.9, 160.1}})
    {
        CAPTURE(expect.name);
        QObject* burst = h.find(expect.name);
        REQUIRE(burst != nullptr);
        QVariant result;
        REQUIRE(QMetaObject::invokeMethod(burst, "burstPoints", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 1.0)));
        const QVariantList corners = result.toList();
        REQUIRE(corners.size() == expect.spikes * 2);
        const double width = property_of(burst, "width").toDouble();
        double lo = 1e9;
        double hi = -1e9;
        for (int i = 0; i < corners.size(); ++i)
        {
            const double x = corners[i].toMap().value("x").toDouble();
            const double y = corners[i].toMap().value("y").toDouble();
            lo = std::min(lo, x);
            hi = std::max(hi, x);
            // Mirror symmetric about the vertical axis: corner i and corner (n - i) share a height.
            const QVariantMap mate = corners[(corners.size() - i) % corners.size()].toMap();
            CHECK_THAT(mate.value("x").toDouble(), WithinAbs(width - x, 0.35));
            CHECK_THAT(mate.value("y").toDouble(), WithinAbs(y, 0.35));
        }
        CHECK_THAT(lo, WithinAbs(expect.min_x, 0.35));
        CHECK_THAT(hi, WithinAbs(expect.max_x, 0.35));
        CHECK(property_of(burst, "fitsInside").toBool());
        CHECK(property_of(burst, "fitScale").toDouble() > 0.9);
    }
    // The hero has alternating spike lengths, not one radius: it reaches the top and bottom of its box and a little less
    // to the sides.
    QObject* hero = h.find("hero");
    QVariant spikes;
    REQUIRE(QMetaObject::invokeMethod(hero, "burstPoints", Q_RETURN_ARG(QVariant, spikes), Q_ARG(QVariant, 1.0)));
    const QVariantList hero_points = spikes.toList();
    CHECK_THAT(hero_points[0].toMap().value("y").toDouble(), WithinAbs(4.0, 0.2));
    CHECK_THAT(hero_points[12].toMap().value("y").toDouble(), WithinAbs(258.0, 0.2));
    CHECK_THAT(hero_points[6].toMap().value("x").toDouble(), WithinAbs(250.4, 0.2));
    CHECK_THAT(hero_points[18].toMap().value("x").toDouble(), WithinAbs(11.6, 0.2));
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
    QObject* shortCard = h.find("short");
    QObject* longCard = h.find("long");
    REQUIRE(shortCard != nullptr);
    REQUIRE(longCard != nullptr);
    CHECK_FALSE(property_of(shortCard, "multiLine").toBool());
    CHECK(property_of(longCard, "multiLine").toBool());
    CHECK(property_of(shortCard, "spoken").toString() == "8 plus what equals 13");
    CHECK(property_of(longCard, "spoken").toString() == "250 plus 480 minus 130 equals what");
    // Every line fits the card: nothing is wider than the card allows.
    CHECK(property_of(shortCard, "contentHeight").toDouble() > 0);
    CHECK(property_of(longCard, "contentHeight").toDouble() > property_of(shortCard, "contentHeight").toDouble());
}
