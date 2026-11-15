// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The two cues of the play screen that belong to the whole window and not to its content: the red frame that pulses in
// the last 10 seconds, and the white flash when the multiplier steps up. AppRoot lays this over the safe-area frame, so on
// a phone both reach the real edge of the screen, under the cut-out and the home indicator, while the play screen's
// content stays inside the safe area. The play screen drives it through the shell (`alert` and `flash()`); nothing here
// takes a tap, a key or a screen reader's attention.
//
// The frame is one thin line whose outer edge is the window's edge, the same width all the way round, whatever the safe
// area is: the status bar and the home indicator sit inside it and stay readable. Qt does not report how round a display's
// corners are, so a rule stands in. A window with no bottom margin (a desktop window, a browser, a phone with a square
// screen) gets square corners and the plain 8 pixel line of board CP3. A bottom margin is the home indicator, and only
// phones and tablets with rounded screens have one, so the frame's outer corners are rounded to follow the screen. Real
// corners differ by device, so the radius is a middle value from Theme: where the screen is rounder than the frame, the
// screen cuts the frame's corner and the line is wide enough not to vanish there; where it is less round, a small gap shows.
Item {
    id: root

    /// True while the red frame shows.
    property bool alert: false
    /// What the device keeps clear at the bottom, from SafeArea: above zero on a screen with rounded corners.
    property real unsafeBottom: 0

    readonly property bool roundedScreen: unsafeBottom > 0
    readonly property real lineWidth: roundedScreen ? Theme.alertLineRounded : Theme.alertLine
    readonly property real cornerRadius: !roundedScreen ? 0
        : (Theme.sizeClassFor(Math.min(width, height)) === "compact" ? Theme.alertCornerCompact : Theme.alertCornerRegular)

    /// A short white flash over the window: the visual twin of the rising tone.
    function flash() {
        flashAnimation.restart();
    }

    enabled: false

    // The red frame: one transparent rectangle with a border, so the pulse of its opacity moves only the line.
    Rectangle {
        id: edge

        objectName: "alertEdge"
        anchors.fill: parent
        visible: root.alert
        enabled: false
        color: "transparent"
        radius: root.cornerRadius
        border.width: root.lineWidth
        border.color: Theme.red

        SequentialAnimation on opacity {
            running: edge.visible
            loops: Animation.Infinite

            NumberAnimation {
                from: 1
                to: 0.45
                duration: 450
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                from: 0.45
                to: 1
                duration: 450
                easing.type: Easing.InOutSine
            }
        }
    }

    Rectangle {
        id: flashSheet

        objectName: "streakFlash"
        anchors.fill: parent
        color: Theme.white
        opacity: 0
        enabled: false

        SequentialAnimation {
            id: flashAnimation

            NumberAnimation {
                target: flashSheet
                property: "opacity"
                to: 0.5
                duration: 70
            }
            NumberAnimation {
                target: flashSheet
                property: "opacity"
                to: 0
                duration: 220
            }
        }
    }
}
