// SPDX-License-Identifier: GPL-3.0-or-later
// The journeys of docs/user-journeys.md, walked step by step through the whole app shell on a controller with recorded
// services and a manual clock. Each test is named after a journey and asserts the screen after every step, and the save
// where the journey says something is written. A second shell on the first one's save file is a restart of the app.
#include "heart_boom_maths/save_codec.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QQuickItem>
#include <QTest>
#include <bit>
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

const QString PRIVACY_URL = QStringLiteral("https://iainchesworthlabs.github.io/heart-boom-maths/privacy-policy/");

/// The text of a save that holds a full top 10, 100 points down to 10.
std::string full_list_save()
{
    SaveData data;
    const char* names[] = {"BOMB SQUAD", "ROCKET", "DAD", "MUM", "SPARKY", "NINJA", "CAT", "ALEX", "SUNNY", "TINY"};
    for (int i = 0; i < 10; ++i)
    {
        data.top10.push_back({names[i], 100 - 10 * i});
    }
    return *serialise(data);
}

/// The whole app shell on a rig. Its save is the rig's store, so a second Journey made from `save()` is the app after a
/// restart.
struct Journey
{
    explicit Journey(QSize size = PHONE, std::string save_text = {}, bool leaderboard = false)
        : rig(SEED, leaderboard, std::move(save_text)), h(&rig.controller, QStringLiteral("AppShell.qml"), size)
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

    [[nodiscard]] QString page() const
    {
        return property_of(h.root(), "page").toString();
    }

    [[nodiscard]] bool visible(const char* name) const
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        return item != nullptr && item->isVisible();
    }

    [[nodiscard]] bool has(const char* name) const
    {
        return h.find(QLatin1String(name)) != nullptr;
    }

    [[nodiscard]] bool enabled(const char* name) const
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        return item != nullptr && item->isEnabled();
    }

    [[nodiscard]] QString text_of(const char* name) const
    {
        return property_of(h.find(QLatin1String(name)), "text").toString();
    }

    void tap(const char* name)
    {
        QQuickItem* item = h.find_item(QLatin1String(name));
        REQUIRE(item != nullptr);
        click(item);
        h.settle();
    }

    [[nodiscard]] QQuickItem* tile(int position) const
    {
        return h.find_item(QStringLiteral("tile%1").arg(position));
    }

    [[nodiscard]] QString look(int position) const
    {
        return property_of(tile(position), "look").toString();
    }

    [[nodiscard]] int right_tile()
    {
        return c().session_view().question.correct_tile;
    }

    /// A tile that is neither the right one nor one a hint took away.
    [[nodiscard]] int wrong_tile()
    {
        const SessionView& v = c().session_view();
        for (int position = 0; position < 4; ++position)
        {
            if (position != v.question.correct_tile && ((v.removed_tiles >> position) & 1) == 0)
            {
                return position;
            }
        }
        FAIL_CHECK("no wrong tile is left");
        return -1;
    }

    void wait_out_holds()
    {
        QTest::qWait(PAST_HOLD_MS);
        h.settle();
    }

    /// A tap on the right tile, then the hold runs out.
    void tap_right()
    {
        click(tile(right_tile()));
        wait_out_holds();
    }

    void key(Qt::Key key_code)
    {
        QTest::keyClick(&h.view(), key_code);
        QCoreApplication::processEvents();
        h.settle();
    }

    void type_name(const char* letters)
    {
        for (const char* letter = letters; *letter != '\0'; ++letter)
        {
            QTest::keyClick(&h.view(), *letter);
        }
        h.settle();
    }

    void keypad(const QString& digits)
    {
        for (const QChar digit : digits)
        {
            tap(QStringLiteral("key%1").arg(digit).toLatin1().constData());
        }
    }

    /// The number the gate now open asks for, and one that is not it.
    [[nodiscard]] QString gate_answer() const
    {
        return QString::number(UiRig::gate_challenge(rig.gate_seeds_used - 1).number);
    }

    [[nodiscard]] QString gate_wrong_answer() const
    {
        const int number = UiRig::gate_challenge(rig.gate_seeds_used - 1).number;
        return QString::number(number == 999 ? 998 : number + 1);
    }

    /// What the save file holds now.
    [[nodiscard]] SaveData on_disk() const
    {
        REQUIRE(rig.store.text().has_value());
        const ParseResult parsed = parse_save(*rig.store.text());
        REQUIRE(parsed.status == LoadStatus::OK);
        return parsed.data;
    }

    [[nodiscard]] std::string save() const
    {
        REQUIRE(rig.store.text().has_value());
        return *rig.store.text();
    }

    void background()
    {
        c().on_application_state(false);
        h.settle();
    }

    void foreground()
    {
        c().on_application_state(true);
        h.settle();
    }

    /// Clears level 12 through the controller, as taps would, and waits for the win screen.
    void win_the_game()
    {
        c().startGame();
        for (int level = 1; level <= 12; ++level)
        {
            rig.play_level_right();
            if (level < 12)
            {
                c().nextLevel();
            }
        }
        wait_out_holds();
    }

    /// A game that ends in the bomb after some right answers, and waits for the screen that follows the boom.
    void blow_up_after(int right_answers)
    {
        c().startGame();
        for (int i = 0; i < right_answers; ++i)
        {
            rig.answer_right();
        }
        rig.play_for_ms(61000);
        wait_out_holds();
    }

    UiRig rig;
    QmlHarness h;
};

} // namespace

// ---- Journey: a new player's first game
// ---------------------------------------------------------------------------------

TEST_CASE("Journey: a new player plays a first game, uses a hint, loses, and is ranked", "[ui][journey]")
{
    Journey j;

    // 1. First launch: no save file yet, the title offers PLAY and nothing else about a saved game.
    REQUIRE(j.screen() == "title");
    CHECK_FALSE(j.rig.store.text().has_value());
    CHECK(j.c().wallet() == 0);
    CHECK(j.c().best_score() == 0);
    CHECK(j.text_of("playButton") == "PLAY");
    CHECK_FALSE(j.visible("continueButton"));
    CHECK(j.text_of("titleCaption") == "Every game starts at level 1");

    // 2. PLAY: the play screen on level 1, and a save file with the defaults exists.
    j.tap("playButton");
    REQUIRE(j.screen() == "play");
    CHECK(j.c().state() == State::Playing);
    CHECK(j.text_of("levelLabel") == "LEVEL 1");
    CHECK(j.text_of("questionCount") == "1 / 10");
    CHECK_FALSE(j.on_disk().run.has_value());

    // 3. No coins, no hint. One coin is not enough either. Two is exactly the price.
    CHECK_FALSE(j.enabled("hintButton"));
    j.tap_right();
    CHECK(j.c().wallet() == 1);
    CHECK_FALSE(j.enabled("hintButton"));
    j.tap_right();
    CHECK(j.c().wallet() == 2);
    REQUIRE(j.enabled("hintButton"));

    // 4. The hint takes two coins and two wrong tiles, and cannot be used twice on one question.
    j.tap("hintButton");
    CHECK(j.c().wallet() == 0);
    CHECK(std::popcount(static_cast<unsigned>(j.c().removed_tiles())) == 2);
    CHECK_FALSE(j.enabled("hintButton"));
    j.tap("hintButton");
    CHECK(j.c().wallet() == 0); // never below nothing
    j.tap_right();
    CHECK(j.c().wallet() == 1);

    // 5. A wrong answer: red tile, the right one shown, and a second tap on the same tile or on another one counts for
    // nothing while the look is held.
    const int wrong = j.wrong_tile();
    const int right = j.right_tile();
    const int answered_before = j.c().question_number();
    click(j.tile(wrong));
    j.h.settle();
    CHECK(j.look(wrong) == "wrong");
    CHECK(j.look(right) == "right");
    click(j.tile(wrong));
    click(j.tile(right));
    j.h.settle();
    CHECK(j.c().question_number() == answered_before + 1);
    j.wait_out_holds();
    CHECK(j.c().multiplier() == 1);

    // 6. The rest of the level, right: the level-clear card follows the last answer, and the wallet is saved there.
    while (j.c().state() == State::Playing)
    {
        j.tap_right();
    }
    REQUIRE(j.c().state() == State::LevelClear);
    CHECK(j.screen() == "levelClear");
    CHECK(j.on_disk().coins == j.c().wallet());

    // 7. NEXT LEVEL: level 2 with a fresh timer.
    j.tap("nextLevelButton");
    REQUIRE(j.screen() == "play");
    CHECK(j.text_of("levelLabel") == "LEVEL 2");
    CHECK(j.c().ticks_left() == j.c().ticks_total());

    // 8. The timer runs out: the heart pops on the play screen, then the name screen, because the score earns a place.
    j.rig.play_for_ms(61000);
    REQUIRE(j.c().state() == State::GameOver);
    CHECK(j.c().end_reason() == GameController::EndReason::Bomb);
    j.wait_out_holds();
    REQUIRE(j.screen() == "nameEntry");
    CHECK(j.c().name_entry_rank() == 1);
    CHECK(j.on_disk().top10.empty()); // nothing is in the list until SAVE
    CHECK(j.on_disk().coins == j.c().wallet());
    CHECK_FALSE(j.enabled("saveButton"));

    // 9. A name, SAVE: the top 10 with the new entry marked, and the list is on disk.
    j.type_name("zoe");
    CHECK(j.text_of("nameText") == "ZOE");
    j.tap("saveButton");
    REQUIRE(j.screen() == "top10");
    CHECK(j.c().highlight_rank() == 1);
    REQUIRE(j.on_disk().top10.size() == 1);
    CHECK(j.on_disk().top10.front().name == "ZOE");
    CHECK(j.on_disk().top10.front().score == j.c().final_score());

    // 10. Back lands on the game-over card, where the score is the best.
    j.tap("backButton");
    REQUIRE(j.screen() == "gameOver");
    CHECK(j.visible("newBestBadge"));

    // 11. PLAY AGAIN starts level 1 at once.
    j.tap("playAgainButton");
    REQUIRE(j.screen() == "play");
    CHECK(j.text_of("levelLabel") == "LEVEL 1");
    CHECK(j.text_of("pointsValue") == "0");
    CHECK(j.c().wallet() == j.on_disk().coins); // the coins stayed
}

// ---- Journey: dying and ranking
// -----------------------------------------------------------------------------------------

TEST_CASE("Journey: a score that does not rank, then one that does, then SKIP and SAVE", "[ui][journey]")
{
    Journey j(PHONE, full_list_save());
    REQUIRE(j.screen() == "title");
    CHECK(j.c().best_score() == 100);

    // 1. A game of nothing: the bomb, then the game-over card at once, because a score of 0 never ranks.
    j.tap("playButton");
    j.rig.play_for_ms(61000);
    j.wait_out_holds();
    REQUIRE(j.screen() == "gameOver");
    CHECK(j.visible("boomBurst"));
    CHECK_FALSE(j.c().name_entry_pending());

    // 2. TOP 10 from the card: the list as it was, nothing marked, PLAY AGAIN at the foot; back to the card.
    j.tap("top10Button");
    REQUIRE(j.screen() == "top10");
    CHECK(j.text_of("top10PlayButton") == "PLAY AGAIN");
    for (int rank = 1; rank <= 10; ++rank)
    {
        CHECK_FALSE(property_of(j.h.find_item(QStringLiteral("scoreRow%1").arg(rank)), "highlighted").toBool());
    }
    j.tap("backButton");
    REQUIRE(j.screen() == "gameOver");

    // 3. HOME: the title, with the game still over underneath. PLAY from there is a new game.
    j.tap("homeButton");
    REQUIRE(j.screen() == "title");
    j.tap("playButton");
    REQUIRE(j.screen() == "play");
    CHECK(j.c().level() == 1);

    // 4. Three right answers and the bomb: 40 points tie the 7th place, and the older entry stays first.
    for (int i = 0; i < 3; ++i)
    {
        j.tap_right();
    }
    j.rig.play_for_ms(61000);
    j.wait_out_holds();
    REQUIRE(j.screen() == "nameEntry");
    CHECK(j.c().final_score() == 40);
    CHECK(j.c().name_entry_rank() == 8);

    // 5. SKIP: the game-over card, not NEW BEST, and the list on disk is as it was.
    j.tap("skipButton");
    REQUIRE(j.screen() == "gameOver");
    CHECK_FALSE(j.visible("newBestBadge"));
    CHECK(j.on_disk().top10.size() == 10);
    CHECK(j.on_disk().top10.back().name == "TINY");

    // 6. PLAY AGAIN and the same again; SAVE this time: the new entry is 8th and the 10th falls off.
    j.tap("playAgainButton");
    for (int i = 0; i < 3; ++i)
    {
        j.tap_right();
    }
    j.rig.play_for_ms(61000);
    j.wait_out_holds();
    REQUIRE(j.screen() == "nameEntry");
    j.type_name("kit");
    j.tap("saveButton");
    REQUIRE(j.screen() == "top10");
    CHECK(property_of(j.h.find_item("scoreRow8"), "highlighted").toBool());
    CHECK(property_of(j.h.find_item("scoreRow8"), "name").toString() == "KIT");
    CHECK(j.has("scoreRow10"));
    const std::vector<ScoreEntry> disk = j.on_disk().top10;
    REQUIRE(disk.size() == 10);
    CHECK(disk[7].name == "KIT");
    CHECK(disk[9].name == "SUNNY"); // the entry at 10 points, TINY, is the one that fell off

    // 7. The list from the title, after the game: back goes to the title, and nothing is marked as new.
    j.tap("backButton");
    j.tap("homeButton");
    REQUIRE(j.screen() == "title");
    j.tap("top10Button");
    REQUIRE(j.screen() == "top10");
    CHECK_FALSE(property_of(j.h.find_item("scoreRow8"), "highlighted").toBool());
    CHECK(j.text_of("top10PlayButton") == "PLAY"); // from the title the button says what the title's own button says
    j.tap("backButton");
    CHECK(j.screen() == "title");
}

// ---- Journey: winning the game and going on
// ----------------------------------------------------------------------------

TEST_CASE("Journey: clearing level 12 pays the bonus once, KEEP GOING into endless, QUIT, and a second win",
          "[ui][journey]")
{
    Journey j(DESKTOP);

    // 1. Level 12 cleared: the win screen with the bonus, saved with the wallet and the once-only mark.
    j.win_the_game();
    REQUIRE(j.screen() == "won");
    CHECK(property_of(j.h.find("bonusCard"), "text").toString() == "+1,000 COINS");
    CHECK(j.c().win_bonus() == 1000);
    CHECK(j.on_disk().won_once);
    CHECK(j.on_disk().coins == j.c().wallet());
    CHECK(j.c().wallet() >= 1000);

    // 2. KEEP GOING: level 13, the first endless level, with its badge.
    j.tap("keepGoingButton");
    REQUIRE(j.screen() == "play");
    CHECK(j.text_of("levelLabel") == "LEVEL 13");
    CHECK(j.visible("endlessBadge"));

    // 3. The pause button, then QUIT from the pause screen: no BOOM, and the name screen for the score.
    j.rig.play_for_ms(2000);
    j.tap("pauseButton");
    REQUIRE(j.visible("pauseOverlay"));
    CHECK(j.on_disk().run.has_value());
    j.tap("quitButton");
    REQUIRE(j.screen() == "nameEntry");
    CHECK(j.c().end_reason() == GameController::EndReason::Quit);
    CHECK_FALSE(j.on_disk().run.has_value()); // an ended game leaves no run to continue
    j.type_name("ace");
    j.tap("saveButton");
    REQUIRE(j.screen() == "top10");
    j.tap("backButton");
    REQUIRE(j.screen() == "gameOver");
    CHECK_FALSE(j.visible("boomBurst"));
    const std::int64_t wallet_after_first_game = j.c().wallet();

    // 4. A second win in the same install pays nothing, and says so.
    j.tap("playAgainButton");
    for (int level = 1; level <= 12; ++level)
    {
        j.rig.play_level_right();
        if (level < 12)
        {
            j.c().nextLevel();
        }
    }
    j.wait_out_holds();
    REQUIRE(j.screen() == "won");
    CHECK(j.c().win_bonus() == 0);
    CHECK(property_of(j.h.find("bonusCard"), "text").toString() == "BONUS WON BEFORE");
    CHECK(j.c().wallet() - wallet_after_first_game < 1000);
    CHECK(j.on_disk().won_once);

    // 5. FINISH ends the game like QUIT: no BOOM, the name screen, SKIP, the card.
    j.tap("finishButton");
    REQUIRE(j.screen() == "nameEntry");
    CHECK(j.c().end_reason() == GameController::EndReason::Finish);
    j.tap("skipButton");
    REQUIRE(j.screen() == "gameOver");
    CHECK_FALSE(j.visible("boomBurst"));

    // 6. After a restart the mark is still there: the third win pays nothing either.
    Journey again(DESKTOP, j.save());
    again.win_the_game();
    REQUIRE(again.screen() == "won");
    CHECK(again.c().win_bonus() == 0);
}

// ---- Journey: pause, leave, be killed, continue
// -------------------------------------------------------------------------

TEST_CASE("Journey: pause, Settings from the pause screen, leave the app, be killed, then CONTINUE", "[ui][journey]")
{
    std::string after_kill;
    int level = 0;
    int points = 0;
    int ticks = 0;
    QStringList parts;
    {
        Journey first;
        first.tap("playButton");
        first.tap_right();
        first.tap_right();
        first.rig.play_for_ms(3000);

        // 1. The pause button: the pause screen over the play screen, the clock stops, the run is on disk at once.
        first.tap("pauseButton");
        REQUIRE(first.c().state() == State::Paused);
        REQUIRE(first.visible("pauseOverlay"));
        CHECK(first.on_disk().run.has_value());
        const int frozen = first.c().ticks_left();
        first.rig.play_for_ms(5000);
        CHECK(first.c().ticks_left() == frozen);

        // 2. Settings from the pause screen: the run stays paused, the page can be left for the pause screen again.
        first.tap("settingsButton");
        CHECK(first.page() == "settings");
        CHECK(first.c().state() == State::Paused);
        first.tap("aboutButton");
        first.tap("backButton");
        first.tap("backButton");
        CHECK(first.page().isEmpty());
        REQUIRE(first.visible("pauseOverlay"));
        CHECK(first.c().state() == State::Paused);

        // 3. RESUME and then the app is switched away from: it pauses at once with the clock stopped.
        first.tap("resumeButton");
        REQUIRE(first.c().state() == State::Playing);
        CHECK_FALSE(first.on_disk().run.has_value()); // spent on resuming
        first.rig.play_for_ms(1000);
        first.background();
        REQUIRE(first.c().state() == State::Paused);
        REQUIRE(first.screen() == "play");
        CHECK(first.visible("pauseOverlay"));
        CHECK(first.rig.audio.suspends == 1);
        CHECK(first.on_disk().run.has_value());
        const int at_leaving = first.c().ticks_left();
        first.rig.play_for_ms(20000);
        CHECK(first.c().ticks_left() == at_leaving);

        // 4. Back in the app: still the pause screen, and still stopped.
        first.foreground();
        CHECK(first.rig.audio.resumes == 1);
        REQUIRE(first.visible("pauseOverlay"));
        first.rig.play_for_ms(2000);
        CHECK(first.c().ticks_left() == at_leaving);

        level = first.c().level();
        points = static_cast<int>(first.c().points());
        ticks = first.c().ticks_left();
        parts = first.c().question_parts();
        after_kill = first.save();
    } // the app is killed here

    // 5. The next launch: the title says what was saved and offers CONTINUE beside NEW GAME.
    Journey second(PHONE, after_kill);
    REQUIRE(second.screen() == "title");
    REQUIRE(second.visible("continueButton"));
    CHECK(second.text_of("playButton") == "NEW GAME");
    CHECK(second.text_of("titleCaption").startsWith("Saved game: level 1"));

    // The Top 10 from here has a button that starts a game too, so it says NEW GAME like the title and not PLAY.
    second.tap("top10Button");
    REQUIRE(second.page() == "top10");
    CHECK(second.text_of("top10PlayButton") == "NEW GAME");
    second.tap("backButton");
    CHECK(second.screen() == "title");
    CHECK(second.visible("continueButton"));

    // 6. CONTINUE: the pause screen with the same game, and the run is deleted from the save as soon as it loads.
    second.tap("continueButton");
    REQUIRE(second.screen() == "play");
    CHECK(second.visible("pauseOverlay"));
    CHECK(second.c().state() == State::Paused);
    CHECK(second.c().level() == level);
    CHECK(static_cast<int>(second.c().points()) == points);
    CHECK(second.c().ticks_left() == ticks);
    CHECK(second.c().question_parts() == parts);
    CHECK_FALSE(second.on_disk().run.has_value());
    second.rig.play_for_ms(5000);
    CHECK(second.c().ticks_left() == ticks);

    // 7. RESUME: the game goes on from where it stopped.
    second.tap("resumeButton");
    REQUIRE(second.c().state() == State::Playing);
    second.rig.play_for_ms(1000);
    CHECK(second.c().ticks_left() == ticks - 20);

    // 8. Leaving again saves a new run, and NEW GAME on the next launch throws it away.
    second.background();
    REQUIRE(second.on_disk().run.has_value());
    Journey third(PHONE, second.save());
    REQUIRE(third.visible("continueButton"));
    third.tap("playButton");
    REQUIRE(third.screen() == "play");
    CHECK(third.c().state() == State::Playing);
    CHECK(third.c().level() == 1);
    CHECK(third.c().question_number() == 1);
    CHECK_FALSE(third.on_disk().run.has_value());
    CHECK_FALSE(third.c().has_saved_run());
}

TEST_CASE("Journey: a saved run that cannot be opened, and a save from a newer version", "[ui][journey]")
{
    SECTION("a run the game cannot restore leaves a calm title and a working PLAY")
    {
        SaveData data;
        data.coins = 7;
        data.run = std::vector<std::int64_t>{1, 2, 3};
        Journey j(PHONE, *serialise(data));
        REQUIRE(j.visible("continueButton"));
        j.tap("continueButton");
        CHECK(j.screen() == "title");
        CHECK_FALSE(j.visible("continueButton"));
        CHECK(j.c().wallet() == 7);
        CHECK_FALSE(j.on_disk().run.has_value());
        j.tap("playButton");
        CHECK(j.screen() == "play");
    }
    SECTION("a save from a newer version is played on defaults and never written over")
    {
        const std::string newer = R"({"version": 99, "coins": 5, "run": [1, 2, 3], "something_new": true})";
        Journey j(PHONE, newer);
        CHECK(j.c().save_blocked());
        CHECK(j.screen() == "title");
        CHECK_FALSE(j.visible("continueButton")); // nothing of the newer file's run is offered
        CHECK(j.c().wallet() == 0);
        j.tap("playButton");
        j.tap_right();
        j.tap("pauseButton");
        j.background();
        j.foreground();
        j.tap("quitButton");
        REQUIRE(j.screen() == "nameEntry");
        j.type_name("amy");
        j.tap("saveButton");
        CHECK(j.screen() == "top10"); // the list works for this session
        CHECK(j.rig.store.text() == newer);
    }
}

// ---- Journey: settings, About and the parental gate
// --------------------------------------------------------------------

TEST_CASE("Journey: Settings switches, the gate for Game Center and for a link, and a restart", "[ui][journey]")
{
    std::string after_restart;
    {
        Journey j(PHONE, {}, true);

        // 1. The gear, then both switches off: each is applied and written at once.
        j.tap("settingsButton");
        REQUIRE(j.page() == "settings");
        CHECK(property_of(j.h.find("soundSwitch"), "checked").toBool());
        j.tap("soundSwitch");
        j.tap("musicSwitch");
        CHECK_FALSE(j.rig.audio.effects_enabled);
        CHECK_FALSE(j.rig.audio.music_enabled);
        CHECK_FALSE(j.on_disk().sound);
        CHECK_FALSE(j.on_disk().music);

        // 2. Game Center, where there is a leaderboard: the gate, a wrong number, CANCEL; then the right number.
        j.tap("gameCenterSwitch");
        REQUIRE(j.c().gate_active());
        CHECK(j.visible("gateScreen"));
        j.keypad(j.gate_wrong_answer());
        CHECK(j.c().gate_active()); // a new number is dealt
        CHECK_FALSE(j.c().game_center_enabled());
        j.tap("cancelButton");
        CHECK_FALSE(j.c().gate_active());
        CHECK_FALSE(j.on_disk().game_center);
        j.tap("gameCenterSwitch");
        j.keypad(j.gate_answer());
        j.tap("openButton");
        CHECK(j.c().game_center_enabled());
        CHECK(j.rig.scores.enabled);
        CHECK(j.on_disk().game_center);
        CHECK(j.page() == "settings");

        // 3. About from Settings, and a link: the gate, the Escape key closes it, then the right number opens it once.
        j.tap("aboutButton");
        REQUIRE(j.page() == "about");
        j.tap("privacyButton");
        REQUIRE(j.c().gate_active());
        j.key(Qt::Key_Escape);
        CHECK_FALSE(j.c().gate_active());
        CHECK(j.page() == "about");
        CHECK(j.rig.links.opened.empty());
        j.tap("privacyButton");
        j.keypad(j.gate_wrong_answer());
        CHECK(j.rig.links.opened.empty());
        j.keypad(j.gate_answer());
        j.tap("openButton");
        REQUIRE(j.rig.links.opened.size() == 1);
        CHECK(QString::fromStdString(j.rig.links.opened.front()) == PRIVACY_URL);
        CHECK(j.page() == "about");

        // 4. A gate left open when the app is switched away from is closed, and its link does not open later.
        j.tap("sourceButton");
        REQUIRE(j.c().gate_active());
        CHECK_FALSE(j.enabled("privacyButton")); // the page under the gate takes no taps
        j.background();
        CHECK_FALSE(j.c().gate_active());
        CHECK_FALSE(j.has("gateScreen"));
        j.foreground();
        CHECK(j.page() == "about");
        CHECK(j.rig.links.opened.size() == 1);

        // 5. Back out: About to Settings to the title.
        j.key(Qt::Key_Escape);
        CHECK(j.page() == "settings");
        j.key(Qt::Key_Escape);
        CHECK(j.page().isEmpty());
        CHECK(j.screen() == "title");
        after_restart = j.save();
    }

    // 6. After a restart the switches are as they were left, and the services were told at once.
    Journey again(PHONE, after_restart, true);
    CHECK_FALSE(again.rig.audio.effects_enabled);
    CHECK_FALSE(again.rig.audio.music_enabled);
    CHECK(again.rig.scores.enabled);
    again.tap("settingsButton");
    CHECK_FALSE(property_of(again.h.find("soundSwitch"), "checked").toBool());
    CHECK_FALSE(property_of(again.h.find("musicSwitch"), "checked").toBool());
    CHECK(property_of(again.h.find("gameCenterSwitch"), "checked").toBool());
}

// ---- The app leaving the foreground in every state
// -------------------------------------------------------------------

namespace
{

/// Everything the app can be showing when it is switched away from.
enum class Where
{
    Title,
    Playing,
    Paused,
    LevelClear,
    Won,
    NameEntry,
    GameOver,
    Settings,
    About,
    GateOpen,
    Top10,
};

const char* where_name(Where where)
{
    switch (where)
    {
    case Where::Title:
        return "title";
    case Where::Playing:
        return "playing";
    case Where::Paused:
        return "paused";
    case Where::LevelClear:
        return "level clear";
    case Where::Won:
        return "won";
    case Where::NameEntry:
        return "name entry";
    case Where::GameOver:
        return "game over";
    case Where::Settings:
        return "settings";
    case Where::About:
        return "about";
    case Where::GateOpen:
        return "gate open";
    case Where::Top10:
        return "top 10";
    }
    return "";
}

void arrive(Journey& j, Where where)
{
    switch (where)
    {
    case Where::Title:
        break;
    case Where::Playing:
        j.c().startGame();
        j.rig.answer_right();
        break;
    case Where::Paused:
        j.c().startGame();
        j.rig.answer_right();
        j.c().pause();
        break;
    case Where::LevelClear:
        j.c().startGame();
        j.rig.play_level_right();
        break;
    case Where::Won:
        j.win_the_game();
        break;
    case Where::NameEntry:
        j.blow_up_after(3);
        break;
    case Where::GameOver:
        j.blow_up_after(0);
        break;
    case Where::Settings:
        j.tap("settingsButton");
        break;
    case Where::About:
        j.tap("settingsButton");
        j.tap("aboutButton");
        break;
    case Where::GateOpen:
        j.tap("settingsButton");
        j.tap("aboutButton");
        j.tap("sourceButton");
        break;
    case Where::Top10:
        j.tap("top10Button");
        break;
    }
    j.wait_out_holds();
}

} // namespace

TEST_CASE("Lifecycle: leaving the app and coming back in every state shows the same screen or the pause screen",
          "[ui][journey][lifecycle]")
{
    struct Case
    {
        Where where;
        QString screen;    // the screen on show afterwards
        QString page;      // the page on top afterwards
        State state;       // the game's state afterwards
        const char* shown; // an item that must be visible afterwards
    };
    const std::vector<Case> cases = {
        {Where::Title, "title", "", State::Idle, "playButton"},
        {Where::Playing, "play", "", State::Paused, "pauseOverlay"},
        {Where::Paused, "play", "", State::Paused, "pauseOverlay"},
        {Where::LevelClear, "levelClear", "", State::LevelClear, "nextLevelButton"},
        {Where::Won, "won", "", State::Won, "keepGoingButton"},
        {Where::NameEntry, "nameEntry", "", State::GameOver, "saveButton"},
        {Where::GameOver, "gameOver", "", State::GameOver, "playAgainButton"},
        {Where::Settings, "title", "settings", State::Idle, "settingsScreen"},
        {Where::About, "title", "about", State::Idle, "aboutScreen"},
        {Where::GateOpen, "title", "about", State::Idle, "aboutScreen"},
        {Where::Top10, "top10", "top10", State::Idle, "top10Screen"},
    };
    for (const Case& test : cases)
    {
        DYNAMIC_SECTION(where_name(test.where))
        {
            Journey j;
            arrive(j, test.where);
            const std::int64_t wallet = j.c().wallet();

            j.background();
            j.rig.play_for_ms(10000); // time passes while the app is away: nothing of it counts
            j.foreground();

            CHECK(j.screen() == test.screen);
            CHECK(j.page() == test.page);
            CHECK(j.c().state() == test.state);
            CHECK(j.visible(test.shown));
            CHECK_FALSE(j.c().gate_active());
            CHECK(j.c().wallet() == wallet);
            CHECK(j.on_disk().coins == wallet);
            if (test.where == Where::Playing || test.where == Where::Paused)
            {
                CHECK(j.on_disk().run.has_value());
            }
            else
            {
                CHECK_FALSE(j.on_disk().run.has_value());
            }
            if (test.where == Where::Won)
            {
                CHECK(j.on_disk().won_once);
            }
        }
    }
}

TEST_CASE("Lifecycle: leaving the app during the hold of an answer strands no screen", "[ui][journey][lifecycle]")
{
    SECTION("a wrong answer, then leaving, then RESUME")
    {
        Journey j;
        j.c().startGame();
        j.h.settle();
        click(j.tile(j.wrong_tile()));
        REQUIRE(j.screen() == "play");
        j.background();
        j.foreground();
        j.wait_out_holds();
        REQUIRE(j.visible("pauseOverlay"));
        j.tap("resumeButton");
        REQUIRE(j.c().state() == State::Playing);
        // The look has gone and the tiles answer again.
        CHECK(j.look(0) == "normal");
        j.tap_right();
        CHECK(j.c().correct_answers() == 1);
    }
    SECTION("the last answer of a level, then leaving: the card follows the hold")
    {
        Journey j;
        j.c().startGame();
        for (int i = 0; i < 9; ++i)
        {
            j.rig.answer_right();
        }
        j.wait_out_holds();
        click(j.tile(j.right_tile()));
        REQUIRE(j.c().state() == State::LevelClear);
        j.background();
        j.foreground();
        j.wait_out_holds();
        CHECK(j.screen() == "levelClear");
        j.tap("nextLevelButton");
        CHECK(j.screen() == "play");
    }
    SECTION("the bomb, then leaving: the card follows the hold")
    {
        Journey j;
        j.c().startGame();
        j.rig.play_for_ms(61000);
        REQUIRE(j.c().state() == State::GameOver);
        j.background();
        j.foreground();
        j.wait_out_holds();
        CHECK(j.screen() == "gameOver");
    }
    SECTION("QUIT during a wrong answer's hold goes straight to the end screen")
    {
        Journey j;
        j.c().startGame();
        j.h.settle();
        QObject* feedback = j.h.find("answerFeedback");
        feedback->setProperty("wrongHoldMs", 60000);
        click(j.tile(j.wrong_tile()));
        j.tap("pauseButton");
        REQUIRE(j.visible("pauseOverlay"));
        j.tap("quitButton");
        CHECK(j.screen() == "gameOver"); // a score of nothing: no name screen, and no wait for the held look
        CHECK_FALSE(j.has("playScreen"));
    }
}

TEST_CASE("Lifecycle: the ticking and the clock stop with the pause and with leaving the app",
          "[ui][journey][lifecycle]")
{
    Journey j;
    j.c().startGame();
    j.rig.play_for_ms(51000);
    REQUIRE(j.c().alert());
    CHECK(j.rig.audio.count(SoundId::TICKING) == 1);

    // The pause button: the ticking stops, and RESUME starts it again.
    j.tap("pauseButton");
    CHECK(j.rig.audio.stopped.size() == 1);
    j.tap("resumeButton");
    CHECK(j.rig.audio.count(SoundId::TICKING) == 2);

    // Leaving the app: the ticking stops and the audio service is told to be quiet.
    j.background();
    CHECK(j.rig.audio.stopped.size() == 2);
    CHECK(j.rig.audio.suspends == 1);
    const int left = j.c().ticks_left();
    j.rig.play_for_ms(30000);
    CHECK(j.c().ticks_left() == left);

    // Back in the app the game is still paused, so the ticking has not come back.
    j.foreground();
    CHECK(j.rig.audio.resumes == 1);
    CHECK(j.rig.audio.count(SoundId::TICKING) == 2);
    j.tap("resumeButton");
    CHECK(j.rig.audio.count(SoundId::TICKING) == 3);
}

// ---- Keys: nothing traps the player --------------------------------------------------------------------------------

TEST_CASE("Keys: the Android back key and Escape have a way back on every screen", "[ui][journey][keys]")
{
    Journey j(PHONE, full_list_save());

    // Title: the back key is the system's (it closes the app); Escape does nothing.
    j.key(Qt::Key_Escape);
    CHECK(j.screen() == "title");

    // Top 10 from the title.
    j.tap("top10Button");
    REQUIRE(j.page() == "top10");
    j.key(Qt::Key_Back);
    CHECK(j.screen() == "title");
    j.tap("top10Button");
    j.key(Qt::Key_Escape);
    CHECK(j.screen() == "title");

    // Settings and About from the title, and the gate over About.
    j.tap("settingsButton");
    j.tap("aboutButton");
    j.tap("sourceButton");
    REQUIRE(j.c().gate_active());
    j.key(Qt::Key_Back);
    CHECK_FALSE(j.c().gate_active());
    CHECK(j.page() == "about");
    // The licence text over About: back closes the text and not About.
    j.tap("licenceButton");
    REQUIRE(j.visible("textViewer"));
    j.key(Qt::Key_Back);
    CHECK_FALSE(j.visible("textViewer"));
    CHECK(j.page() == "about");
    j.key(Qt::Key_Back);
    CHECK(j.page() == "settings");
    j.key(Qt::Key_Back);
    CHECK(j.page().isEmpty());
    CHECK(j.screen() == "title");

    // Play: back pauses, back again resumes.
    j.tap("playButton");
    REQUIRE(j.c().state() == State::Playing);
    j.key(Qt::Key_Back);
    CHECK(j.c().state() == State::Paused);
    CHECK(j.visible("pauseOverlay"));
    j.key(Qt::Key_Back);
    CHECK(j.c().state() == State::Playing);
    j.key(Qt::Key_Escape);
    CHECK(j.c().state() == State::Paused);
    j.key(Qt::Key_Escape);
    CHECK(j.c().state() == State::Playing);

    // Settings from the pause screen: back leaves it for the pause screen, with the game still paused.
    j.tap("pauseButton");
    j.tap("settingsButton");
    REQUIRE(j.page() == "settings");
    j.key(Qt::Key_Back);
    CHECK(j.page().isEmpty());
    CHECK(j.c().state() == State::Paused);
    CHECK(j.visible("pauseOverlay"));
    j.key(Qt::Key_Back);
    REQUIRE(j.c().state() == State::Playing);

    // Level clear: a stray back key neither closes the app nor skips the card.
    for (int i = 0; i < 10; ++i)
    {
        j.rig.answer_right();
    }
    j.wait_out_holds();
    REQUIRE(j.screen() == "levelClear");
    j.key(Qt::Key_Back);
    CHECK(j.screen() == "levelClear");
    j.tap("nextLevelButton");

    // Name entry: the back key keeps the score where it is.
    j.rig.play_for_ms(61000);
    j.wait_out_holds();
    REQUIRE(j.screen() == "nameEntry");
    j.key(Qt::Key_Back);
    CHECK(j.screen() == "nameEntry");
    j.tap("skipButton");

    // Game over: back goes home, and the title is the way on from there.
    REQUIRE(j.screen() == "gameOver");
    j.key(Qt::Key_Back);
    CHECK(j.screen() == "title");
    j.tap("playButton");
    CHECK(j.screen() == "play");
}

TEST_CASE("Keys: the back key on the win screen does nothing, and the top 10 from game over goes back to the card",
          "[ui][journey][keys]")
{
    Journey j;
    j.win_the_game();
    REQUIRE(j.screen() == "won");
    j.key(Qt::Key_Back);
    j.key(Qt::Key_Escape);
    CHECK(j.screen() == "won");
    j.tap("finishButton");
    j.tap("skipButton");
    REQUIRE(j.screen() == "gameOver");
    j.tap("top10Button");
    REQUIRE(j.screen() == "top10");
    j.key(Qt::Key_Back);
    CHECK(j.screen() == "gameOver");
}

// ---- Resize and rotation
// ----------------------------------------------------------------------------------------------

TEST_CASE("Lifecycle: changing the window's shape in every state leaves one screen and no page behind",
          "[ui][journey][resize]")
{
    Journey j;
    const auto reshape = [&](const char* expected_screen, const char* only_object)
    {
        for (const QSize size : {DESKTOP, TABLET, PHONE})
        {
            j.h.resize(size);
            CAPTURE(size.width());
            CHECK(j.screen() == expected_screen);
            CHECK(j.has(only_object));
            // One screen at a time: the others are not kept around.
            for (const char* other :
                 {"playScreen", "levelClearScreen", "winScreen", "nameEntryScreen", "gameOverScreen", "titleScreen"})
            {
                if (std::string(other) != only_object)
                {
                    CHECK_FALSE(j.has(other));
                }
            }
        }
    };

    reshape("title", "titleScreen");
    j.c().startGame();
    j.h.settle();
    reshape("play", "playScreen");
    j.c().pause();
    j.h.settle();
    CHECK(j.visible("pauseOverlay"));
    j.tap("settingsButton");
    for (const QSize size : {DESKTOP, TABLET, PHONE})
    {
        j.h.resize(size);
        CHECK(j.page() == "settings");
        CHECK(j.has("settingsScreen"));
    }
    j.key(Qt::Key_Escape);
    CHECK(j.visible("pauseOverlay"));
    j.c().resume();
    j.h.settle();
    j.rig.play_level_right();
    j.wait_out_holds();
    reshape("levelClear", "levelClearScreen");
    j.c().nextLevel();
    j.rig.play_for_ms(61000);
    j.wait_out_holds();
    reshape("nameEntry", "nameEntryScreen");
    j.tap("skipButton");
    reshape("gameOver", "gameOverScreen");
    j.tap("top10Button");
    for (const QSize size : {DESKTOP, TABLET, PHONE})
    {
        j.h.resize(size);
        CHECK(j.screen() == "top10");
        CHECK(j.has("top10Screen"));
    }
}
