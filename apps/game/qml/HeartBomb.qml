// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The heart bomb, the one character of the game, in its three states.
//
//   calm   pink heart, black outline, metal cap, rope fuse, an orange spark, the seconds left in black numerals
//   alert  red body, white numerals, the heart tilts and shakes, speed lines, a bigger yellow spark: the shape and the
//          motion change as well as the colour
//   boom   the heart pops into a star burst
//
// It is drawn on a 120 by 120 grid and scaled to `size`, so it holds from an icon to the title screen.
Item {
    id: root

    property int secondsLeft: 0
    property bool alert: false
    property bool boom: false
    property real size: 120
    // The seconds in the heart. Off for the title and About, where the heart is a picture and not a timer.
    property bool showSeconds: true
    // Two eyes and a smile, for the title screen.
    property bool face: false

    readonly property string stateName: boom ? "boom" : (alert ? "alert" : "calm")

    // How far the alert shake swings, in degrees, from its resting tilt.
    property real wobble: 0

    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    Accessible.role: Accessible.Graphic
    Accessible.name: root.boom ? qsTr("Boom") : (!root.showSeconds ? qsTr("Heart bomb") : (root.alert ? qsTr("Heart bomb with %1 seconds left, fuse sparking").arg(root.secondsLeft) : qsTr("Heart bomb with %1 seconds left").arg(root.secondsLeft)))

    SequentialAnimation on wobble {
        running: root.alert && !root.boom
        loops: Animation.Infinite

        NumberAnimation {
            from: -3
            to: 3
            duration: 90
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            from: 3
            to: -3
            duration: 90
            easing.type: Easing.InOutSine
        }
    }

    Item {
        id: bomb

        visible: !root.boom
        width: 120
        height: 120
        scale: root.size / 120
        transformOrigin: Item.TopLeft

        Item {
            id: body

            width: 120
            height: 120
            rotation: root.alert ? -6 + root.wobble : 0
            x: root.alert ? root.wobble * 0.6 : 0

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                // Speed lines: the shake, drawn.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.alert ? Theme.ink : "transparent"
                    strokeWidth: 4
                    capStyle: ShapePath.RoundCap

                    PathSvg {
                        path: "M2 40 l-12 -4 M0 62 l-14 0 M4 84 l-12 6"
                    }
                }

                // The metal cap and its band.
                ShapePath {
                    fillColor: Theme.metal
                    strokeColor: Theme.ink
                    strokeWidth: 4

                    PathSvg {
                        path: "M49 9 H63 A5 5 0 0 1 68 14 V42 A5 5 0 0 1 63 47 H49 A5 5 0 0 1 44 42 V14 A5 5 0 0 1 49 9 Z"
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.ink
                    strokeWidth: 3
                    capStyle: ShapePath.RoundCap

                    PathSvg {
                        path: "M46 20 h20"
                    }
                }

                // The heart.
                ShapePath {
                    objectName: "heartBody"
                    fillColor: root.alert ? Theme.red : Theme.heartPink
                    strokeColor: Theme.ink
                    strokeWidth: 5
                    joinStyle: ShapePath.RoundJoin

                    PathSvg {
                        path: "M56 112 C18 84 4 60 8 42 C12 22 38 14 56 38 C74 14 100 22 104 42 C108 60 94 84 56 112 Z"
                    }
                }

                // The rope fuse: an ink outline with the rope over it.
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.ink
                    strokeWidth: 9
                    capStyle: ShapePath.RoundCap

                    PathSvg {
                        path: "M60 12 q16 -14 30 -3"
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.rope
                    strokeWidth: 5
                    capStyle: ShapePath.RoundCap

                    PathSvg {
                        path: "M60 12 q16 -14 30 -3"
                    }
                }

                // The shine on the heart.
                ShapePath {
                    fillColor: Qt.rgba(1, 1, 1, root.alert ? 0.32 : 0.6)
                    strokeColor: "transparent"

                    PathSvg {
                        path: "M19.1 51.8 A9 5.5 -40 1 0 32.9 40.2 A9 5.5 -40 1 0 19.1 51.8 Z"
                    }
                }

                // The spark at the end of the fuse: orange when calm, bigger and yellow with a red edge in the alert.
                ShapePath {
                    objectName: "spark"
                    fillColor: root.alert ? Theme.background : Theme.orange
                    strokeColor: root.alert ? Theme.red : Theme.ink
                    strokeWidth: root.alert ? 3 : 2.5
                    joinStyle: ShapePath.RoundJoin

                    PathSvg {
                        path: root.alert ? "M96 -11 L100.1 0.3 L112.2 0.7 L102.7 8.2 L106 19.8 L96 13 L86 19.8 L89.3 8.2 L79.8 0.7 L91.9 0.3 Z" : "M94 -5 L97.2 3.6 L106.4 4 L99.2 9.7 L101.6 18.5 L94 13.5 L86.4 18.5 L88.8 9.7 L81.6 4 L90.8 3.6 Z"
                    }
                }

                // The face: two eyes with a glint each, and a smile.
                ShapePath {
                    objectName: "eyes"
                    fillColor: root.face ? Theme.ink : "transparent"
                    strokeColor: "transparent"

                    PathSvg {
                        path: "M35.5 62 A6.5 8.5 0 1 0 48.5 62 A6.5 8.5 0 1 0 35.5 62 Z M63.5 62 A6.5 8.5 0 1 0 76.5 62 A6.5 8.5 0 1 0 63.5 62 Z"
                    }
                }
                ShapePath {
                    fillColor: root.face ? Theme.white : "transparent"
                    strokeColor: "transparent"

                    PathSvg {
                        path: "M41.6 59 A2.4 2.4 0 1 0 46.4 59 A2.4 2.4 0 1 0 41.6 59 Z M69.6 59 A2.4 2.4 0 1 0 74.4 59 A2.4 2.4 0 1 0 69.6 59 Z"
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: root.face ? Theme.ink : "transparent"
                    strokeWidth: 4
                    capStyle: ShapePath.RoundCap

                    PathSvg {
                        path: "M43 80 q13 11 26 0"
                    }
                }
            }

            Text {
                objectName: "bombSeconds"
                visible: root.showSeconds
                x: 10
                y: 49
                width: 92
                horizontalAlignment: Text.AlignHCenter
                text: Theme.formatTime(root.secondsLeft)
                color: root.alert ? Theme.white : Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: 36
                font.letterSpacing: 1
            }
        }
    }

    // The pop: a star burst that springs out where the heart was, with two small hearts flying off.
    Item {
        id: pop

        visible: root.boom
        anchors.centerIn: parent
        width: root.size * 1.4
        height: root.size * 1.4
        scale: root.boom ? 1 : 0.3
        transformOrigin: Item.Center

        Behavior on scale {
            NumberAnimation {
                duration: 320
                easing.type: Easing.OutBack
            }
        }

        StarBurst {
            anchors.fill: parent
            outerColor: Theme.orange
            textColor: Theme.ink
            text: qsTr("BOOM!")
            textSize: 38
            letterSpacing: 1
            spikes: 10
        }

        Repeater {
            model: [
                {
                    "x": -0.02,
                    "y": 0.08,
                    "turn": -20
                },
                {
                    "x": 0.88,
                    "y": 0.78,
                    "turn": 18
                }
            ]

            delegate: Shape {
                id: flyingHeart

                required property var modelData

                x: modelData.x * pop.width
                y: modelData.y * pop.height
                width: 120
                height: 120
                scale: pop.width * 0.17 / 120
                transformOrigin: Item.TopLeft
                rotation: modelData.turn
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    fillColor: Theme.heartPink
                    strokeColor: Theme.ink
                    strokeWidth: 9
                    joinStyle: ShapePath.RoundJoin

                    PathSvg {
                        path: "M56 112 C18 84 4 60 8 42 C12 22 38 14 56 38 C74 14 100 22 104 42 C108 60 94 84 56 112 Z"
                    }
                }
            }
        }
    }
}
