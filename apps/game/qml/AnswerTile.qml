// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// One answer tile. Its `look` says what the screen wants shown, and each look has a shape or motion cue as well as a
// colour, so the game plays the same without colour:
//
//   normal  white, takes a tap
//   wrong   red, tilted, shakes, a cross in the corner
//   right   green, a little bigger, pulses, a tick in the corner
//   dimmed  faded: the tiles that were neither tapped nor right
//   gone    a hint removed it: the tile fades and drops away and leaves a dashed ghost
Item {
    id: root

    property string value: ""
    // "normal", "wrong", "right", "dimmed" or "gone".
    property string look: "normal"
    // A tile takes taps only in the normal look, and only when the screen allows answers.
    property bool interactive: true

    property real fontSize: 50
    property real borderWidth: Theme.outlineLarge
    property real radius: 20
    property real shadowOffset: Theme.shadowSmall
    property real badgeSize: 30

    readonly property bool tappable: interactive && look === "normal"
    readonly property bool pressed: tap.pressed

    signal tapped

    // How far a wrong tile has shaken from its rest, for the animation.
    property real shake: 0

    implicitHeight: 96
    activeFocusOnTab: tappable

    Accessible.role: Accessible.Button
    Accessible.name: look === "gone" ? "" : (look === "wrong" ? qsTr("Answer %1, wrong").arg(value) : (look === "right" ? qsTr("Answer %1, right answer").arg(value) : qsTr("Answer %1").arg(value)))
    Accessible.focusable: tappable
    Accessible.ignored: look === "gone"
    Accessible.onPressAction: if (root.tappable)
        root.tapped()

    Keys.onPressed: event => {
        if (root.tappable && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)) {
            event.accepted = true;
            root.tapped();
        }
    }

    // What is left where a hint took the tile.
    DashedBox {
        objectName: "tileGhost"
        anchors.fill: parent
        opacity: root.look === "gone" ? 1 : 0
        lineColor: Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.35)
        lineWidth: root.borderWidth
        radius: root.radius
        dashPattern: [2, 1.2]

        Behavior on opacity {
            NumberAnimation {
                duration: 250
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        visible: root.activeFocus && root.tappable
        radius: root.radius + 6
        color: "transparent"
        border.width: 3
        border.color: Theme.ink
    }

    ComicCard {
        id: face

        objectName: "tileFace"
        width: parent.width
        height: parent.height
        fillColor: root.look === "wrong" ? Theme.red : (root.look === "right" ? Theme.green : Theme.white)
        borderWidth: root.borderWidth
        radius: root.radius
        shadowOffset: root.shadowOffset
        opacity: root.look === "gone" ? 0 : (root.look === "dimmed" ? 0.55 : 1)
        scale: root.look === "right" ? pulse : 1
        rotation: root.look === "wrong" ? -4 : 0
        transform: Translate {
            x: (root.look === "wrong" ? -6 + root.shake : 0) + (root.pressed ? root.shadowOffset / 2 : 0)
            y: (root.look === "gone" ? root.height * 0.3 : 0) + (root.pressed ? root.shadowOffset / 2 : 0)
        }

        property real pulse: 1.04

        Behavior on opacity {
            NumberAnimation {
                duration: 250
            }
        }

        SequentialAnimation on pulse {
            running: root.look === "right"
            loops: Animation.Infinite

            NumberAnimation {
                to: 1.09
                duration: 220
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                to: 1.04
                duration: 220
                easing.type: Easing.InOutSine
            }
        }

        Text {
            anchors.centerIn: parent
            text: root.value
            color: root.look === "wrong" ? Theme.white : Theme.ink
            font.family: Theme.bodyFont
            font.pixelSize: root.fontSize
        }

        // The corner badge: a cross on the wrong tile, a tick on the right one.
        Shape {
            objectName: "tileBadge"
            visible: root.look === "wrong" || root.look === "right"
            x: parent.width - root.badgeSize - 6
            y: 4 - root.borderWidth
            width: 30
            height: 30
            scale: root.badgeSize / 30
            transformOrigin: Item.TopLeft
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: Theme.white
                strokeColor: Theme.ink
                strokeWidth: 3

                PathSvg {
                    path: "M3 15 A12 12 0 1 0 27 15 A12 12 0 1 0 3 15 Z"
                }
            }
            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.ink
                strokeWidth: 3.5
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: root.look === "wrong" ? "M10 10 L20 20 M20 10 L10 20" : "M9 15 L13 19 L21 11"
                }
            }
        }
    }

    // The shake of a wrong tile, once, when the look turns wrong.
    SequentialAnimation {
        id: shaking

        running: root.look === "wrong"

        NumberAnimation {
            target: root
            property: "shake"
            to: 9
            duration: 55
        }
        NumberAnimation {
            target: root
            property: "shake"
            to: -9
            duration: 90
        }
        NumberAnimation {
            target: root
            property: "shake"
            to: 6
            duration: 80
        }
        NumberAnimation {
            target: root
            property: "shake"
            to: -4
            duration: 70
        }
        NumberAnimation {
            target: root
            property: "shake"
            to: 0
            duration: 55
        }
    }

    TapHandler {
        id: tap

        enabled: root.tappable
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.tapped()
    }
}
