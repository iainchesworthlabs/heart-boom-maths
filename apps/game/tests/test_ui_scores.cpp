// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QByteArray>
#include <QQuickItem>
#include <QRectF>
#include <QString>
#include <QTest>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using namespace hbm_test;
using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using State = GameController::State;

namespace
{

constexpr QSize PHONE{390, 844};
constexpr QSize TABLET{820, 1180};
constexpr QSize DESKTOP{1280, 800};

constexpr int SHORT_HOLD_MS = 40;
constexpr int PAST_HOLD_MS = 160;
constexpr std::uint64_t SEED = 20261114;

struct SizeCase
{
    const char* name;
    QSize size;
};

#define ALL_SIZES GENERATE(SizeCase{"phone", PHONE}, SizeCase{"tablet", TABLET}, SizeCase{"desktop", DESKTOP})

std::string save_text(const std::vector<ScoreEntry>& top10, bool won_once = false)
{
    SaveData data;
    data.top10 = top10;
    data.won_once = won_once;
    const auto text = serialise(data);
    REQUIRE(text.has_value());
    return *text;
}

/// Ten entries from 100 down to 10 points, and five from 100 down to 60.
std::vector<ScoreEntry> full_list()
{
    std::vector<ScoreEntry> entries;
    const char* names[] = {"BOMB SQUAD", "ROCKET", "DAD", "MUM", "SPARKY", "NINJA", "CAT", "ALEX", "SUNNY", "TINY"};
    for (int i = 0; i < 10; ++i)
    {
        entries.push_back({names[i], 100 - 10 * i});
    }
    return entries;
}

std::vector<ScoreEntry> short_list()
{
    auto entries = full_list();
    entries.resize(5);
    return entries;
}

/// The whole app shell on a controller, with the holds shortened so a test can wait them out.
struct Shell
{
    explicit Shell(QSize size) : h(&rig.controller, QStringLiteral("AppShell.qml"), size)
    {
        shorten_holds();
    }

    Shell(QSize size, std::string save)
        : rig(SEED, std::move(save)), h(&rig.controller, QStringLiteral("AppShell.qml"), size)
    {
        shorten_holds();
    }

    void shorten_holds()
    {
        QObject* feedback = h.find("answerFeedback");
        REQUIRE(feedback != nullptr);
        feedback->setProperty("rightHoldMs", SHORT_HOLD_MS);
        feedback->setProperty("wrongHoldMs", SHORT_HOLD_MS * 2);
        feedback->setProperty("boomHoldMs", SHORT_HOLD_MS * 2);
    }

    [[nodiscard]] GameController& c()
    {
        return rig.controller;
    }

    [[nodiscard]] QString screen() const
    {
        return property_of(h.root(), "screen").toString();
    }

    [[nodiscard]] bool has(const char* name) const
    {
        return h.find(QLatin1String(name)) != nullptr;
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

    /// The number of a StatRow: its Value text.
    [[nodiscard]] QString value_of(const char* row) const
    {
        return text_of((std::string(row) + "Value").c_str());
    }

    [[nodiscard]] QString label_of(const char* row) const
    {
        return text_of((std::string(row) + "Label").c_str());
    }

    void wait_out_holds()
    {
        QTest::qWait(PAST_HOLD_MS);
        h.settle();
    }

    void tap(const char* name)
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        REQUIRE(item != nullptr);
        click(item);
        h.settle();
    }

    /// The item's rectangle in the window.
    [[nodiscard]] QRectF rect_of(const char* name) const
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        REQUIRE(item != nullptr);
        return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    }

    /// Every named item lies inside the window.
    void check_inside(std::initializer_list<const char*> names)
    {
        const QRectF window(QPointF(0, 0), QSizeF(h.root()->width(), h.root()->height()));
        for (const char* name : names)
        {
            INFO(name);
            const QRectF r = rect_of(name);
            CHECK(r.left() >= window.left() - 0.5);
            CHECK(r.top() >= window.top() - 0.5);
            CHECK(r.right() <= window.right() + 0.5);
            CHECK(r.bottom() <= window.bottom() + 0.5);
        }
    }

    /// The items, from top to bottom, do not overlap one another.
    void check_stacked(std::initializer_list<const char*> names)
    {
        const char* previous = nullptr;
        for (const char* name : names)
        {
            if (previous != nullptr)
            {
                INFO(previous << " above " << name);
                CHECK(rect_of(name).top() >= rect_of(previous).bottom() - 0.5);
            }
            previous = name;
        }
    }

    void start()
    {
        c().startGame();
        h.settle();
    }

    /// Plays a whole level right through the controller, as taps would.
    void clear_level()
    {
        rig.play_level_right();
    }

    /// Clears level 12 and waits for the win screen.
    void win_the_game()
    {
        c().startGame();
        for (int level = 1; level <= 12; ++level)
        {
            clear_level();
            if (level < 12)
            {
                c().nextLevel();
            }
        }
        wait_out_holds();
    }

    /// Three right answers (40 points) and then the bomb, and waits for the screen after it.
    void blow_up_after(int right_answers)
    {
        start();
        for (int i = 0; i < right_answers; ++i)
        {
            rig.answer_right();
        }
        rig.play_for_ms(61000);
        wait_out_holds();
    }

    void type_keys(const char* keys)
    {
        for (const char* key = keys; *key != '\0'; ++key)
        {
            QTest::keyClick(&h.view(), *key);
        }
        h.settle();
    }

    UiRig rig;
    QmlHarness h;
};

} // namespace

// ---- Level clear (CP6)
// ------------------------------------------------------------------------------------------------

TEST_CASE("The level-clear card shows the numbers the core gave, and NEXT LEVEL starts the next level",
          "[ui][scores][levelclear]")
{
    const SizeCase size = ALL_SIZES;
    CAPTURE(size.name);
    Shell s(size.size);
    s.start();
    s.clear_level();
    s.wait_out_holds();
    REQUIRE(s.c().state() == State::LevelClear);
    REQUIRE(s.screen() == "levelClear");
    REQUIRE(s.has("levelClearScreen"));
    CHECK_FALSE(s.has("playScreen"));

    // Level 1 right through: ten right answers (+230 at the multiplier steps), a perfect level and all 60 s left.
    CHECK(s.text_of("levelClearBurst") == "CLEAR!");
    CHECK(property_of(s.h.find("levelClearBurst"), "subText").toString() == "LEVEL 1");
    CHECK(s.label_of("answersRow") == "10 of 10 right");
    CHECK(s.value_of("answersRow") == "+230");
    CHECK(s.visible("perfectRow"));
    CHECK(s.label_of("perfectRow") == "PERFECT LEVEL!");
    CHECK(s.value_of("perfectRow") == "+100");
    CHECK(s.label_of("timeRow") == QString::fromUtf8("60 s left × 10"));
    CHECK(s.value_of("timeRow") == "+600");
    CHECK(s.label_of("levelPointsRow") == "LEVEL POINTS");
    CHECK(s.value_of("levelPointsRow") == "930");
    CHECK(s.c().points() == 930);
    // Ten coins, a streak bonus at the fifth and tenth, and five for the perfect level; the wallet holds exactly that.
    CHECK(property_of(s.h.find("coinsCard"), "text").toString() == "+17 COINS");
    CHECK(property_of(s.h.find("coinsCard"), "note").toString() == "WALLET 17");
    CHECK(s.c().wallet() == 17);
    CHECK(s.text_of("nextUpText") == "Next up: level 2, take away, up to 5");

    // Everything is on the screen, in order, and the button is a real target.
    s.check_inside({"levelClearBurst", "pointsCard", "coinsCard", "nextLevelButton", "nextUpText"});
    s.check_stacked({"levelClearBurst", "pointsCard", "coinsCard", "nextLevelButton", "nextUpText"});
    CHECK(s.rect_of("nextLevelButton").height() >= 44);

    s.tap("nextLevelButton");
    CHECK(s.c().state() == State::Playing);
    CHECK(s.c().level() == 2);
    CHECK(s.screen() == "play");
    CHECK(s.has("playScreen"));
}

TEST_CASE("A level with a wrong answer has no perfect row and says how many were right", "[ui][scores][levelclear]")
{
    Shell s(PHONE);
    s.start();
    s.rig.answer_wrong();
    for (int i = 0; i < 9; ++i)
    {
        s.rig.answer_right();
    }
    s.wait_out_holds();
    REQUIRE(s.screen() == "levelClear");
    CHECK_FALSE(s.visible("perfectRow"));
    CHECK(s.label_of("answersRow") == "9 of 10 right");

    // The rows add up to the level points the card shows.
    const QVariantMap card = s.c().level_clear();
    CHECK(s.value_of("answersRow") == "+" + QString::number(card.value("answerPoints").toInt()));
    CHECK(s.value_of("levelPointsRow") == QString::number(card.value("levelPoints").toInt()));
    CHECK(card.value("levelPoints").toInt() ==
          card.value("answerPoints").toInt() + card.value("timeBonusPoints").toInt());
    CHECK(s.visible("coinsCard"));
}

TEST_CASE("A level that came back from a saved run part way shows what the core gave and no sums",
          "[ui][scores][levelclear]")
{
    std::string text;
    {
        UiRig first;
        first.controller.startGame();
        first.answer_right();
        first.answer_right();
        first.controller.pause();
        REQUIRE(first.store.text().has_value());
        text = *first.store.text();
    }
    Shell s(PHONE, text);
    REQUIRE(s.c().continueGame());
    s.c().resume();
    for (int i = 0; i < 8; ++i)
    {
        s.rig.answer_right();
    }
    s.wait_out_holds();
    REQUIRE(s.screen() == "levelClear");
    CHECK(s.label_of("answersRow") == "10 of 10 right");
    CHECK(s.value_of("answersRow").isEmpty());
    CHECK(s.label_of("levelPointsRow") == "SCORE");
    CHECK(s.value_of("levelPointsRow") == QString::number(s.c().points()));
    CHECK(property_of(s.h.find("coinsCard"), "text").toString().startsWith("WALLET"));
    CHECK(property_of(s.h.find("coinsCard"), "note").toString().isEmpty());
}

// ---- Win (CP8)
// --------------------------------------------------------------------------------------------------------

TEST_CASE("The win screen shows the bonus the win paid and offers KEEP GOING and FINISH", "[ui][scores][win]")
{
    const SizeCase size = GENERATE(SizeCase{"phone", PHONE}, SizeCase{"desktop", DESKTOP});
    CAPTURE(size.name);
    Shell s(size.size);
    s.win_the_game();
    REQUIRE(s.c().state() == State::Won);
    REQUIRE(s.screen() == "won");
    CHECK(s.text_of("winBurst") == "YOU\nWIN!");
    CHECK(s.text_of("winHeading") == "You cleared all 12 levels!");
    CHECK(property_of(s.h.find("bonusCard"), "text").toString() == "+1,000 COINS");
    CHECK(s.text_of("winWallet") ==
          "Wallet " + QLocale(QLocale::English).toString(static_cast<qlonglong>(s.c().wallet())));
    CHECK(s.c().wallet() >= 1000);
    CHECK(s.visible("keepGoingButton"));
    CHECK(s.visible("finishButton"));
    s.check_inside(
        {"winBurst", "winHeading", "bonusCard", "winWallet", "keepGoingButton", "finishButton", "endlessNote"});
    s.check_stacked(
        {"winBurst", "winHeading", "bonusCard", "winWallet", "keepGoingButton", "finishButton", "endlessNote"});

    // The star burst's title stays inside it, leaning as it does.
    CHECK(property_of(s.h.find("winBurst"), "fitsInside").toBool());
}

TEST_CASE("KEEP GOING from the win screen starts the first endless level", "[ui][scores][win]")
{
    Shell s(PHONE);
    s.win_the_game();
    REQUIRE(s.screen() == "won");
    s.tap("keepGoingButton");
    CHECK(s.c().state() == State::Playing);
    CHECK(s.c().level() == 13);
    CHECK(s.c().endless());
    CHECK(s.screen() == "play");
    CHECK(s.has("playScreen"));
}

TEST_CASE("FINISH from the win screen ends the game, and its card has no BOOM", "[ui][scores][win]")
{
    Shell s(PHONE);
    s.win_the_game();
    REQUIRE(s.screen() == "won");
    s.tap("finishButton");
    CHECK(s.c().state() == State::GameOver);
    CHECK(s.c().end_reason() == GameController::EndReason::Finish);
    // The score is good for the top 10, so the name screen comes first.
    REQUIRE(s.screen() == "nameEntry");
    s.tap("skipButton");
    REQUIRE(s.screen() == "gameOver");
    CHECK_FALSE(s.visible("boomBurst"));
    CHECK(s.visible("gameOverHeading"));
    CHECK(s.text_of("gameOverHeading") == "GAME OVER");
}

TEST_CASE("When an earlier game paid the win bonus the win screen says so and shows no payment", "[ui][scores][win]")
{
    Shell s(PHONE, save_text({}, true));
    s.win_the_game();
    REQUIRE(s.screen() == "won");
    CHECK(s.c().win_bonus() == 0);
    const QString bonus = property_of(s.h.find("bonusCard"), "text").toString();
    CHECK(bonus == "BONUS WON BEFORE");
    CHECK_FALSE(bonus.contains("1,000"));
    CHECK(s.text_of("winWallet") == "Wallet " + QString::number(s.c().wallet()));
    CHECK(s.c().wallet() < 1000);
    s.check_inside({"bonusCard"});
    // The words and the coin fit in the card.
    QQuickItem* card = s.h.find_item("bonusCard");
    QQuickItem* words = qobject_cast<QQuickItem*>(property_of(card, "textItem").value<QObject*>());
    REQUIRE(words != nullptr);
    const QRectF words_rect = words->mapRectToScene(QRectF(0, 0, words->width(), words->height()));
    CHECK(words_rect.right() <= s.rect_of("bonusCard").right());
    CHECK(words_rect.left() >= s.rect_of("bonusCard").left());
}

// ---- Game over (CP7), name entry (CP11) and the top 10 (CP12)
// -----------------------------------------------------------

TEST_CASE("A game over by the bomb shows BOOM and the numbers of the game", "[ui][scores][gameover]")
{
    const SizeCase size = ALL_SIZES;
    CAPTURE(size.name);
    Shell s(size.size, save_text({{"OLD", 5000}}));
    s.blow_up_after(3); // 10 + 10 + 20 points, and the list has room
    REQUIRE(s.screen() == "nameEntry");
    s.tap("skipButton");
    REQUIRE(s.screen() == "gameOver");

    CHECK(s.c().end_reason() == GameController::EndReason::Bomb);
    CHECK(s.visible("boomBurst"));
    CHECK(s.text_of("boomBurst") == "BOOM!");
    CHECK(property_of(s.h.find("boomBurst"), "fitsInside").toBool());
    CHECK(s.text_of("gameOverHeading") == "GAME OVER");
    CHECK(s.value_of("levelReachedRow") == "1");
    CHECK(s.value_of("scoreRow") == "40");
    CHECK(s.value_of("bestScoreRow") == "5,000");
    CHECK_FALSE(s.visible("newBestBadge"));
    CHECK(s.value_of("coinsEarnedRow") == "+" + QString::number(s.c().coins_earned()));
    CHECK(s.c().coins_earned() == 3);
    CHECK(property_of(s.h.find("walletCard"), "text").toString() == "WALLET 3");
    CHECK(property_of(s.h.find("walletCard"), "note").toString() == "saved");
    CHECK(s.visible("playAgainButton"));
    CHECK(s.visible("top10Button"));
    CHECK(s.visible("homeButton"));
    s.check_inside(
        {"boomBurst", "gameOverHeading", "resultCard", "walletCard", "playAgainButton", "top10Button", "homeButton"});
    s.check_stacked({"boomBurst", "gameOverHeading", "resultCard", "walletCard", "playAgainButton", "top10Button"});
    CHECK(s.rect_of("top10Button").height() >= 44);
    CHECK(s.rect_of("homeButton").height() >= 44);
    // TOP 10 and HOME share a row.
    CHECK(s.rect_of("top10Button").right() <= s.rect_of("homeButton").left());

    s.tap("playAgainButton");
    CHECK(s.c().state() == State::Playing);
    CHECK(s.c().level() == 1);
    CHECK(s.c().points() == 0);
    CHECK(s.screen() == "play");
}

TEST_CASE("A score that earns a place goes through name entry, then the top 10 with the new entry marked",
          "[ui][scores][nameentry][top10]")
{
    const SizeCase size = ALL_SIZES;
    CAPTURE(size.name);
    Shell s(size.size, save_text(short_list()));
    s.blow_up_after(3);
    REQUIRE(s.c().name_entry_pending());
    REQUIRE(s.screen() == "nameEntry");
    CHECK(s.c().name_entry_rank() == 6);

    // The card says what happened.
    CHECK(s.value_of("rankRow") == "6 of 10");
    CHECK(s.value_of("entryScoreRow") == "40");
    CHECK(s.value_of("entryLevelRow") == "1");
    CHECK(s.text_of("nameCounter") == "0 / 12");
    CHECK(s.text_of("nameText").isEmpty());
    QQuickItem* save = s.h.find_item("saveButton");
    REQUIRE(save != nullptr);
    CHECK_FALSE(save->isEnabled()); // SAVE needs a character

    s.type_keys("sparky");
    CHECK(s.text_of("nameText") == "SPARKY");
    CHECK(s.text_of("nameCounter") == "6 / 12");
    CHECK(save->isEnabled());
    s.check_inside({"nameBurst", "entryCard", "nameField", "saveButton", "skipButton"});
    s.check_stacked({"nameBurst", "entryCard", "nameField", "saveButton"});

    s.tap("saveButton");
    REQUIRE(s.screen() == "top10");
    CHECK(s.c().highlight_rank() == 6);
    CHECK_FALSE(s.c().name_entry_pending());
    REQUIRE(s.c().top10().size() == 6);
    CHECK(s.c().top10().at(5).toMap().value("name").toString() == "SPARKY");

    // The new row is the only one marked, and it holds the name and score just saved.
    for (int rank = 1; rank <= 6; ++rank)
    {
        INFO(rank);
        QQuickItem* row = s.h.find_item(QStringLiteral("scoreRow%1").arg(rank));
        REQUIRE(row != nullptr);
        CHECK(property_of(row, "highlighted").toBool() == (rank == 6));
        CHECK(property_of(row, "rank").toInt() == rank);
    }
    QQuickItem* mine = s.h.find_item("scoreRow6");
    CHECK(property_of(mine, "name").toString() == "SPARKY");
    CHECK(property_of(mine, "scoreText").toString() == "40");
    CHECK(s.h.find_item("scoreRow7") == nullptr);
    CHECK(s.text_of("top10PlayButton") == "PLAY AGAIN");
    s.check_inside({"scoreRow1", "scoreRow6", "top10PlayButton", "backButton"});
    s.check_stacked({"scoreRow1", "scoreRow2", "scoreRow3", "scoreRow4", "scoreRow5", "scoreRow6", "top10PlayButton"});
    CHECK(s.rect_of("backButton").width() >= 44);
    CHECK(s.rect_of("backButton").height() >= 44);

    // Back leaves the list for the game-over card, which now reads NEW BEST? No: 40 is not the best of the list.
    s.tap("backButton");
    CHECK(s.screen() == "gameOver");
    CHECK(s.value_of("scoreRow") == "40");
    CHECK_FALSE(s.visible("newBestBadge"));

    s.tap("playAgainButton");
    CHECK(s.c().state() == State::Playing);
    CHECK(s.c().level() == 1);
}

TEST_CASE("A new best score is marked NEW BEST on the game-over card once it is saved", "[ui][scores][gameover]")
{
    Shell s(PHONE);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    CHECK(s.value_of("rankRow") == "1 of 10");
    s.type_keys("ada");
    s.tap("saveButton");
    REQUIRE(s.screen() == "top10");
    CHECK(property_of(s.h.find_item("scoreRow1"), "highlighted").toBool());
    s.tap("backButton");
    REQUIRE(s.screen() == "gameOver");
    CHECK(s.visible("newBestBadge"));
    CHECK(s.text_of("newBestBadge") == "NEW BEST!");
    CHECK(s.value_of("bestScoreRow") == "40");
}

TEST_CASE("SKIP drops the entry and lands on the game-over card; the top 10 stays empty", "[ui][scores][nameentry]")
{
    const SizeCase size = ALL_SIZES;
    CAPTURE(size.name);
    Shell s(size.size);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    s.type_keys("nobody");
    s.tap("skipButton");
    CHECK(s.screen() == "gameOver");
    CHECK(s.c().top10().isEmpty());
    CHECK_FALSE(s.visible("newBestBadge"));
    CHECK(s.value_of("bestScoreRow") == "0");

    // The top 10 opens from the card, says it is empty, and backs out to the card again.
    s.tap("top10Button");
    REQUIRE(s.screen() == "top10");
    CHECK(s.visible("emptyCard"));
    CHECK_FALSE(s.visible("top10List"));
    s.check_inside({"emptyCard", "top10PlayButton", "backButton"});
    s.tap("backButton");
    CHECK(s.screen() == "gameOver");
}

TEST_CASE("A score of nothing, or one that does not beat a full list, goes straight to the game-over card",
          "[ui][scores][gameover]")
{
    {
        Shell s(PHONE);
        s.blow_up_after(0);
        CHECK(s.c().final_score() == 0);
        CHECK(s.screen() == "gameOver");
        CHECK_FALSE(s.has("nameEntryScreen"));
        CHECK(s.visible("boomBurst"));
        CHECK(s.value_of("scoreRow") == "0");
    }
    {
        // Ten entries, all better than a game of 40 points: it does not beat the last one.
        auto entries = full_list();
        for (auto& entry : entries)
        {
            entry.score += 1000;
        }
        Shell s(PHONE, save_text(entries));
        s.blow_up_after(3);
        CHECK(s.c().final_score() == 40);
        CHECK_FALSE(s.c().name_entry_pending());
        CHECK(s.screen() == "gameOver");
        CHECK_FALSE(s.has("nameEntryScreen"));
        CHECK(s.value_of("bestScoreRow") == "1,100");
    }
}

TEST_CASE("Quitting from the pause screen ends the game without a BOOM", "[ui][scores][gameover]")
{
    Shell s(PHONE);
    s.start();
    s.rig.answer_right();
    s.wait_out_holds();
    s.c().pause();
    s.h.settle();
    REQUIRE(s.visible("pauseOverlay"));
    s.tap("quitButton");
    CHECK(s.c().state() == State::GameOver);
    CHECK(s.c().end_reason() == GameController::EndReason::Quit);
    REQUIRE(s.screen() == "nameEntry");
    s.tap("skipButton");
    REQUIRE(s.screen() == "gameOver");
    CHECK_FALSE(s.visible("boomBurst"));
    CHECK(s.text_of("gameOverHeading") == "GAME OVER");
    CHECK(s.value_of("scoreRow") == "10");
    s.check_inside({"gameOverHeading", "resultCard", "walletCard", "playAgainButton", "top10Button", "homeButton"});
    s.check_stacked({"gameOverHeading", "resultCard", "walletCard", "playAgainButton", "top10Button"});
}

TEST_CASE("HOME on the game-over card shows the title; PLAY there starts a new game", "[ui][scores][gameover]")
{
    Shell s(PHONE);
    s.blow_up_after(3);
    s.tap("skipButton");
    REQUIRE(s.screen() == "gameOver");
    s.tap("homeButton");
    CHECK(s.screen() == "title");
    CHECK(s.visible("playButton"));
    CHECK_FALSE(s.has("gameOverScreen"));
    s.tap("playButton");
    CHECK(s.c().state() == State::Playing);
    CHECK(s.c().level() == 1);
    CHECK(s.screen() == "play");
}

TEST_CASE(
    "The top 10 opens from the title and from the game-over card, and its back arrow returns to where it was opened",
    "[ui][scores][top10]")
{
    Shell s(PHONE, save_text(full_list()));
    REQUIRE(s.screen() == "title");

    // From the title: a list with no new entry, and PLAY.
    QMetaObject::invokeMethod(s.h.root(), "showTop10");
    s.h.settle();
    REQUIRE(s.screen() == "top10");
    CHECK(s.text_of("top10PlayButton") == "PLAY");
    for (int rank = 1; rank <= 10; ++rank)
    {
        INFO(rank);
        QQuickItem* row = s.h.find_item(QStringLiteral("scoreRow%1").arg(rank));
        REQUIRE(row != nullptr);
        CHECK_FALSE(property_of(row, "highlighted").toBool());
    }
    CHECK(property_of(s.h.find_item("scoreRow1"), "name").toString() == "BOMB SQUAD");
    CHECK(property_of(s.h.find_item("scoreRow1"), "scoreText").toString() == "100");
    CHECK(property_of(s.h.find_item("scoreRow10"), "name").toString() == "TINY");
    s.check_inside({"scoreRow1", "scoreRow10", "top10PlayButton"});
    s.check_stacked({"backButton", "scoreRow1", "scoreRow2", "scoreRow3", "scoreRow4", "scoreRow5", "scoreRow6",
                     "scoreRow7", "scoreRow8", "scoreRow9", "scoreRow10", "top10PlayButton"});
    s.tap("backButton");
    CHECK(s.screen() == "title");

    // After a game that earned a place, HOME and then the list: the entry is not marked as new any more.
    s.start();
    s.rig.answer_right();
    s.rig.play_for_ms(61000);
    s.wait_out_holds();
    CHECK(s.c().final_score() == 10);
    CHECK_FALSE(s.c().name_entry_pending()); // 10 points does not beat the 10th (10 is not above 10)
    REQUIRE(s.screen() == "gameOver");
    s.tap("homeButton");
    REQUIRE(s.screen() == "title");
    QMetaObject::invokeMethod(s.h.root(), "showTop10");
    s.h.settle();
    CHECK(s.screen() == "top10");
    s.tap("backButton");
    CHECK(s.screen() == "title"); // back to the title, not to the card it came from
}

TEST_CASE("The top 10 marks the new entry in a full list and drops the one pushed off", "[ui][scores][top10]")
{
    const SizeCase size = ALL_SIZES;
    CAPTURE(size.name);
    Shell s(size.size, save_text(full_list()));
    s.blow_up_after(3); // 40 points: above the 10th (10), the 9th (20) and the 8th (30)
    REQUIRE(s.screen() == "nameEntry");
    // An equal score ranks after the older entry, so 40 is the 8th place (above 30, below the older 40).
    CHECK(s.c().name_entry_rank() == 8);
    s.type_keys("zed");
    s.tap("saveButton");
    REQUIRE(s.screen() == "top10");
    CHECK(s.c().highlight_rank() == 8);
    int marked = 0;
    for (int rank = 1; rank <= 10; ++rank)
    {
        QQuickItem* row = s.h.find_item(QStringLiteral("scoreRow%1").arg(rank));
        REQUIRE(row != nullptr);
        marked += property_of(row, "highlighted").toBool() ? 1 : 0;
    }
    CHECK(marked == 1);
    CHECK(property_of(s.h.find_item("scoreRow8"), "highlighted").toBool());
    CHECK(property_of(s.h.find_item("scoreRow8"), "name").toString() == "ZED");
    CHECK(property_of(s.h.find_item("scoreRow10"), "name").toString() == "SUNNY"); // TINY was pushed off
    CHECK(s.h.find_item("scoreRow11") == nullptr);
    s.check_inside({"scoreRow1", "scoreRow10", "top10PlayButton", "backButton"});
    s.check_stacked({"backButton", "scoreRow1", "scoreRow10", "top10PlayButton"});
}

// ---- Name entry details
// -----------------------------------------------------------------------------------------------

TEST_CASE("The name field takes what the core allows and the counter follows the length from the tuning",
          "[ui][scores][nameentry]")
{
    Shell s(PHONE);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    QQuickItem* save = s.h.find_item("saveButton");
    REQUIRE(save != nullptr);

    // Lower case becomes capitals; digits and marks are left out.
    s.type_keys("a1!b c");
    CHECK(s.text_of("nameText") == "AB C");
    CHECK(s.text_of("nameCounter") == QString("4 / %1").arg(s.c().name_max_length()));
    CHECK(save->isEnabled());

    // Delete takes the last character off.
    QTest::keyClick(&s.h.view(), Qt::Key_Backspace);
    s.h.settle();
    CHECK(s.text_of("nameText") == "AB ");
    QTest::keyClick(&s.h.view(), Qt::Key_Backspace);
    QTest::keyClick(&s.h.view(), Qt::Key_Backspace);
    QTest::keyClick(&s.h.view(), Qt::Key_Backspace);
    QTest::keyClick(&s.h.view(), Qt::Key_Backspace); // nothing left to delete
    s.h.settle();
    CHECK(s.text_of("nameText").isEmpty());
    CHECK_FALSE(save->isEnabled());

    // Spaces alone are not a name: SAVE stays off although the core would accept them.
    s.type_keys("   ");
    CHECK(s.text_of("nameText") == "   ");
    CHECK_FALSE(save->isEnabled());
    s.tap("saveButton");
    CHECK(s.screen() == "nameEntry");
    CHECK(s.c().top10().isEmpty());

    // The length stops at the tuning's number, and the letter keys go grey with it.
    s.type_keys("abcdefghijklmnopqrstuvwxyz");
    const int longest = s.c().name_max_length();
    CHECK(s.text_of("nameText").size() == longest);
    CHECK(s.text_of("nameCounter") == QString("%1 / %1").arg(longest));
    QQuickItem* caret = s.h.find_item("nameCaret");
    REQUIRE(caret != nullptr);
    CHECK_FALSE(caret->isVisible());
}

TEST_CASE("Return saves a name, and spaces around it are not saved", "[ui][scores][nameentry]")
{
    Shell s(DESKTOP);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    s.type_keys(" ada ");
    QTest::keyClick(&s.h.view(), Qt::Key_Return);
    s.h.settle();
    CHECK(s.screen() == "top10");
    REQUIRE(s.c().top10().size() == 1);
    CHECK(s.c().top10().first().toMap().value("name").toString() == "ADA");
}

TEST_CASE("The on-screen keyboard types into the field and matches the phone board's keyboard area",
          "[ui][scores][nameentry]")
{
    Shell s(PHONE);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    QQuickItem* keyboard = s.h.find_item("nameKeyboard");
    REQUIRE(keyboard != nullptr);
    CHECK(keyboard->isVisible());
    // The board reserves about 336 pixels at the foot of the window for it.
    CHECK(keyboard->height() == 336);
    CHECK(keyboard->mapToScene(QPointF(0, keyboard->height())).y() == PHONE.height());

    // The content sits above it with SAVE and SKIP clear of the keys.
    CHECK(s.rect_of("saveButton").bottom() <= keyboard->y());
    CHECK(s.rect_of("skipButton").bottom() <= keyboard->y());

    for (const char* name : {"keyS", "keyP", "keyA", "spaceKey", "keyR", "keyK", "keyY"})
    {
        INFO(name);
        s.tap(name);
    }
    CHECK(s.text_of("nameText") == "SPA RKY");
    CHECK(s.text_of("nameCounter") == "7 / 12");
    s.tap("deleteKey");
    CHECK(s.text_of("nameText") == "SPA RK");

    // Every key is as tall as a hit target should be, and lies inside the keyboard and the window.
    const QRectF area = keyboard->mapRectToScene(QRectF(0, 0, keyboard->width(), keyboard->height()));
    for (const char* name : {"keyQ", "keyP", "keyA", "keyL", "keyZ", "keyM", "deleteKey", "spaceKey"})
    {
        INFO(name);
        const QRectF r = s.rect_of(name);
        CHECK(r.height() >= 44);
        CHECK(r.left() >= area.left());
        CHECK(r.right() <= area.right());
        CHECK(r.top() >= area.top());
        CHECK(r.bottom() <= area.bottom());
    }
    // A letter key is narrower than a tap target on a phone, but its height and the gap make it reachable: ten fit a
    // row.
    CHECK(s.rect_of("keyP").right() <= PHONE.width());

    s.tap("saveButton");
    CHECK(s.screen() == "top10");
    CHECK(s.c().top10().first().toMap().value("name").toString() == "SPA RK");
}

TEST_CASE("The on-screen keyboard goes grey when the name is as long as it may be", "[ui][scores][nameentry]")
{
    Shell s(PHONE);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    s.type_keys("abcdefghijkl");
    REQUIRE(s.text_of("nameText").size() == s.c().name_max_length());
    CHECK_FALSE(s.h.find_item("keyQ")->isEnabled());
    CHECK_FALSE(s.h.find_item("spaceKey")->isEnabled());
    CHECK(s.h.find_item("deleteKey")->isEnabled());
    s.tap("keyQ"); // ignored
    CHECK(s.text_of("nameText").size() == s.c().name_max_length());
    s.tap("deleteKey");
    CHECK(s.h.find_item("keyQ")->isEnabled());
}

TEST_CASE("The on-screen keyboard shows on small windows, not on a desktop window, and on request",
          "[ui][scores][nameentry]")
{
    {
        Shell s(DESKTOP);
        s.blow_up_after(3);
        REQUIRE(s.screen() == "nameEntry");
        CHECK_FALSE(s.visible("nameKeyboard"));
        // A hardware keyboard is all a desktop window needs.
        s.type_keys("zip");
        CHECK(s.text_of("nameText") == "ZIP");
        s.check_inside({"nameBurst", "entryCard", "nameField", "saveButton", "skipButton"});

        // Asked for, the keyboard appears and the content makes room for it.
        QObject* screen = s.h.find("nameEntryScreen");
        REQUIRE(screen != nullptr);
        screen->setProperty("keyboardMode", "on");
        s.h.settle();
        REQUIRE(s.visible("nameKeyboard"));
        CHECK(s.rect_of("saveButton").bottom() <= s.rect_of("nameKeyboard").top());
        CHECK(s.rect_of("deleteKey").height() >= 44);
        s.tap("deleteKey");
        CHECK(s.text_of("nameText") == "ZI");
        s.check_inside({"nameBurst", "entryCard", "nameField", "saveButton", "skipButton", "deleteKey", "spaceKey"});
        s.check_stacked({"nameBurst", "entryCard", "nameField", "saveButton", "nameKeyboard"});
    }
    {
        Shell s(TABLET);
        s.blow_up_after(3);
        REQUIRE(s.screen() == "nameEntry");
        CHECK_FALSE(s.visible("nameKeyboard"));
        QObject* screen = s.h.find("nameEntryScreen");
        screen->setProperty("keyboardMode", "on");
        s.h.settle();
        REQUIRE(s.visible("nameKeyboard"));
        CHECK(s.rect_of("saveButton").bottom() <= s.rect_of("nameKeyboard").top());
        s.check_inside({"nameBurst", "entryCard", "nameField", "saveButton", "skipButton", "deleteKey", "spaceKey"});
    }
}

TEST_CASE("SAVE tells a screen reader why it is off until there is a name", "[ui][scores][nameentry]")
{
    Shell s(PHONE);
    s.blow_up_after(3);
    REQUIRE(s.screen() == "nameEntry");
    CHECK(property_of(s.h.find("saveButton"), "accessibleName").toString() == "Save, type a name first");
    s.type_keys("kit");
    CHECK(property_of(s.h.find("saveButton"), "accessibleName").toString() == "Save");
    CHECK(property_of(s.h.find("skipButton"), "accessibleName").toString().isEmpty()); // its text is its name
    CHECK(property_of(s.h.find("skipButton"), "text").toString() == "SKIP");
}

// ---- Screenshots for the hand check against the boards
// ----------------------------------------------------------------

TEST_CASE("Screenshots of the result and score screens for the hand check against the boards", "[.][shots]")
{
    const QByteArray dir = qgetenv("HBM_SHOT_DIR");
    REQUIRE_FALSE(dir.isEmpty());
    const QString out = QString::fromUtf8(dir) + QLatin1Char('/');

    for (const SizeCase& size : {SizeCase{"phone", PHONE}, SizeCase{"tablet", TABLET}, SizeCase{"desktop", DESKTOP}})
    {
        const QString prefix = out + size.name + QLatin1Char('-');
        {
            // CP6: level clear, CP8: win.
            Shell s(size.size);
            s.start();
            s.clear_level();
            s.wait_out_holds();
            CHECK(s.h.grab(prefix + "levelclear.png"));
            s.win_the_game();
            CHECK(s.h.grab(prefix + "win.png"));
        }
        {
            // CP11: name entry (the keyboard on, as on a touch device), CP12: top 10 with the new entry, CP7: game
            // over.
            Shell s(size.size, save_text(full_list()));
            s.blow_up_after(3);
            QObject* screen = s.h.find("nameEntryScreen");
            if (screen != nullptr)
            {
                screen->setProperty("keyboardMode", "on");
            }
            s.type_keys("sparky");
            CHECK(s.h.grab(prefix + "nameentry.png"));
            screen->setProperty("keyboardMode", "off");
            s.h.settle();
            CHECK(s.h.grab(prefix + "nameentry-nokeyboard.png"));
            s.tap("saveButton");
            CHECK(s.h.grab(prefix + "top10.png"));
            s.tap("backButton");
            CHECK(s.h.grab(prefix + "gameover.png"));
        }
        {
            // The game-over card for a game ended by QUIT, and the empty top 10.
            Shell s(size.size);
            s.start();
            s.rig.answer_right();
            s.wait_out_holds();
            s.c().pause();
            s.c().quit();
            s.h.settle();
            s.tap("skipButton");
            CHECK(s.h.grab(prefix + "gameover-quit.png"));
            s.tap("top10Button");
            CHECK(s.h.grab(prefix + "top10-empty.png"));
        }
    }
}
