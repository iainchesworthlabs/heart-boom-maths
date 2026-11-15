// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The coin count in a white pill. A change that has just happened (a hint costs 2) shows beside it in red with its sign,
// so the sign carries the meaning and the colour only adds to it.
Item {
    id: root

    property int value: 0
    // A signed change to show beside the count; 0 shows nothing.
    property int delta: 0
    property real coinSize: 18
    property real pixelSize: 20
    property real deltaPixelSize: 15
    property real borderWidth: Theme.outlineSmall
    property real radius: Theme.radiusSmall
    // The count in the heading font, as the title screen has it.
    property bool displayNumerals: false
    // The coin and the count without the pill, for a card that already has its own outline (the title's player card).
    property bool plain: false

    implicitWidth: plain ? row.implicitWidth : row.implicitWidth + 24 + 2 * borderWidth
    implicitHeight: plain ? Math.max(coinSize, row.implicitHeight) : pixelSize + 16 + 2 * borderWidth

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("%1 coins").arg(root.value)

    ComicCard {
        anchors.fill: parent
        fillColor: root.plain ? "transparent" : Theme.white
        borderWidth: root.plain ? 0 : root.borderWidth
        radius: root.radius
        shadowOffset: 0

        Row {
            id: row

            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: root.coinSize
                height: root.coinSize
                radius: width / 2
                color: Theme.coin
                border.width: 1.5
                border.color: Theme.ink
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: Theme.formatNumber(root.value)
                color: Theme.ink
                font.family: root.displayNumerals ? Theme.displayFont : Theme.bodyFont
                font.pixelSize: root.pixelSize
                font.letterSpacing: root.displayNumerals ? 1 : 0
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.delta !== 0
                text: (root.delta < 0 ? "−" : "+") + Math.abs(root.delta)
                color: Theme.red
                font.family: Theme.bodyFont
                font.pixelSize: root.deltaPixelSize
            }
        }
    }
}
