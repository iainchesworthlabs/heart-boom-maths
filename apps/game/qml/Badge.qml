// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// A small tag: the multiplier, ENDLESS, "-3 s" and "+5 s". A dashed outline marks a value that is changing.
Item {
    id: root

    property string text: ""
    property color fillColor: Theme.white
    property color textColor: Theme.ink
    property color borderColor: Theme.ink
    property real borderWidth: Theme.outlineSmall // 0 draws no outline
    property real radius: Theme.radiusSmall
    property bool dashed: false
    property real shadowOffset: 0
    property real pixelSize: 24
    property real letterSpacing: 1
    property real horizontalPadding: 8
    property real verticalPadding: 0
    // A badge may hold something besides text, such as an arrow.
    default property alias content: extra.data

    implicitWidth: Math.max(label.implicitWidth, extra.childrenRect.width) + 2 * (horizontalPadding + borderWidth)
    implicitHeight: Math.max(label.implicitHeight, extra.childrenRect.height) + 2 * (verticalPadding + borderWidth)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    ComicCard {
        anchors.fill: parent
        fillColor: root.fillColor
        borderColor: root.borderColor
        borderWidth: root.borderWidth
        radius: root.radius
        dashed: root.dashed
        shadowOffset: root.shadowOffset

        Text {
            id: label

            anchors.centerIn: parent
            visible: root.text !== ""
            text: root.text
            color: root.textColor
            font.family: Theme.displayFont
            font.pixelSize: root.pixelSize
            font.letterSpacing: root.letterSpacing
        }

        Item {
            id: extra

            anchors.centerIn: parent
            width: childrenRect.width
            height: childrenRect.height
        }
    }
}
