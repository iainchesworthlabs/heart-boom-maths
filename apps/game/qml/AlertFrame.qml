// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The two cues of the play screen that belong to the whole window and not to its content: the red frame that pulses in
// the last 10 seconds, and the white flash when the multiplier steps up. AppRoot lays this over the safe-area frame, so on
// a phone both reach the real edge of the screen, under the cut-out and the home indicator, while the play screen's
// content stays inside the safe area. The play screen drives it through the shell (`alert` and `flash()`); nothing here
// takes a tap, a key or a screen reader's attention.
//
// A phone's corners are rounded and its cut-out hides part of the top edge, so an 8 pixel line at the window edge would
// all but vanish there. Each side is therefore as wide as the line plus the margin the device keeps clear on that side:
// the frame is the old one (its line just inside the safe area) carried out to the edge, and it reads as a frame at the
// corners too. With no margins (a desktop window) it is the plain 8 pixel line at the window edge, as board CP3 draws it.
Item {
    id: root

    /// True while the red frame shows.
    property bool alert: false
    /// What the device keeps clear on each side, from SafeAreaFrame.
    property real unsafeLeft: 0
    property real unsafeTop: 0
    property real unsafeRight: 0
    property real unsafeBottom: 0
    /// The line of the frame inside the safe area.
    property real lineWidth: 8

    /// A short white flash over the window: the visual twin of the rising tone.
    function flash() {
        flashAnimation.restart();
    }

    enabled: false

    // The red frame: four bars that do not overlap, so the pulse of the group's opacity never doubles up at a corner.
    Item {
        id: edge

        objectName: "alertEdge"
        anchors.fill: parent
        visible: root.alert
        enabled: false

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

        Rectangle {
            id: topBar

            objectName: "alertEdgeTop"
            x: 0
            y: 0
            width: edge.width
            height: root.lineWidth + root.unsafeTop
            color: Theme.red
        }

        Rectangle {
            id: bottomBar

            objectName: "alertEdgeBottom"
            x: 0
            y: edge.height - height
            width: edge.width
            height: root.lineWidth + root.unsafeBottom
            color: Theme.red
        }

        Rectangle {
            objectName: "alertEdgeLeft"
            x: 0
            y: topBar.height
            width: root.lineWidth + root.unsafeLeft
            height: Math.max(0, edge.height - topBar.height - bottomBar.height)
            color: Theme.red
        }

        Rectangle {
            objectName: "alertEdgeRight"
            x: edge.width - width
            y: topBar.height
            width: root.lineWidth + root.unsafeRight
            height: Math.max(0, edge.height - topBar.height - bottomBar.height)
            color: Theme.red
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
