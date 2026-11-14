// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The level-clear card (board CP6): the points breakdown, the coins earned and the wallet, then NEXT LEVEL. Every number
// is one the core's LEVEL_CLEARED event carried, or a sum of the answer events of the level that the controller added up;
// nothing is worked out here.
ScorePage {
    id: root

    required property GameController game

    // The numbers of the last cleared level (GameController.levelClear).
    readonly property var card: game ? game.levelClear : ({})
    // False when the level came back from a saved run part way: the answers before the pause are not known, so the card
    // shows what the core gave and leaves the sums out.
    readonly property bool complete: card.complete === true

    designHeight: 640

    StarBurst {
        objectName: "levelClearBurst"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: root.columnWidth
        Layout.preferredHeight: 172 * root.unit
        variant: "wide"
        outerOutline: 4 * root.unit
        innerOutline: 3 * root.unit
        text: qsTr("CLEAR!")
        subText: qsTr("LEVEL %1").arg(root.card.level)
        textSize: 56 * root.unit
        subTextSize: 24 * root.unit
        letterSpacing: 2 * root.unit
    }

    ComicCard {
        objectName: "pointsCard"
        Layout.fillWidth: true
        Layout.preferredHeight: rows.implicitHeight + 2 * (18 * root.unit) + 2 * borderWidth
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit

        ColumnLayout {
            id: rows

            anchors.fill: parent
            anchors.leftMargin: 20 * root.unit
            anchors.rightMargin: 20 * root.unit
            anchors.topMargin: 18 * root.unit
            anchors.bottomMargin: 18 * root.unit
            spacing: 10 * root.unit

            StatRow {
                objectName: "answersRow"
                Layout.fillWidth: true
                pixelSize: 22 * root.unit
                label: qsTr("%1 of %2 right").arg(root.card.correctAnswers).arg(root.game.questionsPerLevel)
                value: root.complete ? "+" + Theme.formatNumber(root.card.answerPoints) : ""
            }

            // A perfect level says so in words as well as in red.
            StatRow {
                objectName: "perfectRow"
                Layout.fillWidth: true
                visible: root.card.perfect === true
                pixelSize: 22 * root.unit
                textColor: Theme.red
                label: qsTr("PERFECT LEVEL!")
                value: "+" + Theme.formatNumber(root.card.perfectBonusPoints)
            }

            StatRow {
                objectName: "timeRow"
                Layout.fillWidth: true
                pixelSize: 22 * root.unit
                label: qsTr("%1 s left × %2").arg(root.card.secondsLeft).arg(root.card.timeBonusPerSecond)
                value: "+" + Theme.formatNumber(root.card.timeBonusPoints)
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4 * root.unit
                radius: 2 * root.unit
                color: Theme.ink
            }

            // The level's own points when they are all known; the score so far when the start of the level is not.
            StatRow {
                objectName: "levelPointsRow"
                Layout.fillWidth: true
                display: true
                pixelSize: 34 * root.unit
                label: root.complete ? qsTr("LEVEL POINTS") : qsTr("SCORE")
                value: Theme.formatNumber(root.complete ? root.card.levelPoints : root.card.totalPoints)
            }
        }
    }

    CoinCard {
        objectName: "coinsCard"
        Layout.fillWidth: true
        coinSize: 28 * root.unit
        pixelSize: 34 * root.unit
        notePixelSize: 18 * root.unit
        borderWidth: Theme.outlineSmall * root.unit
        radius: 16 * root.unit
        horizontalPadding: 16 * root.unit
        verticalPadding: 10 * root.unit
        // The coins of the level when they are all known; otherwise just the wallet, in the big words.
        text: root.complete ? qsTr("+%1 COINS").arg(Theme.formatNumber(root.card.levelCoins)) : qsTr("WALLET %1").arg(Theme.formatNumber(root.card.wallet))
        note: root.complete ? qsTr("WALLET %1").arg(Theme.formatNumber(root.card.wallet)) : ""
    }

    ComicButton {
        objectName: "nextLevelButton"
        Layout.fillWidth: true
        Layout.preferredHeight: 72 * root.unit
        text: qsTr("NEXT LEVEL")
        pixelSize: 42 * root.unit
        letterSpacing: 3 * root.unit
        borderWidth: Theme.outlineLarge * root.unit
        radius: Theme.radiusLarge * root.unit
        shadowOffset: Theme.shadowLarge * root.unit
        onClicked: root.game.nextLevel()
    }

    Text {
        objectName: "nextUpText"
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.card.nextUp ?? ""
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: 18 * root.unit
    }
}
