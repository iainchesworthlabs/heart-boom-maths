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
//   LevelClear   Level clear
//   Won          Win
//   GameOver     Name entry when the score earned a place, then Game over
//
// Two screens are not a state of the core: the Top 10 (from Title, from Game over, and after SAVE) and Title again after HOME
// on the game-over card. `overlay` says which one is on top. It is cleared whenever the core's state changes, so a new game,
// CONTINUE or anything else the core does ends it.
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
    // The title screen laid over the core's game-over state after HOME: "" for none or "title".
    property string overlay: ""
    // The screen on show: "title", "play", "levelClear", "won", "nameEntry" or "gameOver", or "top10" while that page is open.
    readonly property string screen: page === "top10" ? "top10" : screenFor(game.state, answerFeedback.active, overlay, game.nameEntryPending)

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

    // HOME on the game-over card: the title screen, while the core still rests in its game-over state.
    function showTitle() {
        overlay = "title";
    }

    function screenFor(state, holding, overlay, naming) {
        switch (state) {
        case GameController.Playing:
        case GameController.Paused:
            return "play";
        case GameController.LevelClear:
            return holding ? "play" : "levelClear";
        case GameController.Won:
            return holding ? "play" : "won";
        case GameController.GameOver:
            if (holding) {
                return "play";
            }
            if (naming) {
                return "nameEntry";
            }
            return overlay === "title" ? "title" : "gameOver";
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
            root.overlay = "";
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
            case "nameEntry":
                return nameEntryScreen;
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

    // ---- The result and score screens ----------------------------------------------------------------------------------

    Component {
        id: levelClearScreen

        LevelClearScreen {
            objectName: "levelClearScreen"
            game: root.game
        }
    }

    Component {
        id: winScreen

        WinScreen {
            objectName: "winScreen"
            game: root.game
        }
    }

    Component {
        id: nameEntryScreen

        NameEntryScreen {
            objectName: "nameEntryScreen"
            game: root.game
            // SAVE goes on to the Top 10 with the new entry marked; SKIP lands on the game-over card by itself.
            onSaveStarted: root.showTop10()
        }
    }

    Component {
        id: gameOverScreen

        GameOverScreen {
            objectName: "gameOverScreen"
            game: root.game
            onTop10Requested: root.showTop10()
            onHomeRequested: root.showTitle()
        }
    }

    // The Top 10 is a page, so it opens the same way from the Title, the game-over card and the name screen.
    Component {
        id: top10Page

        Top10Screen {
            objectName: "top10Screen"
            game: root.game
            // The new entry is marked only while the list follows the game that earned it.
            highlightRank: root.game.state === GameController.GameOver && root.overlay !== "title" ? root.game.highlightRank : 0
            playText: root.game.state === GameController.GameOver ? qsTr("PLAY AGAIN") : qsTr("PLAY")
            onBackRequested: root.back()
            onPlayRequested: root.game.startGame()
        }
    }
}
