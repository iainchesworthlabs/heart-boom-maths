// SPDX-License-Identifier: GPL-3.0-or-later
// Title, Settings, About and the parental gate, through the whole app shell on a controller with recorded services.
#include "heart_boom_maths/save_codec.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QByteArray>
#include <QDate>
#include <QLocale>
#include <QQuickItem>
#include <QTest>
#include <algorithm>
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

const QString PRIVACY_URL = QStringLiteral("https://iainchesworthlabs.github.io/heart-boom-maths/privacy-policy/");
const QString REPOSITORY_URL = QStringLiteral("https://github.com/iainchesworthlabs/heart-boom-maths");

/// A save with a best score of 1,640 and 61 coins, as the Title board shows.
std::string board_save()
{
    SaveData data;
    data.coins = 61;
    data.top10.push_back(ScoreEntry{"BOMB SQUAD", 1640});
    return *serialise(data);
}

/// The text of a save file that holds a run paused in the middle of level 1, from a game played on a first rig.
std::string saved_run_text(int right_answers = 2, int play_ms = 4000)
{
    UiRig first;
    first.controller.startGame();
    for (int i = 0; i < right_answers; ++i)
    {
        first.answer_right();
    }
    first.play_for_ms(play_ms);
    first.controller.pause();
    REQUIRE(first.store.text().has_value());
    return *first.store.text();
}

/// A save whose run is not a state the core would restore.
std::string broken_run_text()
{
    SaveData data;
    data.coins = 7;
    data.run = std::vector<std::int64_t>{1, 2, 3};
    return *serialise(data);
}

/// The app shell on a rig, with the object tree read by name.
struct Screens
{
    explicit Screens(QSize size, bool scores_available = false, std::string saved_text = {})
        : rig(20261114, scores_available, std::move(saved_text)),
          h(&rig.controller, QStringLiteral("AppShell.qml"), size), window_size(size)
    {
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

    void key(Qt::Key key_code)
    {
        QTest::keyClick(&h.view(), key_code);
        QCoreApplication::processEvents();
        h.settle();
    }

    /// Types digits with the on-screen keypad.
    void keypad(const QString& digits)
    {
        for (const QChar digit : digits)
        {
            tap(QStringLiteral("key%1").arg(digit).toLatin1().constData());
        }
    }

    /// The right number for the n-th challenge the rig deals.
    [[nodiscard]] static QString answer(int nth)
    {
        return QString::number(UiRig::gate_challenge(nth).number);
    }

    /// A number of three digits that is not the answer.
    [[nodiscard]] static QString wrong_answer(int nth)
    {
        const int number = UiRig::gate_challenge(nth).number;
        return QString::number(number == 999 ? 998 : number + 1);
    }

    /// True when the item lies inside the window.
    [[nodiscard]] bool inside(const QQuickItem* it) const
    {
        const QPointF top_left = it->mapToScene(QPointF(0, 0));
        const QPointF bottom_right = it->mapToScene(QPointF(it->width(), it->height()));
        return top_left.x() >= -0.5 && top_left.y() >= -0.5 && bottom_right.x() <= window_size.width() + 0.5 &&
               bottom_right.y() <= window_size.height() + 0.5;
    }

    [[nodiscard]] QQuickItem* gate_open_button() const
    {
        return h.find_item(QStringLiteral("openButton"));
    }

    [[nodiscard]] bool gate_open_enabled() const
    {
        QQuickItem* button = gate_open_button();
        return button != nullptr && button->isEnabled();
    }

    UiRig rig;
    QmlHarness h;
    QSize window_size;
};

QString time_text(int seconds)
{
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

TEST_CASE("The title screen shows the best score, the coins, PLAY, TOP 10 and the gear at every size", "[ui][title]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
    };
    const Arrangement arrangement =
        GENERATE(Arrangement{"phone", PHONE}, Arrangement{"tablet", TABLET}, Arrangement{"desktop", DESKTOP});
    CAPTURE(arrangement.name);
    Screens s(arrangement.size, false, board_save());
    REQUIRE(s.rig.controller.state() == State::Idle);

    CHECK(s.visible("titleScreen"));
    CHECK(s.text_of("bestScoreValue") == "1,640");
    CHECK(property_of(s.h.find("walletPill"), "value").toInt() == 61);
    CHECK(s.text_of("tagline") == "10 sums. 60 seconds. Beat the bomb!");
    CHECK(s.text_of("titleCaption") == "Every game starts at level 1");
    CHECK(s.text_of("titleLineOne") == "HEART BOOM");
    CHECK(s.text_of("titleLineTwo") == "MATHS");

    REQUIRE(s.visible("playButton"));
    CHECK(property_of(s.h.find("playButton"), "text").toString() == "PLAY");
    CHECK_FALSE(s.visible("continueButton"));
    CHECK(s.visible("top10Button"));
    CHECK(s.visible("settingsButton"));

    // The heart is the calm one, a picture and not a timer, with a face.
    QObject* heart = s.h.find("titleHeart");
    REQUIRE(heart != nullptr);
    CHECK(property_of(heart, "stateName").toString() == "calm");
    CHECK_FALSE(property_of(heart, "showSeconds").toBool());
    CHECK(property_of(heart, "face").toBool());
    CHECK(s.visible("heroBurst"));

    // Real hit targets, and nothing sticks out of the window.
    for (const char* name : {"playButton", "top10Button", "settingsButton"})
    {
        INFO(name);
        CHECK(s.item(name)->width() >= 44);
        CHECK(s.item(name)->height() >= 44);
        CHECK(s.inside(s.item(name)));
    }
    for (const char* name : {"bestScore", "walletPill", "hero", "tagline", "titleCaption"})
    {
        INFO(name);
        CHECK(s.inside(s.item(name)));
    }

    // Top to bottom: the pills, the burst, the tagline, the buttons, the caption. Nothing overlaps.
    const auto top_of = [&](const char* name) { return s.item(name)->mapToScene(QPointF(0, 0)).y(); };
    const auto bottom_of = [&](const char* name)
    { return s.item(name)->mapToScene(QPointF(0, s.item(name)->height())).y(); };
    CHECK(bottom_of("bestScore") <= top_of("hero") + 1);
    CHECK(bottom_of("hero") <= top_of("tagline"));
    CHECK(bottom_of("tagline") <= top_of("playButton"));
    CHECK(bottom_of("playButton") <= top_of("top10Button"));
    CHECK(bottom_of("top10Button") <= top_of("titleCaption"));
}

TEST_CASE("The title screen in a window of the phone's shape matches the board's measurements", "[ui][title]")
{
    Screens s(PHONE, false, board_save());
    // The boards are drawn at 390 by 844 and the phone window is the same, so the unit is 1.
    CHECK(property_of(s.h.find("titleScreen"), "unit").toDouble() == 1.0);
    CHECK(s.item("playButton")->height() == 84);
    CHECK(s.item("top10Button")->height() == 64);
    CHECK(s.item("settingsButton")->width() == 64);
    CHECK(s.item("hero")->width() == 342);
    CHECK(s.item("hero")->height() == 262);
}

TEST_CASE("With a saved run the title offers CONTINUE and NEW GAME and says what was saved", "[ui][title][saved]")
{
    Screens s(PHONE, false, saved_run_text());
    REQUIRE(s.rig.controller.has_saved_run());
    const QVariantMap saved = s.rig.controller.saved_run();
    REQUIRE(saved.value(QStringLiteral("level")).toInt() == 1);

    REQUIRE(s.visible("continueButton"));
    CHECK(property_of(s.h.find("continueButton"), "text").toString() == "CONTINUE");
    CHECK(property_of(s.h.find("playButton"), "text").toString() == "NEW GAME");
    CHECK(s.text_of("titleCaption") == QStringLiteral("Saved game: level 1, %1 left")
                                           .arg(time_text(saved.value(QStringLiteral("secondsLeft")).toInt())));
    // The two buttons share the row.
    CHECK(s.item("continueButton")->y() == s.item("playButton")->y());
    CHECK(s.item("continueButton")->width() == s.item("playButton")->width());
    CHECK(s.inside(s.item("continueButton")));
    CHECK(s.inside(s.item("playButton")));

    SECTION("CONTINUE goes to the pause screen with the saved run")
    {
        const int points = static_cast<int>(s.rig.controller.session_view().points);
        (void)points;
        s.tap("continueButton");
        CHECK(s.rig.controller.state() == State::Paused);
        CHECK(s.rig.controller.level() == 1);
        CHECK(s.visible("pauseOverlay"));
        CHECK_FALSE(s.rig.controller.has_saved_run());
        CHECK(s.h.find("titleScreen") == nullptr);
    }
    SECTION("NEW GAME starts level 1 and drops the saved run")
    {
        s.tap("playButton");
        CHECK(s.rig.controller.state() == State::Playing);
        CHECK(s.rig.controller.question_number() == 1);
        CHECK_FALSE(s.rig.controller.has_saved_run());
        CHECK(s.visible("playScreen"));
    }
}

TEST_CASE("A CONTINUE that fails stays on the title with a calm line and no caption", "[ui][title][saved]")
{
    Screens s(PHONE, false, broken_run_text());
    REQUIRE(s.rig.controller.has_saved_run());
    REQUIRE(s.visible("continueButton"));
    // The run is not one the core restores, so there is nothing true to say about it.
    CHECK(s.text_of("titleCaption") == "Every game starts at level 1");

    s.tap("continueButton");

    CHECK(s.rig.controller.state() == State::Idle);
    CHECK(s.visible("titleScreen"));
    CHECK(s.h.find("playScreen") == nullptr);
    CHECK_FALSE(s.rig.controller.has_saved_run());
    CHECK_FALSE(s.visible("continueButton"));
    CHECK(property_of(s.h.find("playButton"), "text").toString() == "PLAY");
    CHECK(s.text_of("titleCaption") == "That saved game could not be opened. You can start a new one.");
    CHECK(s.inside(s.item("titleCaption")));
    // Nothing started by itself, and the wallet from the same file is still there.
    CHECK(s.rig.controller.wallet() == 7);

    s.tap("playButton");
    CHECK(s.rig.controller.state() == State::Playing);
}

TEST_CASE("TOP 10 and the gear open their pages and the back button returns to the title", "[ui][title][nav]")
{
    Screens s(PHONE, false, board_save());

    s.tap("top10Button");
    CHECK(s.visible("top10Screen"));
    CHECK_FALSE(s.visible("titleScreen"));
    s.tap("backButton");
    CHECK(s.h.find("top10Screen") == nullptr);
    CHECK(s.visible("titleScreen"));

    s.tap("settingsButton");
    CHECK(s.visible("settingsScreen"));
    CHECK_FALSE(s.visible("titleScreen"));
    s.tap("backButton");
    CHECK(s.visible("titleScreen"));
    CHECK(s.visible("playButton"));
}

TEST_CASE("Settings shows the switches with ON and OFF in words and writes them through the controller",
          "[ui][settings]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
    };
    const Arrangement arrangement =
        GENERATE(Arrangement{"phone", PHONE}, Arrangement{"tablet", TABLET}, Arrangement{"desktop", DESKTOP});
    CAPTURE(arrangement.name);
    Screens s(arrangement.size);
    s.tap("settingsButton");
    REQUIRE(s.visible("settingsScreen"));

    CHECK(s.text_of("screenTitle") == "SETTINGS");
    REQUIRE(s.visible("soundSwitch"));
    REQUIRE(s.visible("musicSwitch"));
    // No leaderboard on this device: no Game Center row.
    CHECK_FALSE(s.visible("gameCenterRow"));
    CHECK_FALSE(s.visible("gameCenterSwitch"));

    // Both are on to begin with. The word, the knob's side and the colour agree, so colour is not the only cue.
    QObject* sound = s.h.find("soundSwitch");
    CHECK(property_of(sound, "checked").toBool());
    const auto label_of = [&](const char* name)
    {
        QQuickItem* parent_item = s.item(name);
        for (QQuickItem* child : parent_item->findChildren<QQuickItem*>())
        {
            if (child->objectName() == "switchLabel")
            {
                return property_of(child, "text").toString();
            }
        }
        return QString();
    };
    const auto knob_right = [&](const char* name)
    {
        QQuickItem* switch_item = s.item(name);
        for (QQuickItem* child : switch_item->findChildren<QQuickItem*>())
        {
            if (child->objectName() == "switchKnob")
            {
                return child->mapToItem(switch_item, QPointF(0, 0)).x() > switch_item->width() / 2;
            }
        }
        return false;
    };
    CHECK(label_of("soundSwitch") == "ON");
    CHECK(knob_right("soundSwitch"));
    CHECK(label_of("musicSwitch") == "ON");

    s.tap("soundSwitch");
    CHECK_FALSE(s.rig.controller.sound_enabled());
    CHECK_FALSE(s.rig.audio.effects_enabled);
    CHECK(s.rig.audio.music_enabled);
    CHECK(label_of("soundSwitch") == "OFF");
    CHECK_FALSE(knob_right("soundSwitch"));
    CHECK_FALSE(property_of(s.h.find("soundSwitch"), "checked").toBool());

    s.tap("musicSwitch");
    CHECK_FALSE(s.rig.controller.music_enabled());
    CHECK_FALSE(s.rig.audio.music_enabled);
    CHECK(label_of("musicSwitch") == "OFF");

    // Saved at once.
    REQUIRE(s.rig.store.text().has_value());
    const ParseResult saved = parse_save(*s.rig.store.text());
    REQUIRE(saved.status == LoadStatus::OK);
    CHECK_FALSE(saved.data.sound);
    CHECK_FALSE(saved.data.music);

    s.tap("soundSwitch");
    CHECK(s.rig.controller.sound_enabled());
    CHECK(label_of("soundSwitch") == "ON");

    // The footer is the build line from the controller, and the rows are inside the window.
    CHECK(s.text_of("buildFooter") == s.rig.controller.version_text());
    CHECK(s.text_of("buildFooter").startsWith("Heart Boom Maths "));
    CHECK(s.text_of("buildFooter").contains("(build "));
    for (const char* name : {"soundRow", "musicRow", "aboutButton", "buildFooter", "backButton"})
    {
        INFO(name);
        CHECK(s.inside(s.item(name)));
    }
    CHECK(s.item("soundSwitch")->width() >= 44);
    CHECK(s.item("soundSwitch")->height() >= 44);
    CHECK(s.item("aboutButton")->height() >= 44);
    CHECK(s.item("backButton")->width() >= 44);
}

TEST_CASE("A switch answers the keyboard and tells a screen reader what it is", "[ui][settings]")
{
    Screens s(PHONE);
    s.tap("settingsButton");
    QQuickItem* sound = s.item("soundSwitch");
    sound->forceActiveFocus();
    REQUIRE(sound->hasActiveFocus());
    s.key(Qt::Key_Space);
    CHECK_FALSE(s.rig.controller.sound_enabled());
    s.key(Qt::Key_Return);
    CHECK(s.rig.controller.sound_enabled());
    CHECK(property_of(sound, "accessibleName").toString() == "Sound effects");
}

TEST_CASE("Game Center shows only where there is a leaderboard and turns on only behind the gate",
          "[ui][settings][gate]")
{
    Screens s(PHONE, true);
    s.tap("settingsButton");
    REQUIRE(s.visible("gameCenterRow"));
    REQUIRE(s.visible("gameCenterSwitch"));
    CHECK_FALSE(property_of(s.h.find("gameCenterSwitch"), "checked").toBool());
    CHECK(s.inside(s.item("gameCenterRow")));

    // Turning it on asks a grown-up first, and it stays off until the number is typed.
    s.tap("gameCenterSwitch");
    REQUIRE(s.rig.controller.gate_active());
    CHECK(s.rig.controller.gate_purpose() == GameController::GatePurpose::GameCenter);
    CHECK(s.visible("gateScreen"));
    CHECK_FALSE(s.rig.controller.game_center_enabled());
    CHECK_FALSE(s.rig.scores.enabled);
    CHECK(s.text_of("gatePromptText") == "To turn on Game Center, type this number:");
    CHECK(property_of(s.h.find("openButton"), "text").toString() == "TURN ON");
    CHECK_FALSE(s.gate_open_enabled());

    s.keypad(Screens::answer(0));
    CHECK(s.gate_open_enabled());
    CHECK(s.rig.controller.gate_active()); // typing the right number is not yet a yes
    s.tap("openButton");

    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK(s.h.find("gateScreen") == nullptr);
    CHECK(s.rig.controller.game_center_enabled());
    CHECK(s.rig.scores.enabled);
    CHECK(s.visible("settingsScreen"));
    CHECK(property_of(s.h.find("gameCenterSwitch"), "checked").toBool());
    CHECK(s.rig.links.opened.empty()); // a switch is not a link

    // Turning it off is immediate and asks nothing.
    s.tap("gameCenterSwitch");
    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK_FALSE(s.rig.controller.game_center_enabled());
    CHECK_FALSE(property_of(s.h.find("gameCenterSwitch"), "checked").toBool());
}

TEST_CASE("Cancelling the gate for Game Center leaves it off", "[ui][settings][gate]")
{
    Screens s(PHONE, true);
    s.tap("settingsButton");
    s.tap("gameCenterSwitch");
    REQUIRE(s.visible("gateScreen"));
    s.tap("cancelButton");
    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK(s.h.find("gateScreen") == nullptr);
    CHECK_FALSE(s.rig.controller.game_center_enabled());
    CHECK(s.visible("settingsScreen"));
}

TEST_CASE("ABOUT opens from Settings and goes back to it", "[ui][settings][about][nav]")
{
    Screens s(PHONE);
    s.tap("settingsButton");
    s.tap("aboutButton");
    CHECK(s.visible("aboutScreen"));
    CHECK_FALSE(s.visible("settingsScreen"));
    CHECK(s.h.find("settingsScreen") == nullptr);

    s.tap("backButton");
    CHECK(s.visible("settingsScreen"));
    CHECK(s.h.find("aboutScreen") == nullptr);

    s.tap("aboutButton");
    s.key(Qt::Key_Escape);
    CHECK(s.visible("settingsScreen"));
    s.key(Qt::Key_Escape);
    CHECK(s.visible("titleScreen"));
    CHECK(s.h.find("settingsScreen") == nullptr);
}

TEST_CASE("About shows the version, the build, the font licences and four rows", "[ui][about]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
    };
    const Arrangement arrangement =
        GENERATE(Arrangement{"phone", PHONE}, Arrangement{"tablet", TABLET}, Arrangement{"desktop", DESKTOP});
    CAPTURE(arrangement.name);
    Screens s(arrangement.size);
    s.tap("settingsButton");
    s.tap("aboutButton");
    REQUIRE(s.visible("aboutScreen"));

    const QVariantMap info = s.rig.controller.build_info();
    CHECK(s.text_of("screenTitle") == "ABOUT");
    CHECK(s.text_of("aboutName") == info.value(QStringLiteral("appName")).toString().toUpper());
    CHECK(s.text_of("aboutVersion") == QStringLiteral("Version %1 · build %2")
                                           .arg(info.value(QStringLiteral("version")).toString())
                                           .arg(info.value(QStringLiteral("buildNumber")).toInt()));
    CHECK(s.text_of("revisionValue") == info.value(QStringLiteral("commit")).toString());
    CHECK(s.text_of("branchValue") == info.value(QStringLiteral("branch")).toString());
    // A CI build carries a real date, which About writes as "14 Nov 2026"; a local build says "dev" and keeps it.
    const QString date = info.value(QStringLiteral("date")).toString();
    const QDate parsed_date = QDate::fromString(date, Qt::ISODate);
    CHECK(s.text_of("builtValue") == (parsed_date.isValid()
                                          ? QStringLiteral("%1 %2 %3")
                                                .arg(parsed_date.day())
                                                .arg(QLocale::c().monthName(parsed_date.month(), QLocale::ShortFormat))
                                                .arg(parsed_date.year())
                                          : date));
    CHECK(s.visible("aboutHeart"));
    CHECK_FALSE(property_of(s.h.find("aboutHeart"), "showSeconds").toBool());
    CHECK(s.visible("fontLicences"));

    for (const char* name : {"licenceButton", "noticesButton", "privacyButton", "sourceButton"})
    {
        INFO(name);
        REQUIRE(s.visible(name));
        CHECK(s.item(name)->height() >= 44);
        CHECK(s.item(name)->width() > 200);
    }
    // The two that lead out say so on the button.
    CHECK(property_of(s.h.find("privacyButton"), "hint").toString() == "asks a grown-up first");
    CHECK(property_of(s.h.find("sourceButton"), "hint").toString() == "asks a grown-up first");
    CHECK(property_of(s.h.find("privacyButton"), "accessibleName").toString() ==
          "Privacy policy, asks a grown-up first");
    CHECK(property_of(s.h.find("licenceButton"), "hint").toString().isEmpty());

    // On a window tall enough for all of it nothing needs scrolling; on any window the page scrolls instead of cutting
    // off.
    QQuickItem* flick = s.item("aboutFlick");
    CHECK(flick->width() > 0);
    CHECK(s.inside(flick));
    if (arrangement.size == PHONE)
    {
        CHECK_FALSE(property_of(flick, "interactive").toBool());
    }
}

TEST_CASE("The build date reads as day, month name and year", "[ui][about]")
{
    auto h = QmlHarness::from_source(R"(
import QtQuick
import HeartBoomMaths
Item {
    property string iso: Theme.formatBuildDate("2026-11-14")
    property string single: Theme.formatBuildDate("2027-01-05")
    property string local: Theme.formatBuildDate("dev")
    property string odd: Theme.formatBuildDate("2026-13-01")
}
)",
                                     QSize(100, 100));
    QQuickItem* root = h.root();
    CHECK(property_of(root, "iso").toString() == "14 Nov 2026");
    CHECK(property_of(root, "single").toString() == "5 Jan 2027");
    CHECK(property_of(root, "local").toString() == "dev");
    CHECK(property_of(root, "odd").toString() == "2026-13-01");
}

TEST_CASE("Licence and Third-party notices open a scrolling text over About and go back", "[ui][about][viewer]")
{
    Screens s(PHONE);
    s.tap("settingsButton");
    s.tap("aboutButton");
    REQUIRE_FALSE(s.visible("textViewer"));

    SECTION("Licence")
    {
        s.tap("licenceButton");
        REQUIRE(s.visible("textViewer"));
        CHECK(property_of(s.item("textViewer")->findChild<QObject*>("screenTitle"), "text").toString() == "LICENCE");
        CHECK(s.text_of("viewerText").contains("GNU General Public License"));
        CHECK(s.inside(s.item("viewerCard")));
        s.tap("backButton");
        CHECK_FALSE(s.visible("textViewer"));
        CHECK(s.visible("aboutScreen"));
    }

    SECTION("Third-party notices are the controller's notices text, and scroll")
    {
        const QString notices = s.rig.controller.notices();
        REQUIRE_FALSE(notices.isEmpty());
        s.tap("noticesButton");
        REQUIRE(s.visible("textViewer"));
        CHECK(s.text_of("viewerText") == notices);

        QQuickItem* flick = s.item("viewerFlick");
        REQUIRE(property_of(flick, "contentHeight").toDouble() > flick->height()); // longer than the card
        CHECK(s.visible("viewerBar"));
        CHECK(property_of(flick, "contentY").toDouble() == 0);
        flick->forceActiveFocus();
        s.key(Qt::Key_PageDown);
        const double after_page = property_of(flick, "contentY").toDouble();
        CHECK(after_page > 0);
        s.key(Qt::Key_Down);
        CHECK(property_of(flick, "contentY").toDouble() > after_page);
        s.key(Qt::Key_End);
        CHECK(property_of(flick, "contentY").toDouble() ==
              property_of(flick, "contentHeight").toDouble() - flick->height());
        s.key(Qt::Key_Home);
        CHECK(property_of(flick, "contentY").toDouble() == 0);

        // The Escape key closes the viewer first, then About.
        s.key(Qt::Key_Escape);
        CHECK_FALSE(s.visible("textViewer"));
        CHECK(s.visible("aboutScreen"));
    }
}

TEST_CASE("Opening a link never reaches the link opener until the gate is passed, and then exactly once",
          "[ui][about][gate]")
{
    struct Link
    {
        const char* button;
        QString url;
        const char* prompt;
    };
    const Link link = GENERATE(Link{"privacyButton", PRIVACY_URL, "To open the privacy policy, type this number:"},
                               Link{"sourceButton", REPOSITORY_URL, "To open the source code, type this number:"});
    CAPTURE(link.button);
    Screens s(PHONE);
    s.tap("settingsButton");
    s.tap("aboutButton");

    s.tap(link.button);
    REQUIRE(s.rig.controller.gate_active());
    CHECK(s.rig.controller.gate_purpose() == GameController::GatePurpose::Link);
    CHECK(s.rig.controller.gate_url() == link.url);
    CHECK(s.visible("gateScreen"));
    CHECK(s.text_of("gatePromptText") == link.prompt);
    CHECK(s.rig.links.opened.empty());

    SECTION("CANCEL opens nothing")
    {
        s.tap("cancelButton");
        CHECK_FALSE(s.rig.controller.gate_active());
        CHECK(s.rig.links.opened.empty());
        CHECK(s.visible("aboutScreen"));
    }
    SECTION("The back button and the Escape key open nothing")
    {
        s.tap("backButton");
        CHECK_FALSE(s.rig.controller.gate_active());
        s.tap(link.button);
        REQUIRE(s.rig.controller.gate_active());
        s.key(Qt::Key_Escape);
        CHECK_FALSE(s.rig.controller.gate_active());
        CHECK(s.rig.links.opened.empty());
        CHECK(s.visible("aboutScreen")); // the Escape key closed the gate only
    }
    SECTION("A wrong number opens nothing and deals a new one")
    {
        s.keypad(Screens::wrong_answer(0));
        CHECK(s.rig.controller.gate_active());
        CHECK(s.rig.links.opened.empty());
        CHECK(s.text_of("gateWords") == QString::fromStdString(UiRig::gate_challenge(1).words).toUpper());
        s.tap("cancelButton");
        CHECK(s.rig.links.opened.empty());
    }
    SECTION("The right number opens it once, and the gate is spent")
    {
        s.keypad(Screens::answer(0));
        CHECK(s.rig.links.opened.empty()); // typing is not asking to open
        s.tap("openButton");
        REQUIRE(s.rig.links.opened.size() == 1);
        CHECK(QString::fromStdString(s.rig.links.opened.front()) == link.url);
        CHECK_FALSE(s.rig.controller.gate_active());
        CHECK(s.h.find("gateScreen") == nullptr);
        CHECK(s.visible("aboutScreen"));

        // Another tap on the same row is a new question, and nothing opens until it is answered.
        s.tap(link.button);
        CHECK(s.rig.links.opened.size() == 1);
        s.keypad(Screens::answer(1));
        s.tap("openButton");
        CHECK(s.rig.links.opened.size() == 2);
    }
}

TEST_CASE("The gate shows the words, a keypad and OPEN that stays grey until the number matches", "[ui][gate]")
{
    struct Arrangement
    {
        const char* name;
        QSize size;
    };
    const Arrangement arrangement =
        GENERATE(Arrangement{"phone", PHONE}, Arrangement{"tablet", TABLET}, Arrangement{"desktop", DESKTOP});
    CAPTURE(arrangement.name);
    Screens s(arrangement.size);
    s.tap("settingsButton");
    s.tap("aboutButton");
    s.tap("privacyButton");
    REQUIRE(s.visible("gateScreen"));

    const GateChallenge first = UiRig::gate_challenge(0);
    CHECK(s.text_of("screenTitle") == "ASK A GROWN-UP");
    CHECK(s.text_of("gateWords") == QString::fromStdString(first.words).toUpper());
    CHECK(s.text_of("gateTyped").isEmpty());
    CHECK_FALSE(s.visible("gateFailedText"));
    CHECK_FALSE(s.visible("gateFailedMark"));
    CHECK(property_of(s.h.find("openButton"), "text").toString() == "OPEN");
    CHECK_FALSE(s.gate_open_enabled());

    // Every key is a real hit target and inside the window.
    for (const char* name : {"key0", "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9",
                             "keyDelete", "cancelButton", "openButton", "backButton"})
    {
        INFO(name);
        CHECK(s.item(name)->width() >= 44);
        CHECK(s.item(name)->height() >= 44);
        CHECK(s.inside(s.item(name)));
    }
    CHECK(s.inside(s.item("gatePrompt")));
    CHECK(s.inside(s.item("gateDisplay")));
    // The keypad sits under the display and over the buttons.
    const auto top_of = [&](const char* name) { return s.item(name)->mapToScene(QPointF(0, 0)).y(); };
    const auto bottom_of = [&](const char* name)
    { return s.item(name)->mapToScene(QPointF(0, s.item(name)->height())).y(); };
    CHECK(bottom_of("gatePrompt") <= top_of("gateDisplay"));
    CHECK(bottom_of("gateDisplay") <= top_of("key1"));
    CHECK(bottom_of("key0") <= top_of("openButton"));

    // The digits show as they are typed. OPEN turns on at the last right digit and not before.
    const QString right = Screens::answer(0);
    s.tap("key0"); // may be a wrong first digit: the number is three digits and starts 1 to 9, so this is always wrong
    CHECK(s.text_of("gateTyped") == "0");
    s.tap("keyDelete");
    CHECK(s.text_of("gateTyped").isEmpty());
    s.tap(QStringLiteral("key%1").arg(right[0]).toLatin1().constData());
    CHECK_FALSE(s.gate_open_enabled());
    s.tap(QStringLiteral("key%1").arg(right[1]).toLatin1().constData());
    CHECK_FALSE(s.gate_open_enabled());
    CHECK(s.text_of("gateTyped") == right.left(2));
    s.tap(QStringLiteral("key%1").arg(right[2]).toLatin1().constData());
    CHECK(s.gate_open_enabled());
    CHECK(s.text_of("gateTyped") == right);
    // A fourth digit is not taken.
    s.tap("key5");
    CHECK(s.text_of("gateTyped") == right);
    CHECK(s.gate_open_enabled());
    // Deleting a digit turns OPEN grey again.
    s.tap("keyDelete");
    CHECK_FALSE(s.gate_open_enabled());
    CHECK(s.rig.links.opened.empty());
}

TEST_CASE("A wrong number is said in words with a mark, a new number is dealt and nothing opens", "[ui][gate]")
{
    Screens s(PHONE);
    s.tap("settingsButton");
    s.tap("aboutButton");
    s.tap("privacyButton");
    REQUIRE(s.visible("gateScreen"));
    const QString first_words = s.text_of("gateWords");

    s.keypad(Screens::wrong_answer(0));

    // The message has a mark and words, so it does not rely on colour.
    CHECK(s.visible("gateFailedText"));
    CHECK(s.visible("gateFailedMark"));
    CHECK(s.text_of("gateFailedText") == "Not that number. Here is a new one.");
    CHECK(s.text_of("gateTyped").isEmpty());
    CHECK_FALSE(s.gate_open_enabled());
    CHECK(s.text_of("gateWords") != first_words);
    CHECK(s.text_of("gateWords") == QString::fromStdString(UiRig::gate_challenge(1).words).toUpper());
    CHECK(s.rig.links.opened.empty());
    CHECK(s.rig.controller.gate_active());
    // The message does not push the keypad down.
    const double key_y = s.item("key1")->mapToScene(QPointF(0, 0)).y();

    // The old number no longer opens anything, and the new one does.
    s.keypad(Screens::answer(0) == Screens::answer(1) ? QStringLiteral("000") : Screens::answer(0));
    CHECK(s.rig.links.opened.empty());
    CHECK(s.rig.controller.gate_active());
    // That was a wrong number as well (a third challenge), and the message stays.
    CHECK(s.visible("gateFailedText"));
    CHECK(s.item("key1")->mapToScene(QPointF(0, 0)).y() == key_y);

    s.keypad(Screens::answer(2));
    CHECK_FALSE(s.visible("gateFailedText")); // typing again clears the message
    REQUIRE(s.gate_open_enabled());
    s.tap("openButton");
    REQUIRE(s.rig.links.opened.size() == 1);
    CHECK(QString::fromStdString(s.rig.links.opened.front()) == PRIVACY_URL);
}

TEST_CASE("The gate works with a hardware keyboard", "[ui][gate]")
{
    Screens s(PHONE);
    s.tap("settingsButton");
    s.tap("aboutButton");
    s.tap("privacyButton");
    REQUIRE(s.visible("gateScreen"));

    const QString right = Screens::answer(0);
    const auto key_for = [](QChar digit) { return static_cast<Qt::Key>(Qt::Key_0 + (digit.unicode() - '0')); };

    s.key(key_for(right[0]));
    s.key(key_for(right[1]));
    CHECK(s.text_of("gateTyped") == right.left(2));
    s.key(Qt::Key_Backspace);
    CHECK(s.text_of("gateTyped") == right.left(1));
    s.key(key_for(right[1]));
    s.key(Qt::Key_Return); // two digits are not an answer yet
    CHECK(s.rig.controller.gate_active());
    CHECK(s.rig.links.opened.empty());
    CHECK_FALSE(s.visible("gateFailedText"));
    CHECK(s.text_of("gateTyped") == right.left(2));

    s.key(key_for(right[2]));
    CHECK(s.gate_open_enabled());
    s.key(Qt::Key_Return);
    REQUIRE(s.rig.links.opened.size() == 1);
    CHECK_FALSE(s.rig.controller.gate_active());
    CHECK(s.visible("aboutScreen"));
}

TEST_CASE("The pause screen opens Settings and goes back to the pause screen", "[ui][nav][pause]")
{
    Screens s(PHONE);
    s.tap("playButton");
    REQUIRE(s.rig.controller.state() == State::Playing);
    s.rig.controller.pause();
    s.h.settle();
    REQUIRE(s.visible("pauseOverlay"));
    REQUIRE(s.visible("settingsButton")); // the gear is on the pause screen now

    s.tap("settingsButton");
    CHECK(s.visible("settingsScreen"));
    CHECK_FALSE(s.visible("pauseOverlay"));
    CHECK(s.rig.controller.state() == State::Paused); // the bomb still waits

    s.tap("aboutButton");
    CHECK(s.visible("aboutScreen"));
    s.tap("backButton");
    CHECK(s.visible("settingsScreen"));
    s.tap("backButton");
    CHECK(s.h.find("settingsScreen") == nullptr);
    CHECK(s.visible("pauseOverlay"));
    CHECK(s.rig.controller.state() == State::Paused);

    s.tap("resumeButton");
    CHECK(s.rig.controller.state() == State::Playing);
}

TEST_CASE("A change of the game's state closes any page", "[ui][nav]")
{
    Screens s(PHONE);
    s.tap("playButton");
    s.rig.controller.pause();
    s.h.settle();
    s.tap("settingsButton");
    REQUIRE(s.visible("settingsScreen"));

    // The app returning from the background, or any other resume, moves the state under the page.
    s.rig.controller.resume();
    s.h.settle();
    CHECK(s.h.find("settingsScreen") == nullptr);
    CHECK(s.visible("playScreen"));
}

TEST_CASE("The title, Settings and About follow a window that changes shape", "[ui][title][resize]")
{
    Screens s(PHONE, false, board_save());
    for (const QSize size : {DESKTOP, TABLET, PHONE})
    {
        s.h.resize(size);
        s.window_size = size;
        CAPTURE(size.width());
        CHECK(s.inside(s.item("playButton")));
        CHECK(s.inside(s.item("titleCaption")));
    }
    s.tap("settingsButton");
    s.tap("aboutButton");
    for (const QSize size : {DESKTOP, TABLET, PHONE})
    {
        s.h.resize(size);
        s.window_size = size;
        CAPTURE(size.width());
        CHECK(s.inside(s.item("backButton")));
        if (!property_of(s.item("aboutFlick"), "interactive").toBool())
        {
            CHECK(s.inside(s.item("sourceButton")));
        }
    }
}

TEST_CASE("Screenshots of the title, settings, about and gate screens for the hand check against the boards",
          "[.][shots]")
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
        {
            Screens s(size.size, true, board_save());
            CHECK(s.h.grab(prefix + "title.png")); // CP1
            s.tap("settingsButton");
            CHECK(s.h.grab(prefix + "settings.png")); // CP14
            s.tap("aboutButton");
            CHECK(s.h.grab(prefix + "about.png")); // CP15
            s.tap("privacyButton");
            s.keypad(Screens::answer(0).left(2));
            CHECK(s.h.grab(prefix + "gate.png")); // CP17
            s.tap("keyDelete");
            s.tap("keyDelete");
            s.keypad(Screens::wrong_answer(0));
            CHECK(s.h.grab(prefix + "gate-failed.png"));
            s.tap("cancelButton");
            s.tap("licenceButton");
            CHECK(s.h.grab(prefix + "licence.png"));
            s.tap("backButton");
            s.tap("noticesButton");
            CHECK(s.h.grab(prefix + "notices.png"));
        }
        {
            Screens s(size.size, false, saved_run_text(5, 30000));
            CHECK(s.h.grab(prefix + "title-saved.png")); // CP18
        }
        {
            Screens s(size.size, false, broken_run_text());
            s.tap("continueButton");
            CHECK(s.h.grab(prefix + "title-failed.png"));
        }
    }
}
