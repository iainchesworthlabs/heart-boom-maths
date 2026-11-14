// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The line that follows an answer: "NOT QUITE! IT'S 7" after a wrong one, "+10" after a right one, or the red "HURRY!"
// banner of the alert. It tilts a little, like a comic caption, and pops in. Words and a tilt carry the meaning, so the
// colour only adds to them.
Item {
    id: root

    property string text: ""
    // "wrong" (white, red words), "right" (green) or "alert" (red, white words).
    property string tone: "wrong"
    property real pixelSize: 32
    property real borderWidth: 4
    property real shadowOffset: 5
    property real tilt: tone === "alert" ? -1.5 : (tone === "right" ? 2 : -2)

    implicitWidth: label.implicitWidth + 36 + 2 * borderWidth
    implicitHeight: label.implicitHeight + 8 + 2 * borderWidth

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    // The pop and, for a right answer, the rise.
    property real lift: 0
    scale: 1

    ParallelAnimation {
        running: root.visible && root.text !== ""

        NumberAnimation {
            target: root
            property: "scale"
            from: 0.6
            to: 1
            duration: 220
            easing.type: Easing.OutBack
        }
        NumberAnimation {
            target: root
            property: "lift"
            from: root.tone === "right" ? 22 : 0
            to: 0
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    ComicCard {
        width: parent.width
        height: parent.height
        rotation: root.tilt
        fillColor: root.tone === "alert" ? Theme.red : (root.tone === "right" ? Theme.green : Theme.white)
        borderWidth: root.borderWidth
        radius: root.tone === "alert" ? Theme.radiusSmall : 18
        shadowOffset: root.shadowOffset
        transform: Translate {
            y: root.lift
        }

        Text {
            id: label

            anchors.centerIn: parent
            text: root.text
            color: root.tone === "wrong" ? Theme.red : (root.tone === "alert" ? Theme.white : Theme.ink)
            font.family: Theme.displayFont
            font.pixelSize: root.pixelSize
            font.letterSpacing: 2
        }
    }
}
