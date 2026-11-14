// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// Navigation, all of it in this one file: which screen shows for the controller's state. Every screen is a Component here,
// so a screen is added or replaced by editing one place.
//
//   Idle         Title
//   Playing      Play
//   Paused       Play with the Pause overlay on top
//   LevelClear   Level clear      (placeholder)
//   Won          Win              (placeholder)
//   GameOver     Game over        (placeholder)
//
// The play screen stays up for a moment after the last answer of a level, or after the bomb, while AnswerFeedback holds the
// look of that answer, and the next screen follows when the hold ends.
//
// Over any of those can sit a page: Settings, About, or Top 10. They stack (About opens from Settings and goes back to it),
// and any change of the game's state clears them, so a page never outlives the game it was opened in. The pause screen
// opens Settings too. The parental gate sits on top of everything while the controller has one open.
Item {
    id: root

    required property GameController game

    readonly property string sizeClass: Theme.sizeClassFor(width)
    // The screen on show: "title", "play", "levelClear", "won" or "gameOver".
    readonly property string screen: screenFor(game.state, answerFeedback.active)

    // The pages opened over the screen, the last one on top: "settings", "about" or "top10".
    property var pages: []
    // The page on top, or "" for none.
    readonly property string page: pages.length > 0 ? pages[pages.length - 1] : ""

    function showSettings() {
        root.pages = ["settings"];
    }

    function showAbout() {
        root.pages = root.pages.concat(["about"]);
    }

    // Also the way in from the game-over screen.
    function showTop10() {
        root.pages = ["top10"];
    }

    // Gives a loaded screen the keyboard.
    function focusItem(loaded) {
        const screenItem = loaded as Item;
        if (screenItem !== null) {
            screenItem.forceActiveFocus();
        }
    }

    // One page back, to the screen underneath when there is no page left.
    function back() {
        root.pages = root.pages.slice(0, -1);
    }

    function screenFor(state, holding) {
        switch (state) {
        case GameController.Playing:
        case GameController.Paused:
            return "play";
        case GameController.LevelClear:
            return holding ? "play" : "levelClear";
        case GameController.Won:
            return holding ? "play" : "won";
        case GameController.GameOver:
            return holding ? "play" : "gameOver";
        default:
            return "title";
        }
    }

    AnswerFeedback {
        id: answerFeedback

        objectName: "answerFeedback"
        game: root.game
    }

    Connections {
        target: root.game

        function onStateChanged() {
            root.pages = [];
        }
    }

    Loader {
        id: screens

        objectName: "screenLoader"
        anchors.fill: parent
        // Hidden under a page, so nothing of it takes a tap, a key or a screen reader's attention.
        visible: root.page === ""
        enabled: !root.game.gateActive
        sourceComponent: {
            switch (root.screen) {
            case "play":
                return playScreen;
            case "levelClear":
                return levelClearScreen;
            case "won":
                return winScreen;
            case "gameOver":
                return gameOverScreen;
            default:
                return titleScreen;
            }
        }
    }

    Component {
        id: playScreen

        Item {
            PlayScreen {
                objectName: "playScreen"
                anchors.fill: parent
                game: root.game
                feedback: answerFeedback
                enabled: root.game.state !== GameController.Paused
            }

            PauseOverlay {
                objectName: "pauseOverlay"
                anchors.fill: parent
                visible: root.game.state === GameController.Paused
                game: root.game
                showSettings: true
                onSettingsRequested: root.showSettings()
            }
        }
    }

    Component {
        id: titleScreen

        TitleScreen {
            objectName: "titleScreen"
            game: root.game
            onTop10Requested: root.showTop10()
            onSettingsRequested: root.showSettings()
        }
    }

    // ---- Pages, over the screen --------------------------------------------------------------------------------------

    Loader {
        id: pageLoader

        objectName: "pageLoader"
        anchors.fill: parent
        active: root.page !== ""
        enabled: !root.game.gateActive
        // The page takes the keyboard (the Android back key) when it opens.
        onLoaded: root.focusItem(item)
        sourceComponent: {
            switch (root.page) {
            case "settings":
                return settingsPage;
            case "about":
                return aboutPage;
            case "top10":
                return top10Page;
            default:
                return null;
            }
        }
    }

    Component {
        id: settingsPage

        SettingsScreen {
            objectName: "settingsScreen"
            game: root.game
            onAboutRequested: root.showAbout()
            onBackClicked: root.back()
        }
    }

    Component {
        id: aboutPage

        AboutScreen {
            objectName: "aboutScreen"
            game: root.game
            onBackClicked: root.back()
        }
    }

    // The parental gate covers everything, the pages too, for as long as the controller has one open. CANCEL, the back
    // button and the Escape key close it; the controller closes it itself when the number is right.
    Loader {
        id: gateLoader

        objectName: "gateLoader"
        anchors.fill: parent
        active: root.game.gateActive
        sourceComponent: gatePage
        // The gate takes the keyboard when it opens, so a grown-up can type the number.
        onLoaded: root.focusItem(item)
    }

    // The page underneath gets the keyboard back when the gate closes.
    Connections {
        target: root.game

        function onGateChanged() {
            if (!root.game.gateActive) {
                root.focusItem(pageLoader.item);
            }
        }
    }

    Component {
        id: gatePage

        ParentalGateScreen {
            objectName: "gateScreen"
            game: root.game
            onBackClicked: root.game.cancelGate()
        }
    }

    // ---- Placeholders: each is replaced by its own screen ------------------------------------------------------------

    // HOOK for the Top 10 screen: showTop10() puts "top10" on the page stack and this is what shows. The real screen
    // replaces this Component (it can use ScreenFrame, as Settings does, or its own page) and keeps the objectName.
    Component {
        id: top10Page

        ScreenFrame {
            objectName: "top10Screen"
            title: qsTr("TOP 10")
            onBackClicked: root.back()

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("The leaderboard is coming soon.")
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 22
            }
        }
    }

    Component {
        id: levelClearScreen

        Placeholder {
            objectName: "levelClearScreen"
            title: qsTr("LEVEL %1 CLEAR!").arg(root.game.levelClear.level)
            subtitle: qsTr("%1 points so far").arg(Theme.formatNumber(root.game.points))

            ComicButton {
                objectName: "nextLevelButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                text: qsTr("NEXT LEVEL")
                pixelSize: 40
                letterSpacing: 3
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge
                onClicked: root.game.nextLevel()
            }
        }
    }

    Component {
        id: winScreen

        Placeholder {
            objectName: "winScreen"
            title: qsTr("YOU WIN!")
            subtitle: qsTr("%1 points").arg(Theme.formatNumber(root.game.points))

            ComicButton {
                objectName: "keepGoingButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                text: qsTr("KEEP GOING")
                pixelSize: 40
                letterSpacing: 3
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge
                onClicked: root.game.keepGoing()
            }

            ComicButton {
                objectName: "finishButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                variant: "secondary"
                text: qsTr("FINISH")
                pixelSize: 34
                letterSpacing: 2
                borderWidth: Theme.outlineLarge
                radius: 18
                shadowOffset: Theme.shadowSmall
                onClicked: root.game.finish()
            }
        }
    }

    Component {
        id: gameOverScreen

        Placeholder {
            objectName: "gameOverScreen"
            title: root.game.endReason === GameController.Bomb ? qsTr("BOOM!") : qsTr("GAME OVER")
            subtitle: qsTr("Level %1, %2 points").arg(root.game.finalLevel).arg(Theme.formatNumber(root.game.finalScore))

            ComicButton {
                objectName: "playAgainButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                variant: "danger"
                text: qsTr("PLAY AGAIN")
                pixelSize: 40
                letterSpacing: 3
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge
                onClicked: root.game.startGame()
            }
        }
    }
}
