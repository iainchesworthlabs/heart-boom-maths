// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QByteArray>
#include <QQuickItem>
#include <QTest>
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

/// The whole app shell on a controller, with the holds shortened so a test can wait them out.
struct Shell
{
    explicit Shell(QSize size, bool start = true) : h(&rig.controller, QStringLiteral("AppShell.qml"), size)
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

TEST_CASE("The points and chips match the boards on a tablet and a desktop window", "[ui][play][stats]")
{
    const auto scene_top = [](QQuickItem* item) { return item->mapToScene(QPointF(0, 0)).y(); };
    SECTION("tablet")
    {
        Shell s(TABLET);
        QQuickItem* value = s.h.find_item("pointsValue");
        QQuickItem* label = s.h.find_item("pointsLabel");
        QQuickItem* multiplier = s.h.find_item("multiplierBadge");
        QQuickItem* wallet = s.h.find_item("coinWallet");
        QQuickItem* heart = s.h.find_item("heartBomb");
        REQUIRE(value != nullptr);
        REQUIRE(label != nullptr);
        REQUIRE(multiplier != nullptr);
        REQUIRE(wallet != nullptr);
        REQUIRE(heart != nullptr);
        // The number and POINTS stand on one baseline.
        CHECK_THAT(scene_top(value) + property_of(value, "baselineOffset").toDouble(),
                   Catch::Matchers::WithinAbs(scene_top(label) + property_of(label, "baselineOffset").toDouble(), 0.6));
        // The points card is 82 high (its border, 10 above and below and a 52 line) and starts at y 179 as on the board;
        // the chips are 64 or 65 high from y 275.
        CHECK_THAT(scene_top(value), Catch::Matchers::WithinAbs(179.0 + 5.0 + 10.0, 2.0));
        CHECK_THAT(multiplier->height(), Catch::Matchers::WithinAbs(64.0, 1.5));
        CHECK_THAT(wallet->height(), Catch::Matchers::WithinAbs(multiplier->height(), 0.1));
        CHECK_THAT(scene_top(multiplier), Catch::Matchers::WithinAbs(275.0, 1.5));
        // The stack of points and chips is centred on the 190 heart.
        const double stack_top = 179.0;
        const double stack_bottom = scene_top(multiplier) + multiplier->height();
        const double heart_middle = scene_top(heart) + heart->height() / 2;
        CHECK_THAT((stack_top + stack_bottom) / 2, Catch::Matchers::WithinAbs(heart_middle, 2.0));
    }
    SECTION("desktop")
    {
        Shell s(DESKTOP);
        QQuickItem* multiplier = s.h.find_item("multiplierBadge");
        QQuickItem* hint = s.h.find_item("hintButton");
        QQuickItem* heart = s.h.find_item("heartBomb");
        REQUIRE(multiplier != nullptr);
        REQUIRE(hint != nullptr);
        REQUIRE(heart != nullptr);
        // CP10: the chips span 485 to 534 and the hint button 679 to 730; the heart's box starts at 145.
        CHECK_THAT(scene_top(multiplier), Catch::Matchers::WithinAbs(485.0, 2.0));
        CHECK_THAT(multiplier->height(), Catch::Matchers::WithinAbs(50.0, 1.5));
        CHECK_THAT(scene_top(hint), Catch::Matchers::WithinAbs(679.0, 2.0));
        CHECK_THAT(hint->height(), Catch::Matchers::WithinAbs(52.0, 1.0));
        CHECK_THAT(scene_top(heart), Catch::Matchers::WithinAbs(145.0, 2.0));
        // The panel's content starts 29 in (a 5 border and 24 of padding), so the fuse is 302 wide.
        QQuickItem* fuse = s.h.find_item("fuseBar");
        REQUIRE(fuse != nullptr);
        CHECK_THAT(fuse->mapToScene(QPointF(0, 0)).x(), Catch::Matchers::WithinAbs(56.0 + 29.0, 0.6));
        CHECK_THAT(fuse->width(), Catch::Matchers::WithinAbs(302.0, 0.6));
    }
}

TEST_CASE("The digits of the question and its slot share one centre line at every size", "[ui][play][question]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
    };
    const Arrangement arrangement = GENERATE(Arrangement{"phone", PHONE}, Arrangement{"tablet", TABLET},
                                             Arrangement{"desktop", DESKTOP});
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
    click(s.tile(s.rig.controller.session_view().question.correct_tile));
    s.h.settle();
    CHECK(s.visible("timeGain"));
    CHECK(s.text_of("timeGain") == QStringLiteral("+%1 s").arg(s.rig.controller.seconds_per_right_answer()));
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

        // CP16: the pause overlay.
        s.rig.controller.pause();
        s.h.settle();
        CHECK(s.h.grab(prefix + "pause.png"));
        s.rig.controller.resume();

        // The boom.
        s.rig.play_for_ms(61000);
        QTest::qWait(500);
        CHECK(s.h.grab(prefix + "boom.png"));
    }
}
