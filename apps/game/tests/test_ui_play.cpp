// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QQuickItem>
#include <QTest>
#include <algorithm>
#include <memory>
#include <vector>

using namespace hbm_test;
using namespace heart_boom_maths::game;
using State = GameController::State;

namespace
{

constexpr QSize PHONE{390, 844};
constexpr QSize TABLET{820, 1180};
constexpr QSize DESKTOP{1280, 800};

constexpr int SHORT_HOLD_MS = 40;
constexpr int PAST_HOLD_MS = 160;

/// An iPhone's margins in portrait: the cut-out at the top, the home indicator at the bottom.
constexpr Margins PHONE_MARGINS{0, 59, 0, 34};

/// What the theme gives the alert frame, read from QML so the tests follow the theme and do not copy its numbers.
struct AlertTheme
{
    int line = 0;
    int line_rounded = 0;
    int corner_compact = 0;
    int corner_regular = 0;
};

AlertTheme alert_theme()
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    objectName: "host"
    property int line: Theme.alertLine
    property int lineRounded: Theme.alertLineRounded
    property int cornerCompact: Theme.alertCornerCompact
    property int cornerRegular: Theme.alertCornerRegular
}
)",
                                     QSize{10, 10});
    QObject* host = h.root();
    return {property_of(host, "line").toInt(), property_of(host, "lineRounded").toInt(),
            property_of(host, "cornerCompact").toInt(), property_of(host, "cornerRegular").toInt()};
}

/// The width of a Rectangle's border.
int border_width(const QObject* rectangle)
{
    return property_of(qvariant_cast<QObject*>(property_of(rectangle, "border")), "width").toInt();
}

/// The whole application on a controller, with the holds shortened so a test can wait them out.
struct Shell
{
    explicit Shell(QSize size, bool start = true, Margins margins = {}) : h(app_root(&rig.controller, size, margins))
    {
        feedback = h.find("answerFeedback");
        REQUIRE(feedback != nullptr);
        feedback->setProperty("rightHoldMs", SHORT_HOLD_MS);
        feedback->setProperty("wrongHoldMs", SHORT_HOLD_MS * 2);
        feedback->setProperty("boomHoldMs", SHORT_HOLD_MS * 2);
        if (start)
        {
            rig.controller.startGame();
            h.settle();
        }
    }

    [[nodiscard]] QQuickItem* tile(int position) const
    {
        return h.find_item(QStringLiteral("tile%1").arg(position));
    }

    [[nodiscard]] QString look(int position) const
    {
        return property_of(tile(position), "look").toString();
    }

    [[nodiscard]] QString value(int position) const
    {
        return property_of(tile(position), "value").toString();
    }

    [[nodiscard]] bool visible(const char* name) const
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        return item != nullptr && item->isVisible();
    }

    [[nodiscard]] QString text_of(const char* name) const
    {
        return property_of(h.find(QLatin1String(name)), "text").toString();
    }

    /// Waits for the holds to end and the screen to follow.
    void wait_out_holds()
    {
        QTest::qWait(PAST_HOLD_MS);
        h.settle();
    }

    /// Taps the right tile, as a child would, and lets the hold run out.
    void tap_right_and_wait()
    {
        click(tile(rig.controller.session_view().question.correct_tile));
        wait_out_holds();
    }

    UiRig rig;
    QmlHarness h;
    QObject* feedback = nullptr;
};

} // namespace

TEST_CASE("The title placeholder starts a game and the play screen appears", "[ui][shell]")
{
    Shell s(PHONE, false);
    REQUIRE(s.rig.controller.state() == State::Idle);
    CHECK(s.visible("playButton"));
    CHECK(s.h.find("playScreen") == nullptr);

    click(s.h.find_item("playButton"));
    s.h.settle();

    CHECK(s.rig.controller.state() == State::Playing);
    CHECK(s.h.find("playScreen") != nullptr);
    CHECK(s.h.find("playButton") == nullptr);
}

TEST_CASE("The play screen shows the question, four tiles and the stats in every arrangement", "[ui][play]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
        const char* mode;
        bool one_row;
        double min_tile_height;
    };
    const Arrangement arrangement = GENERATE(Arrangement{"phone", PHONE, "compact", false, 96},
                                             Arrangement{"tablet", TABLET, "portrait", false, 96},
                                             Arrangement{"desktop", DESKTOP, "landscape", true, 96});
    CAPTURE(arrangement.name);
    Shell s(arrangement.size);
    QObject* play = s.h.find("playScreen");
    REQUIRE(play != nullptr);
    CHECK(property_of(play, "mode").toString() == arrangement.mode);

    const GameController& c = s.rig.controller;
    REQUIRE(c.tiles().size() == 4);
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE(s.tile(i) != nullptr);
        CHECK(s.value(i) == QString::number(c.tiles().at(i)));
        CHECK(s.look(i) == "normal");
        CHECK(s.tile(i)->height() >= arrangement.min_tile_height);
        CHECK(s.tile(i)->width() > 0);
    }
    // One row of four on a desktop window, a grid of two by two elsewhere.
    CHECK((s.tile(0)->y() == s.tile(3)->y()) == arrangement.one_row);
    CHECK((s.tile(2)->y() > s.tile(0)->y()) == !arrangement.one_row);

    CHECK(s.text_of("levelLabel") == "LEVEL 1");
    CHECK(s.text_of("questionCount") == "1 / 10");
    CHECK_FALSE(s.visible("endlessBadge"));
    CHECK(s.text_of("pointsValue") == "0");
    CHECK(string_list(property_of(s.h.find("questionCard"), "parts")) == c.question_parts());
    CHECK(s.visible("heartBomb"));
    CHECK(property_of(s.h.find("heartBomb"), "stateName").toString() == "calm");
    CHECK_FALSE(s.visible("alertBanner"));
    CHECK_FALSE(s.visible("alertEdge"));

    // The heart bomb has its own room: it overlaps neither the stats beside it nor the fuse bar under it.
    {
        QQuickItem* heart = s.h.find_item("heartBomb");
        QQuickItem* fuse = s.h.find_item("fuseBar");
        QQuickItem* points = s.h.find_item("pointsValue");
        REQUIRE(heart != nullptr);
        REQUIRE(fuse != nullptr);
        REQUIRE(points != nullptr);
        const QPointF heart_end = heart->mapToScene(QPointF(heart->width(), heart->height()));
        CHECK(heart->width() > 100);
        CHECK(heart_end.y() <= fuse->mapToScene(QPointF(0, 0)).y() + 1);
        if (!arrangement.one_row)
        {
            CHECK(heart_end.x() <= points->mapToScene(QPointF(0, 0)).x());
        }
    }

    // The pause button and the hint button are real hit targets.
    CHECK(s.h.find_item("pauseButton")->width() >= 44);
    CHECK(s.h.find_item("pauseButton")->height() >= 44);
    CHECK(s.h.find_item("hintButton")->height() >= 44);

    // Nothing sticks out of the window.
    for (int i = 0; i < 4; ++i)
    {
        const QPointF bottom_right = s.tile(i)->mapToScene(QPointF(s.tile(i)->width(), s.tile(i)->height()));
        CHECK(bottom_right.x() <= arrangement.size.width());
        CHECK(bottom_right.y() <= arrangement.size.height());
    }
}

TEST_CASE("The digits of the question and its slot share one centre line at every size", "[ui][play][question]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
    };
    const Arrangement arrangement =
        GENERATE(Arrangement{"phone", PHONE}, Arrangement{"tablet", TABLET}, Arrangement{"desktop", DESKTOP});
    CAPTURE(arrangement.name);
    Shell s(arrangement.size);
    QQuickItem* card = s.h.find_item("questionCard");
    QQuickItem* slot = s.h.find_item("questionSlot");
    REQUIRE(card != nullptr);
    REQUIRE(slot != nullptr);
    const double slot_centre = slot->mapToScene(QPointF(0, slot->height() / 2)).y();
    // The terms are delegates of a Repeater, so they hang from the card's visual tree.
    std::vector<QQuickItem*> pending{card};
    int terms = 0;
    while (!pending.empty())
    {
        QQuickItem* item = pending.back();
        pending.pop_back();
        for (QQuickItem* child : item->childItems())
        {
            pending.push_back(child);
        }
        if (item->objectName() != QLatin1String("questionTerm") || !item->isVisible())
        {
            continue;
        }
        ++terms;
        const double centre = item->mapToScene(QPointF(0, item->height() / 2)).y();
        CHECK_THAT(centre, Catch::Matchers::WithinAbs(slot_centre, 1.0));
    }
    CHECK(terms >= 3);
    // The card itself is centred on that line too.
    const double card_centre = card->mapToScene(QPointF(0, card->height() / 2)).y();
    CHECK_THAT(slot_centre, Catch::Matchers::WithinAbs(card_centre, 1.5));
}

TEST_CASE("The hint button is disabled when the wallet cannot pay and enabled when it can", "[ui][play][hint]")
{
    Shell s(PHONE);
    QQuickItem* hint = s.h.find_item("hintButton");
    REQUIRE(hint != nullptr);
    CHECK(s.rig.controller.wallet() == 0);
    CHECK_FALSE(hint->isEnabled());
    CHECK(property_of(hint, "text").toString() == QString::fromUtf8("HINT · 2 COINS"));

    s.tap_right_and_wait();
    s.tap_right_and_wait(); // two coins
    REQUIRE(s.rig.controller.wallet() == 2);
    CHECK(hint->isEnabled());
}

TEST_CASE("A hint removes two tiles, says so and costs two coins", "[ui][play][hint]")
{
    Shell s(PHONE);
    s.tap_right_and_wait();
    s.tap_right_and_wait();
    QQuickItem* hint = s.h.find_item("hintButton");
    REQUIRE(hint->isEnabled());

    click(hint);
    s.h.settle();

    CHECK(s.rig.controller.wallet() == 0);
    int gone = 0;
    for (int i = 0; i < 4; ++i)
    {
        gone += s.look(i) == "gone" ? 1 : 0;
    }
    CHECK(gone == 2);
    CHECK(property_of(hint, "text").toString() == QString::fromUtf8("HINT USED · TWO TILES GONE"));
    CHECK_FALSE(hint->isEnabled());
    CHECK(property_of(s.h.find("coinWallet"), "delta").toInt() == -2);
    // A removed tile takes no taps.
    for (int i = 0; i < 4; ++i)
    {
        CHECK(property_of(s.tile(i), "tappable").toBool() == (s.look(i) != "gone"));
    }
    // The delta goes away with the next question.
    s.tap_right_and_wait();
    CHECK(property_of(s.h.find("coinWallet"), "delta").toInt() == 0);
}

TEST_CASE("A right answer turns the tile green with a tick and shows the points and the time gained",
          "[ui][play][answer]")
{
    Shell s(PHONE);
    s.rig.play_for_ms(10000);
    s.h.settle();
    const int correct = s.rig.controller.session_view().question.correct_tile;
    const QString shown = s.value(correct);

    // A long hold, so the test looks at the held screen before the hold ends: a frame under a sanitizer can take longer
    // than the short holds the other tests use (the nightly's MSVC AddressSanitizer leg saw it end first).
    s.feedback->setProperty("rightHoldMs", 60000);
    click(s.tile(correct));
    s.h.settle();

    CHECK(s.rig.controller.points() == 10);
    CHECK(s.look(correct) == "right");
    for (int i = 0; i < 4; ++i)
    {
        if (i != correct)
        {
            CHECK(s.look(i) == "dimmed");
        }
    }
    // The held tiles are the ones that were tapped, not the next question's.
    CHECK(s.value(correct) == shown);
    CHECK(s.visible("rightTag"));
    CHECK(s.text_of("rightTag") == "+10");
    CHECK(s.visible("timeGain"));
    CHECK(s.text_of("timeGain") == "+7 s");
    // Taps are ignored while the answer is held.
    const qint64 points = s.rig.controller.points();
    click(s.tile(correct));
    CHECK(s.rig.controller.points() == points);

    QMetaObject::invokeMethod(s.feedback, "clear");
    s.h.settle();
    s.feedback->setProperty("rightHoldMs", SHORT_HOLD_MS);
    s.wait_out_holds();
    CHECK(s.look(correct) == "normal");
    CHECK_FALSE(s.visible("rightTag"));
    CHECK_FALSE(s.visible("timeGain"));
    CHECK(s.text_of("questionCount") == "2 / 10");
}

TEST_CASE("A wrong answer turns the tile red with a cross, shows the right one and the lost time", "[ui][play][answer]")
{
    Shell s(PHONE);
    // A run of three lifts the step to 2, so a wrong answer has a fall to show.
    s.tap_right_and_wait();
    s.tap_right_and_wait();
    s.tap_right_and_wait();
    REQUIRE(s.rig.controller.multiplier() == 2);
    const int correct = s.rig.controller.session_view().question.correct_tile;
    const int wrong = correct == 0 ? 1 : 0;
    const QString right_text = s.value(correct);

    // A long hold, for the reason given in the test of a right answer.
    s.feedback->setProperty("wrongHoldMs", 60000);
    click(s.tile(wrong));
    s.h.settle();

    CHECK(s.look(wrong) == "wrong");
    CHECK(s.look(correct) == "right");
    for (int i = 0; i < 4; ++i)
    {
        if (i != wrong && i != correct)
        {
            CHECK(s.look(i) == "dimmed");
        }
    }
    CHECK(s.visible("wrongTag"));
    CHECK(s.text_of("wrongTag") == QStringLiteral("NOT QUITE! IT'S %1").arg(right_text));
    CHECK(s.visible("timeFlash"));
    CHECK(s.text_of("timeFlash") == QString::fromUtf8("−3 s"));
    CHECK(s.visible("fuseLost"));
    CHECK(property_of(s.h.find("multiplierBadge"), "dashed").toBool());
    CHECK(s.rig.controller.multiplier() == 1);
    // The question card keeps the question that was answered, with its slot empty.
    CHECK(string_list(property_of(s.h.find("questionCard"), "parts")) != s.rig.controller.question_parts());
    CHECK(property_of(s.h.find("questionCard"), "slotValue").toString().isEmpty());

    QMetaObject::invokeMethod(s.feedback, "clear");
    s.h.settle();
    s.feedback->setProperty("wrongHoldMs", SHORT_HOLD_MS * 2);
    s.wait_out_holds();
    CHECK(s.look(wrong) == "normal");
    CHECK_FALSE(s.visible("wrongTag"));
    CHECK_FALSE(s.visible("timeFlash"));
    CHECK_FALSE(property_of(s.h.find("multiplierBadge"), "dashed").toBool());
}

TEST_CASE("The last ten seconds turn the play screen into the alert", "[ui][play][alert]")
{
    Shell s(PHONE);
    s.rig.play_for_ms(49000);
    s.h.settle();
    CHECK_FALSE(s.visible("alertBanner"));

    s.rig.play_for_ms(1000);
    s.h.settle();
    REQUIRE(s.rig.controller.alert());

    CHECK(s.visible("alertBanner"));
    CHECK(s.text_of("alertBanner") == "HURRY! 10 SECONDS LEFT!");
    CHECK(s.visible("alertEdge"));
    CHECK(property_of(s.h.find("heartBomb"), "stateName").toString() == "alert");

    s.rig.play_for_ms(2000);
    s.h.settle();
    CHECK(s.text_of("alertBanner") == "HURRY! 8 SECONDS LEFT!");
    CHECK(property_of(s.h.find("bombSeconds"), "text").toString() == "0:08");
}

namespace
{

/// The window-sized rectangle an item covers, in scene coordinates.
QRectF scene_rect(const QQuickItem* item)
{
    return QRectF(item->mapToScene(QPointF(0, 0)), QSizeF(item->width(), item->height()));
}

} // namespace

TEST_CASE("The alert frame and the streak flash cover the whole window while the play screen stays in the safe area",
          "[ui][play][alert][safearea]")
{
    struct Case
    {
        const char* name;
        QSize size;
        Margins margins;
    };
    const Case c = GENERATE(Case{"phone, cut-out and home indicator", PHONE, PHONE_MARGINS},
                            Case{"tablet on its side", QSize{1180, 820}, Margins{0, 24, 0, 20}},
                            Case{"phone, all four sides", PHONE, Margins{8, 59, 12, 34}},
                            Case{"no margins, phone size", PHONE, Margins{}},
                            Case{"no margins, desktop window", DESKTOP, Margins{}});
    CAPTURE(c.name);
    Shell s(c.size, true, c.margins);
    s.rig.play_for_ms(50000);
    s.h.settle();
    REQUIRE(s.rig.controller.alert());

    const QRectF window(QPointF(0, 0), QSizeF(c.size));
    const QRectF safe(
        QPointF(c.margins.left, c.margins.top),
        QSizeF(c.size.width() - c.margins.left - c.margins.right, c.size.height() - c.margins.top - c.margins.bottom));
    QQuickItem* edge = s.h.find_item("alertEdge");
    QQuickItem* flash = s.h.find_item("streakFlash");
    QQuickItem* play = s.h.find_item("playScreen");
    REQUIRE(edge != nullptr);
    REQUIRE(flash != nullptr);
    REQUIRE(play != nullptr);
    CHECK(edge->isVisible());

    // The frame and the flash are the window, edge to edge; the play screen is the safe area, as it always was.
    CHECK(scene_rect(edge) == window);
    CHECK(scene_rect(flash) == window);
    CHECK(scene_rect(play) == safe);

    // The frame is one thin line whose outer edge is the window's, the same width on every side: the margins do not
    // thicken it. With no bottom margin its corners are square and the line is the 8 pixel one of board CP3; with a
    // bottom margin (the home indicator of a screen with rounded corners) the corners are rounded as the theme says.
    const AlertTheme theme = alert_theme();
    const bool rounded = c.margins.bottom > 0;
    const int line = rounded ? theme.line_rounded : theme.line;
    const bool compact = std::min(c.size.width(), c.size.height()) < 600;
    const qreal radius = rounded ? (compact ? theme.corner_compact : theme.corner_regular) : 0;
    CHECK(property_of(edge, "radius").toReal() == radius);
    CHECK(border_width(edge) == line);
    CHECK(property_of(edge, "color").value<QColor>().alpha() == 0);
    if (!rounded)
    {
        CHECK(line == 8);
    }

    // What is drawn: red the line's width in from the window edge at the middle of every side, and not beyond it. At
    // the corner the line follows the radius: red on a square one, the page showing outside the arc on a rounded one.
    const QImage image = s.h.grab_image();
    REQUIRE_FALSE(image.isNull());
    const auto reddish = [&](int x, int y)
    {
        const QColor pixel(image.pixel(x, y));
        return pixel.red() > 180 && pixel.green() < 180;
    };
    const int mid_x = c.size.width() / 2;
    const int mid_y = c.size.height() / 2;
    const int last_x = c.size.width() - 1;
    const int last_y = c.size.height() - 1;
    CHECK(reddish(mid_x, 0));
    CHECK(reddish(mid_x, line - 1));
    CHECK_FALSE(reddish(mid_x, line + 3));
    CHECK(reddish(mid_x, last_y));
    CHECK(reddish(mid_x, last_y - line + 1));
    CHECK_FALSE(reddish(mid_x, last_y - line - 3));
    CHECK(reddish(0, mid_y));
    CHECK(reddish(line - 1, mid_y));
    CHECK_FALSE(reddish(line + 3, mid_y));
    CHECK(reddish(last_x, mid_y));
    CHECK(reddish(last_x - line + 1, mid_y));
    CHECK_FALSE(reddish(last_x - line - 3, mid_y));
    CHECK(reddish(0, 0) == !rounded);
    CHECK(reddish(last_x, last_y) == !rounded);

    // Neither takes a tap, a key or a screen reader's attention, and a tap through them reaches the tile below.
    for (QQuickItem* item : {edge, flash})
    {
        CHECK_FALSE(item->isEnabled());
        CHECK(item->acceptedMouseButtons() == Qt::NoButton);
        CHECK_FALSE(item->acceptTouchEvents());
        CHECK_FALSE(item->activeFocusOnTab());
    }
    const int before = s.rig.controller.question_number();
    click(s.tile(s.rig.controller.session_view().question.correct_tile));
    CHECK(s.rig.controller.question_number() == before + 1);
}

TEST_CASE("The alert frame shows with the alert and goes with it", "[ui][play][alert]")
{
    Shell s(PHONE, true, PHONE_MARGINS);
    QQuickItem* edge = s.h.find_item("alertEdge");
    REQUIRE(edge != nullptr);
    CHECK_FALSE(edge->isVisible());

    s.rig.play_for_ms(50000);
    s.h.settle();
    REQUIRE(s.rig.controller.alert());
    CHECK(edge->isVisible());

    // The frame pulses: its opacity moves while it shows.
    const qreal first = edge->opacity();
    QTest::qWait(200);
    CHECK(edge->opacity() != first);

    // Pausing ends it, as it ended the alert of the play screen; resuming brings it back.
    s.rig.controller.pause();
    s.h.settle();
    CHECK_FALSE(edge->isVisible());
    s.rig.controller.resume();
    s.h.settle();
    CHECK(edge->isVisible());

    // The end of the game takes it away.
    s.rig.play_for_ms(11000);
    s.wait_out_holds();
    CHECK_FALSE(s.visible("alertEdge"));
}

TEST_CASE("A multiplier step flashes the whole window white", "[ui][play][streak]")
{
    Shell s(PHONE, true, PHONE_MARGINS);
    QQuickItem* flash = s.h.find_item("streakFlash");
    REQUIRE(flash != nullptr);
    CHECK(flash->opacity() == 0);
    CHECK(scene_rect(flash) == QRectF(0, 0, PHONE.width(), PHONE.height()));

    // Three right answers in a row lift the multiplier to 2: the third one flashes.
    s.tap_right_and_wait();
    s.tap_right_and_wait();
    CHECK(flash->opacity() == 0);
    click(s.tile(s.rig.controller.session_view().question.correct_tile));
    QTest::qWait(100);
    REQUIRE(s.rig.controller.multiplier() == 2);
    CHECK(flash->opacity() > 0);
    QTest::qWait(500);
    CHECK(flash->opacity() == 0);
}

TEST_CASE("In the alert nothing of the play column overlaps another part, at every size", "[ui][play][alert]")
{
    // From a 667 high phone (the shortest the column's minimum heights allow) to the tablets in portrait and the
    // desktop.
    const QSize size =
        GENERATE(PHONE, QSize{375, 667}, TABLET, QSize{768, 1024}, QSize{744, 1133}, DESKTOP, QSize{1024, 600});
    CAPTURE(size.width(), size.height());
    Shell s(size);
    s.rig.play_for_ms(50000);
    s.h.settle();
    REQUIRE(s.rig.controller.alert());
    REQUIRE(s.visible("alertBanner"));

    const auto top_of = [&](const char* name)
    { return s.h.find_item(QLatin1String(name))->mapToScene(QPointF(0, 0)).y(); };
    const auto bottom_of = [&](const char* name)
    {
        QQuickItem* item = s.h.find_item(QLatin1String(name));
        return item->mapToScene(QPointF(0, item->height())).y();
    };
    const bool wide = size.width() >= 600 && size.width() >= size.height();
    CAPTURE(top_of("fuseBar"), bottom_of("fuseBar"), top_of("alertBanner"), bottom_of("alertBanner"),
            top_of("questionCard"), bottom_of("questionCard"), top_of("tile0"), bottom_of("tile0"), top_of("tile2"),
            bottom_of("tile2"));
    // From the top down, each part ends where it or the next begins: the banner and the question card, the hint
    // button and the tiles.
    if (!wide)
    {
        CHECK(bottom_of("fuseBar") <= top_of("alertBanner") + 0.5);
        CHECK(bottom_of("heartBomb") <= top_of("fuseBar") + 0.5);
    }
    // The banner is tilted a little, so its box is measured by its own height, the card by its box.
    CHECK(bottom_of("alertBanner") <= top_of("questionCard") + 0.5);
    if (!wide)
    {
        CHECK(bottom_of("questionCard") <= top_of("hintButton") + 0.5);
        CHECK(bottom_of("hintButton") <= top_of("tile0") + 0.5);
        CHECK(bottom_of("hintButton") <= top_of("tile1") + 0.5);
    }
    else
    {
        // The hint button is in the left panel; the card ends above the tiles.
        CHECK(bottom_of("questionCard") <= top_of("tile0") + 0.5);
    }
    // The card keeps room for its question, and the tiles are inside the window.
    QQuickItem* card = s.h.find_item("questionCard");
    CHECK(card->height() > property_of(card, "contentHeight").toDouble());
    CHECK(bottom_of("tile2") <= size.height() + 0.5);
    CHECK(bottom_of("tile3") <= size.height() + 0.5);

    // A held answer in the alert: the caption that replaces the hint button lies between the card and the tiles.
    s.feedback->setProperty("wrongHoldMs", 60000);
    s.rig.answer_wrong();
    s.h.settle();
    QTest::qWait(300);
    if (s.visible("wrongTag") && !wide)
    {
        CHECK(top_of("wrongTag") >= bottom_of("questionCard") - 0.5);
        CHECK(bottom_of("wrongTag") <= top_of("tile0") + 0.5);
        CHECK(bottom_of("alertBanner") <= top_of("questionCard") + 0.5);
    }
    CHECK(s.visible("wrongTag"));
    CHECK(bottom_of("tile3") <= size.height() + 0.5);
}

TEST_CASE("The pause button opens the pause overlay and RESUME and QUIT leave it", "[ui][play][pause]")
{
    Shell s(PHONE);
    s.tap_right_and_wait();
    CHECK_FALSE(s.visible("pauseOverlay"));

    click(s.h.find_item("pauseButton"));
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::Paused);

    CHECK(s.visible("pauseOverlay"));
    CHECK(s.text_of("pauseLevel") == "1");
    CHECK(s.text_of("pauseQuestion") == "2 / 10");
    CHECK(s.text_of("pausePoints") == "10");
    CHECK(property_of(s.h.find("pauseHeart"), "secondsLeft").toInt() == s.rig.controller.seconds_left());

    SECTION("RESUME goes back to the game")
    {
        click(s.h.find_item("resumeButton"));
        s.h.settle();
        CHECK(s.rig.controller.state() == State::Playing);
        CHECK_FALSE(s.visible("pauseOverlay"));
    }
    SECTION("QUIT ends the game")
    {
        click(s.h.find_item("quitButton"));
        s.h.settle();
        CHECK(s.rig.controller.state() == State::GameOver);
        // Ten points earn a place in the top 10, so the name screen comes before the game-over card.
        REQUIRE(s.visible("skipButton"));
        click(s.h.find_item("skipButton"));
        s.h.settle();
        CHECK(s.visible("playAgainButton"));
    }
}

TEST_CASE("A run from level 1 to the bomb and back to level 1, through the screens", "[ui][shell][run]")
{
    Shell s(PHONE, false);
    click(s.h.find_item("playButton"));
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::Playing);

    // Ten answers clear the level; the play screen stays for the last answer's hold, then the level clear card follows.
    for (int i = 0; i < 9; ++i)
    {
        s.tap_right_and_wait();
    }
    // A long hold for this one, so the test looks at the held screen before the hold ends (a frame can take longer than
    // the short holds the other tests use).
    s.feedback->setProperty("rightHoldMs", 60000);
    click(s.tile(s.rig.controller.session_view().question.correct_tile));
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::LevelClear);
    CHECK(s.h.find("playScreen") != nullptr);
    CHECK(s.look(s.rig.controller.session_view().question.correct_tile) == "right");
    QMetaObject::invokeMethod(s.feedback, "clear");
    s.h.settle();
    CHECK(s.h.find("playScreen") == nullptr);
    s.feedback->setProperty("rightHoldMs", SHORT_HOLD_MS);
    REQUIRE(s.visible("nextLevelButton"));

    click(s.h.find_item("nextLevelButton"));
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::Playing);
    CHECK(s.text_of("levelLabel") == "LEVEL 2");
    CHECK(s.text_of("questionCount") == "1 / 10");

    // The timer runs out: the heart pops while the play screen is held, then the game-over card follows.
    s.feedback->setProperty("boomHoldMs", 60000);
    s.rig.play_for_ms(61000);
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::GameOver);
    CHECK(s.h.find("playScreen") != nullptr);
    CHECK(property_of(s.h.find("heartBomb"), "stateName").toString() == "boom");
    QMetaObject::invokeMethod(s.feedback, "clear");
    s.h.settle();
    CHECK(s.h.find("playScreen") == nullptr);
    // The score earned a place in the top 10: the name screen first, and SKIP leads to the game-over card.
    REQUIRE(s.visible("skipButton"));
    click(s.h.find_item("skipButton"));
    s.h.settle();
    REQUIRE(s.visible("playAgainButton"));

    click(s.h.find_item("playAgainButton"));
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::Playing);
    CHECK(s.text_of("levelLabel") == "LEVEL 1");
    CHECK(s.text_of("pointsValue") == "0");
}

TEST_CASE("Winning level 12 offers KEEP GOING, and endless levels say so and show the time gained",
          "[ui][shell][endless]")
{
    Shell s(DESKTOP);
    for (int level = 1; level <= 12; ++level)
    {
        s.rig.play_level_right();
        REQUIRE(s.rig.controller.state() == (level == 12 ? State::Won : State::LevelClear));
        if (level < 12)
        {
            s.rig.controller.nextLevel();
        }
    }
    s.wait_out_holds();
    REQUIRE(s.visible("keepGoingButton"));
    CHECK(s.visible("finishButton"));

    click(s.h.find_item("keepGoingButton"));
    s.h.settle();
    REQUIRE(s.rig.controller.state() == State::Playing);
    REQUIRE(s.rig.controller.endless());
    CHECK(s.text_of("levelLabel") == "LEVEL 13");
    CHECK(s.visible("endlessBadge"));
    CHECK(property_of(s.h.find("fuseBar"), "alertFraction").toDouble() ==
          static_cast<double>(s.rig.controller.alert_ticks()) / s.rig.controller.ticks_total());

    s.rig.play_for_ms(9000);
    s.h.settle();
    // A long hold, so a slow frame cannot clear the look before the test reads it.
    s.feedback->setProperty("rightHoldMs", 60000);
    click(s.tile(s.rig.controller.session_view().question.correct_tile));
    s.h.settle();
    CHECK(s.visible("timeGain"));
    CHECK(s.text_of("timeGain") == QStringLiteral("+%1 s").arg(s.rig.controller.seconds_per_right_answer()));
}

TEST_CASE("In an endless level the time gained hangs clear of the question card", "[ui][shell][endless]")
{
    Shell s(PHONE);
    for (int level = 1; level <= 12; ++level)
    {
        s.rig.play_level_right();
        if (level < 12)
        {
            s.rig.controller.nextLevel();
        }
    }
    s.wait_out_holds();
    click(s.h.find_item("keepGoingButton"));
    s.h.settle();
    s.rig.play_for_ms(9000);
    s.h.settle();
    s.feedback->setProperty("rightHoldMs", 60000);
    click(s.tile(s.rig.controller.session_view().question.correct_tile));
    s.h.settle();
    QTest::qWait(400); // the tag pops in

    QQuickItem* gain = s.h.find_item("timeGain");
    QQuickItem* fuse = s.h.find_item("fuseBar");
    QQuickItem* card = s.h.find_item("questionCard");
    REQUIRE(gain != nullptr);
    REQUIRE(gain->isVisible());
    // The tag sits 5 under the bar, and the card is at least 30 under the bar: the tag is whole above it.
    const double gain_bottom = gain->mapToScene(QPointF(0, gain->height())).y();
    const double card_top = card->mapToScene(QPointF(0, 0)).y();
    const double fuse_bottom = fuse->mapToScene(QPointF(0, fuse->height())).y();
    CHECK_THAT(gain->y() - fuse->height(), Catch::Matchers::WithinAbs(5.0, 0.1));
    CHECK(gain_bottom <= card_top - 1.0);
    CHECK(card_top - fuse_bottom >= 30.0);
}

TEST_CASE("The window can change from a phone to a desktop shape while playing", "[ui][play][resize]")
{
    Shell s(PHONE);
    QObject* play = s.h.find("playScreen");
    REQUIRE(property_of(play, "mode").toString() == "compact");

    s.h.resize(DESKTOP);
    play = s.h.find("playScreen");
    CHECK(property_of(play, "mode").toString() == "landscape");
    CHECK(s.tile(0)->y() == s.tile(3)->y());
    CHECK(s.value(0) == QString::number(s.rig.controller.tiles().at(0)));

    s.h.resize(TABLET);
    CHECK(property_of(s.h.find("playScreen"), "mode").toString() == "portrait");
    s.h.resize(PHONE);
    CHECK(property_of(s.h.find("playScreen"), "mode").toString() == "compact");
}

TEST_CASE("Screenshots of the play screen for the hand check against the boards", "[.][shots]")
{
    const QByteArray dir = qgetenv("HBM_SHOT_DIR");
    REQUIRE_FALSE(dir.isEmpty());
    const QString out = QString::fromUtf8(dir) + QLatin1Char('/');

    struct Size
    {
        const char* name;
        QSize size;
    };
    for (const Size& size : {Size{"phone", PHONE}, Size{"tablet", TABLET}, Size{"desktop", DESKTOP}})
    {
        const QString prefix = out + size.name + QLatin1Char('-');
        Shell s(size.size);
        s.feedback->setProperty("rightHoldMs", 60000);
        s.feedback->setProperty("wrongHoldMs", 60000);
        s.feedback->setProperty("boomHoldMs", 60000);

        // CP2: level 1, mid game.
        s.rig.controller.answer(s.rig.controller.session_view().question.correct_tile);
        QMetaObject::invokeMethod(s.feedback, "clear");
        s.rig.play_for_ms(2000);
        s.h.settle();
        CHECK(s.h.grab(prefix + "play.png"));

        // CP4: a wrong answer.
        s.rig.controller.answer(s.rig.controller.session_view().question.correct_tile);
        QMetaObject::invokeMethod(s.feedback, "clear");
        s.rig.controller.answer(s.rig.controller.session_view().question.correct_tile);
        QMetaObject::invokeMethod(s.feedback, "clear");
        s.rig.answer_wrong();
        QTest::qWait(400);
        CHECK(s.h.grab(prefix + "wrong.png"));
        QMetaObject::invokeMethod(s.feedback, "clear");

        // CP5: a hint.
        s.rig.controller.useHint();
        QTest::qWait(400);
        CHECK(s.h.grab(prefix + "hint.png"));

        // CP3: the alert.
        s.rig.play_for_ms(49000);
        QTest::qWait(300);
        CHECK(s.h.grab(prefix + "alert.png"));

        // The alert with a wrong answer held, and then with a right one.
        s.rig.answer_wrong();
        QTest::qWait(400);
        CHECK(s.h.grab(prefix + "alert-wrong.png"));
        QMetaObject::invokeMethod(s.feedback, "clear");
        s.rig.controller.answer(s.rig.controller.session_view().question.correct_tile);
        QTest::qWait(400);
        CHECK(s.h.grab(prefix + "alert-right.png"));
        QMetaObject::invokeMethod(s.feedback, "clear");

        // CP16: the pause overlay.
        s.rig.controller.pause();
        s.h.settle();
        CHECK(s.h.grab(prefix + "pause.png"));
        s.rig.controller.resume();

        // The boom.
        s.rig.play_for_ms(61000);
        QTest::qWait(500);
        CHECK(s.h.grab(prefix + "boom.png"));

        // CP13: an endless level, with the time gained by a right answer hanging from the fuse.
        {
            Shell endless(size.size);
            for (int level = 1; level <= 12; ++level)
            {
                endless.rig.play_level_right();
                if (level < 12)
                {
                    endless.rig.controller.nextLevel();
                }
            }
            endless.wait_out_holds();
            click(endless.h.find_item("keepGoingButton"));
            endless.h.settle();
            // Levels 13, 14 and 20, each with a right answer held: the time gained hangs from the fuse.
            for (int level = 13; level <= 20; ++level)
            {
                if (level == 13 || level == 14 || level == 20)
                {
                    endless.rig.play_for_ms(9000);
                    endless.h.settle();
                    endless.feedback->setProperty("rightHoldMs", 60000);
                    click(endless.tile(endless.rig.controller.session_view().question.correct_tile));
                    QTest::qWait(400);
                    CHECK(endless.h.grab(prefix + (level == 13 ? QStringLiteral("endless.png")
                                                               : QStringLiteral("endless-%1.png").arg(level))));
                    QMetaObject::invokeMethod(endless.feedback, "clear");
                    endless.h.settle();
                }
                endless.rig.play_level_right();
                endless.rig.controller.nextLevel();
                endless.h.settle();
            }
        }
    }
}

// The play screen in the alert with a phone's margins, for the owner to look at: the frame at the window edge. Run by
// hand with HBM_SCREENSHOT_DIR set to a folder; it does nothing otherwise and is hidden from the ordinary run.
// The "masked" shots draw black over everything outside a rounded rectangle at the window edge, which is roughly what a
// phone's own corners leave visible (iPhones run from about 39 to 62 points); the mask is made here, never in the app.
TEST_CASE("Screenshots of the alert frame at the window edge", "[.][screenshots]")
{
    const QString dir = qEnvironmentVariable("HBM_SCREENSHOT_DIR");
    if (dir.isEmpty())
    {
        return;
    }
    struct Shot
    {
        const char* file;
        QSize size;
        Margins margins;
        int mask_radius; // 0: no mask
    };
    const Shot shots[] = {
        {"alert-frame-390x844-phone-margins", PHONE, PHONE_MARGINS, 0},
        {"alert-frame-390x844-masked-r47", PHONE, PHONE_MARGINS, 47},
        {"alert-frame-390x844-masked-r55", PHONE, PHONE_MARGINS, 55},
        {"alert-frame-820x1180-tablet-margins", TABLET, Margins{0, 24, 0, 20}, 0},
        {"alert-frame-1280x800-desktop", DESKTOP, Margins{}, 0},
    };
    for (const Shot& shot : shots)
    {
        Shell s(shot.size, true, shot.margins);
        s.rig.play_for_ms(50000);
        s.h.settle();
        REQUIRE(s.rig.controller.alert());
        // The frame pulses; the shot is of it at full strength.
        QTest::qWait(900);
        QImage image = s.h.grab_image();
        REQUIRE_FALSE(image.isNull());
        if (shot.mask_radius > 0)
        {
            const QRectF bounds(QPointF(0, 0), QSizeF(image.size()));
            QPainterPath window;
            window.addRect(bounds);
            QPainterPath screen;
            screen.addRoundedRect(bounds, shot.mask_radius, shot.mask_radius);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.fillPath(window.subtracted(screen), Qt::black);
        }
        CHECK(image.save(QStringLiteral("%1/%2.png").arg(dir, QLatin1String(shot.file))));
    }
}
