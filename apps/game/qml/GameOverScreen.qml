// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The game-over card (board CP7): the level reached, the score, the best score and the coins earned, the wallet, then PLAY
// AGAIN, TOP 10 and HOME. The star burst reads BOOM! only when the bomb went off; a game the child ended (QUIT on the pause
// screen, FINISH on the win screen) has the heading alone.
//
// PLAY AGAIN starts level 1 at once. TOP 10 and HOME are for the shell to act on, because the core has no screen to go to.
ScorePage {
    id: root

    required property GameController game

    readonly property bool bombWentOff: game.endReason === GameController.Bomb

    signal top10Requested
    signal homeRequested

    designHeight: 730
    columnSpacing: 14
    topMax: 52

    StarBurst {
        objectName: "boomBurst"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 190 * root.unit
        Layout.preferredHeight: 190 * root.unit
        visible: root.bombWentOff
        variant: "squashed"
        outerOutline: 4 * root.unit
        innerOutline: 3 * root.unit
        outerColor: Theme.red
        innerColor: Theme.background
        textColor: Theme.ink
        text: qsTr("BOOM!")
        textSize: 44 * root.unit
        letterSpacing: 1 * root.unit
        textRotation: -6
    }

    Text {
        objectName: "gameOverHeading"
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("GAME OVER")
        color: Theme.ink
        font.family: Theme.displayFont
        font.pixelSize: (root.bombWentOff ? 48 : 60) * root.unit
        font.letterSpacing: 3 * root.unit
        lineHeight: 1
    }

    ComicCard {
        objectName: "resultCard"
        Layout.fillWidth: true
        Layout.preferredHeight: rows.implicitHeight + 2 * (16 * root.unit) + 2 * borderWidth
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit

        ColumnLayout {
            id: rows

            anchors.fill: parent
            anchors.leftMargin: 20 * root.unit
            anchors.rightMargin: 20 * root.unit
            anchors.topMargin: 16 * root.unit
            anchors.bottomMargin: 16 * root.unit
            spacing: 8 * root.unit

            StatRow {
                objectName: "levelReachedRow"
                Layout.fillWidth: true
                pixelSize: 22 * root.unit
                label: qsTr("LEVEL REACHED")
                value: String(root.game.finalLevel)
            }

            StatRow {
                objectName: "scoreRow"
                Layout.fillWidth: true
                pixelSize: 22 * root.unit
                label: qsTr("SCORE")
                value: Theme.formatNumber(root.game.finalScore)
            }

            // A new best score is saved in the list first (the name screen), so by the time this card shows the best score
            // is the score itself. The tag says so in words and in its own shape.
            StatRow {
                objectName: "bestScoreRow"
                Layout.fillWidth: true
                pixelSize: 22 * root.unit
                label: qsTr("BEST SCORE")
                value: Theme.formatNumber(root.game.bestScore)

                Badge {
                    objectName: "newBestBadge"
                    visible: root.game.newBest
                    text: qsTr("NEW BEST!")
                    fillColor: Theme.red
                    textColor: Theme.white
                    borderWidth: 3 * root.unit
                    radius: 8 * root.unit
                    pixelSize: 16 * root.unit
                    letterSpacing: 1
                    horizontalPadding: 6 * root.unit
                }
            }

            // Left out when the game was resumed from a saved run: its first part is not known.
            StatRow {
                objectName: "coinsEarnedRow"
                Layout.fillWidth: true
                visible: root.game.coinsEarnedKnown
                pixelSize: 22 * root.unit
                label: qsTr("COINS EARNED")
                value: "+" + Theme.formatNumber(root.game.coinsEarned)
            }
        }
    }

    CoinCard {
        objectName: "walletCard"
        Layout.fillWidth: true
        coinSize: 28 * root.unit
        pixelSize: 34 * root.unit
        notePixelSize: 16 * root.unit
        borderWidth: Theme.outlineSmall * root.unit
        radius: 16 * root.unit
        horizontalPadding: 16 * root.unit
        verticalPadding: 10 * root.unit
        text: qsTr("WALLET %1").arg(Theme.formatNumber(root.game.wallet))
        // "saved" is only true when the save could be written: a save from a newer version is left alone.
        note: root.game.saveBlocked ? "" : qsTr("saved")
    }

    ComicButton {
        objectName: "playAgainButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 72 * root.unit
        variant: "danger"
        text: qsTr("PLAY AGAIN")
        pixelSize: 44 * root.unit
        letterSpacing: 3 * root.unit
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit
        onClicked: root.game.startGame()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14 * root.unit

        ComicButton {
            objectName: "top10Button"
            Layout.fillWidth: true
            Layout.preferredHeight: 56 * root.unit
            variant: "secondary"
            text: qsTr("TOP 10")
            pixelSize: 32 * root.unit
            letterSpacing: 2 * root.unit
            borderWidth: Theme.outlineLarge * root.unit
            radius: 20 * root.unit
            shadowOffset: Theme.shadowSmall * root.unit
            onClicked: root.top10Requested()
        }

        ComicButton {
            objectName: "homeButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 56 * root.unit
            variant: "secondary"
            text: qsTr("HOME")
            pixelSize: 32 * root.unit
            letterSpacing: 2 * root.unit
            borderWidth: Theme.outlineLarge * root.unit
            radius: 20 * root.unit
            shadowOffset: Theme.shadowSmall * root.unit
            onClicked: root.homeRequested()
        }
    }

    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("A new game starts at level 1")
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: 16 * root.unit
    }
}
