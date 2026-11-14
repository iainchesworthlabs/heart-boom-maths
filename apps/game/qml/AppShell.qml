// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// Navigation, all of it in this one file: which screen shows for the controller's state. Every screen is a Component here,
// so a screen is added or replaced by editing one place.
//
//   Idle         Title            (placeholder)
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
Item {
    id: root

    required property GameController game

    readonly property string sizeClass: Theme.sizeClassFor(width)
    // A screen laid over the core's state: "" for none, "top10" or "title".
    property string overlay: ""
    // Where the back arrow of the Top 10 goes: the overlay that was showing when it opened.
    property string overlayBack: ""
    // The screen on show: "title", "play", "levelClear", "won", "nameEntry", "gameOver" or "top10".
    readonly property string screen: screenFor(game.state, answerFeedback.active, overlay, game.nameEntryPending)

    // Opens the Top 10 over whatever is showing. The Title screen calls this from its TOP 10 button; the game-over card does
    // too.
    function showTop10() {
        overlayBack = overlay;
        overlay = "top10";
    }

    // The back arrow of the Top 10.
    function closeTop10() {
        overlay = overlayBack;
        overlayBack = "";
    }

    // HOME on the game-over card: the title screen, while the core still rests in its game-over state.
    function showTitle() {
        overlayBack = "";
        overlay = "title";
    }

    function screenFor(state, holding, overlay, naming) {
        if (overlay === "top10" && (state === GameController.Idle || state === GameController.GameOver)) {
            return "top10";
        }
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
            root.overlay = "";
            root.overlayBack = "";
        }
    }

    Loader {
        id: screens

        objectName: "screenLoader"
        anchors.fill: parent
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
            case "top10":
                return top10Screen;
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
            }
        }
    }

    // ---- Placeholders: each is replaced by its own screen ------------------------------------------------------------

    Component {
        id: titleScreen

        Placeholder {
            objectName: "titleScreen"
            title: qsTr("HEART BOOM MATHS")
            subtitle: root.game.bestScore > 0 ? qsTr("Best score %1").arg(Theme.formatNumber(root.game.bestScore)) : ""

            ComicButton {
                objectName: "continueButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                visible: root.game.hasSavedRun
                variant: "secondary"
                text: qsTr("CONTINUE")
                pixelSize: 40
                letterSpacing: 3
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge
                onClicked: root.game.continueGame()
            }

            ComicButton {
                objectName: "playButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                variant: "danger"
                text: root.game.hasSavedRun ? qsTr("NEW GAME") : qsTr("PLAY")
                pixelSize: 40
                letterSpacing: 3
                borderWidth: Theme.outlineLarge
                radius: Theme.radiusLarge
                shadowOffset: Theme.shadowLarge
                onClicked: root.game.startGame()
            }
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

    Component {
        id: top10Screen

        Top10Screen {
            objectName: "top10Screen"
            game: root.game
            // The new entry is marked only while the list follows the game that earned it.
            highlightRank: root.game.state === GameController.GameOver && root.overlayBack !== "title" ? root.game.highlightRank : 0
            playText: root.game.state === GameController.GameOver ? qsTr("PLAY AGAIN") : qsTr("PLAY")
            onBackRequested: root.closeTop10()
            onPlayRequested: root.game.startGame()
        }
    }
}
