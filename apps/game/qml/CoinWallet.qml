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
    // The room on each side of the content, inside the border, and the gap between the coin and the count.
    property real horizontalPadding: 12
    property real spacing: 6

    implicitWidth: row.implicitWidth + 2 * (horizontalPadding + borderWidth)
    implicitHeight: pixelSize + 16 + 2 * borderWidth

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("%1 coins").arg(root.value)

    ComicCard {
        anchors.fill: parent
        borderWidth: root.borderWidth
        radius: root.radius
        shadowOffset: 0

        Row {
            id: row

            anchors.centerIn: parent
            spacing: root.spacing

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
