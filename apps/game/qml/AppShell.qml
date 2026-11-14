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
//   LevelClear   Level clear      (placeholder)
//   Won          Win              (placeholder)
//   GameOver     Game over        (placeholder)
//
// The play screen stays up for a moment after the last answer of a level, or after the bomb, while AnswerFeedback holds the
// look of that answer, and the next screen follows when the hold ends.
Item {
    id: root

    required property GameController game

    readonly property string sizeClass: Theme.sizeClassFor(width)
    // The screen on show: "title", "play", "levelClear", "won" or "gameOver".
    readonly property string screen: screenFor(game.state, answerFeedback.active)

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
