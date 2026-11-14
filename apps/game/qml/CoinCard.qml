// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The white card with a coin on its left: "+17 COINS" and the wallet on the level-clear card, "+1,000 COINS" on the win
// screen, "WALLET 61" on the game-over card. `text` is the big words in the heading font, `note` a small line on the right.
Item {
    id: root

    property string text: ""
    property string note: ""
    property real coinSize: 28
    property real pixelSize: 34
    property real notePixelSize: 18
    property real borderWidth: 4
    property real radius: 16
    property real shadowOffset: 0
    property real horizontalPadding: 16
    property real verticalPadding: 10
    // The coin and the words sit in the middle of the card instead of at its left.
    property bool centered: false

    readonly property alias coinItem: coin
    readonly property alias textItem: label
    readonly property alias noteItem: noteLabel

    implicitHeight: row.implicitHeight + 2 * (verticalPadding + borderWidth)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.note !== "" ? root.text + ", " + root.note : root.text

    ComicCard {
        anchors.fill: parent
        borderWidth: root.borderWidth
        radius: root.radius
        shadowOffset: root.shadowOffset

        RowLayout {
            id: row

            anchors.fill: parent
            anchors.leftMargin: root.horizontalPadding
            anchors.rightMargin: root.horizontalPadding
            anchors.topMargin: root.verticalPadding
            anchors.bottomMargin: root.verticalPadding
            spacing: 10

            Item {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                visible: root.centered
            }

            Rectangle {
                id: coin

                Layout.preferredWidth: root.coinSize
                Layout.preferredHeight: root.coinSize
                Layout.alignment: Qt.AlignVCenter
                radius: width / 2
                color: Theme.coin
                border.width: Math.max(1, root.coinSize / 18 * 1.5)
                border.color: Theme.ink
            }

            Text {
                id: label

                Layout.alignment: Qt.AlignVCenter
                text: root.text
                color: Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: root.pixelSize
                font.letterSpacing: 1
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
            }

            Text {
                id: noteLabel

                Layout.alignment: Qt.AlignVCenter
                visible: root.note !== "" && !root.centered
                text: root.note
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: root.notePixelSize
            }
        }
    }
}
