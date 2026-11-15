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
// Over any of those can sit a page: Settings, About, Top 10, WHO'S PLAYING, or the name screen of a player. They stack (About
// opens from Settings and goes back to it; the name screen opens from WHO'S PLAYING and goes back to it), and any change of
// the game's state clears them, so a page never outlives the game it was opened in. The pause screen opens Settings too.
// The parental gate sits on top of everything while the controller has one open.
//
// A game that ends with a place in the top 10 for a player who has a name has no name screen: the controller saves the entry
// at once and says so (`scoreSavedAtOnce`), and the Top 10 page opens as soon as the game-over card is due, as it does after
// SAVE.
Item {
    id: root

    required property GameController game

    // The controls ask Haptics for their tick, and Haptics asks this controller.
    Binding {
        target: Haptics
        property: "controller"
        value: root.game
    }

    readonly property string sizeClass: Theme.sizeClassFor(width)
    // The title screen laid over the core's game-over state after HOME: "" for none or "title".
    property string overlay: ""
    // The screen on show: "title", "play", "levelClear", "won", "nameEntry" or "gameOver", or "top10" while that page is open.
    readonly property string screen: page === "top10" ? "top10" : screenFor(game.state, answerFeedback.active, overlay, game.nameEntryPending)

    // The pages opened over the screen, the last one on top: "settings", "about", "top10", "players" or "playerName".
    property var pages: []
    // The player the "playerName" page is for, and whether the player has just been added.
    property string namingPlayer: ""
    property bool namingNew: false
    // A score was saved without a name screen and the Top 10 page is still to open (it waits for the boom to end).
    property bool top10Due: false
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

    // The way in from the player chip on the title.
    function showPlayers() {
        root.pages = ["players"];
    }

    function showPlayerName(playerId, isNew) {
        root.namingPlayer = playerId;
        root.namingNew = isNew;
        root.pages = root.pages.concat(["playerName"]);
    }

    function openDueTop10() {
        if (root.top10Due && root.screen === "gameOver") {
            root.top10Due = false;
            root.showTop10();
        }
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

    // What the Android back key does on the screens that have no back button of their own. Settings, About and the gate say
    // `backClicked` themselves (ScreenFrame), and the Title is left to the system, which closes the app. The play screen
    // pauses, the pause screen resumes, the Top 10 goes back and the game-over card goes home. On the level-clear, win and
    // name screens the key does nothing: each has its buttons, and a stray press must not close the app on a score that
    // was not saved yet. The Escape key of a desktop keeps its own shortcuts on the same screens.
    function backKeyPressed() {
        if (root.page === "top10" || root.page === "playerName") {
            root.back();
            return;
        }
        switch (root.game.state) {
        case GameController.Playing:
            root.game.pause();
            break;
        case GameController.Paused:
            root.game.resume();
            break;
        case GameController.GameOver:
            if (root.screen === "gameOver") {
                root.showTitle();
            }
            break;
        default:
            break;
        }
    }

    Shortcut {
        sequence: "Back"
        enabled: !root.game.gateActive && (root.page === "top10" || root.page === "playerName" || (root.page === "" && root.screen !== "title"))
        onActivated: root.backKeyPressed()
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
            root.top10Due = false;
        }

        function onScoreSavedAtOnce() {
            root.top10Due = true;
            root.openDueTop10();
        }
    }

    // The boom holds the play screen up for a moment; the Top 10 of a saved score opens when the card is due. It opens a
    // moment after the change, not inside it: opening it changes `screen` again.
    onScreenChanged: Qt.callLater(root.openDueTop10)

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
            onPlayersRequested: root.showPlayers()
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
            case "players":
                return playersPage;
            case "playerName":
                return playerNamePage;
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

    Component {
        id: playersPage

        PlayersScreen {
            objectName: "playersScreen"
            game: root.game
            onNameRequested: (playerId, isNew) => root.showPlayerName(playerId, isNew)
            onBackClicked: root.back()
        }
    }

    // The name screen of a player: a new one (SAVE names it, SKIP leaves it unnamed) or one being renamed (SAVE or CANCEL).
    Component {
        id: playerNamePage

        NameEntryScreen {
            objectName: "playerNameScreen"
            game: root.game
            forPlayer: true
            playerId: root.namingPlayer
            initialName: root.game.players.find(p => p.id === root.namingPlayer)?.name ?? ""
            burstText: root.namingNew ? qsTr("NEW PLAYER") : qsTr("NEW NAME")
            skipText: root.namingNew ? qsTr("SKIP") : qsTr("CANCEL")
            onFinished: root.back()
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
            // PLAY AGAIN follows a game that has just ended. Otherwise the title is on show and the button says what the
            // title's own one says, NEW GAME when a saved run is waiting there, because starting a game drops that run.
            playText: {
                if (root.game.state === GameController.GameOver && root.overlay !== "title") {
                    return qsTr("PLAY AGAIN");
                }
                return root.game.hasSavedRun ? qsTr("NEW GAME") : qsTr("PLAY");
            }
            onBackRequested: root.back()
            onPlayRequested: root.game.startGame()
        }
    }
}
