// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The on-screen keyboard of the name screen, for devices with a touch screen: A to Z, a space bar and delete, which are all a
// name can hold. It fills the grey area the board reserves for a keyboard. It is drawn here, not left to the system
// keyboard, so a child can only type what a name may hold and the same keys appear on every platform. A hardware keyboard
// types into the same field (NameEntryScreen).
//
// The keys only say what was tapped; the name screen decides what that does.
Rectangle {
    id: root

    // False when the name is as long as it may be: the letters and the space bar then go grey, a second cue next to the
    // counter on the field.
    property bool lettersEnabled: true
    // The rows of letters, top to bottom.
    property var rows: ["QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"]

    signal letterTyped(string letter)
    signal deleteTyped

    readonly property real gap: Math.max(4, Math.min(8, width / 60))
    readonly property real sideMargin: Math.max(6, gap)
    readonly property real keyWidth: (Math.min(width, 640) - 2 * sideMargin - 9 * gap) / 10
    readonly property real keyHeight: Math.max(44, Math.min(64, (height - 4 - 2 * sideMargin - 3 * gap) / 4))

    // The board draws the keyboard area in this light grey, with an ink line above it.
    color: "#D1D5DB"

    Rectangle {
        width: parent.width
        height: 4
        color: Theme.ink
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 4 + root.sideMargin
        spacing: root.gap

        Repeater {
            model: root.rows

            Row {
                id: letterRow

                required property string modelData
                required property int index

                anchors.horizontalCenter: parent.horizontalCenter
                spacing: root.gap

                Repeater {
                    model: letterRow.modelData.split("")

                    Key {
                        required property string modelData

                        width: root.keyWidth
                        height: root.keyHeight
                        label: modelData
                        enabled: root.lettersEnabled
                        onTapped: root.letterTyped(modelData)
                    }
                }

                // Delete sits at the end of the last row.
                Repeater {
                    model: letterRow.index === root.rows.length - 1 ? 1 : 0

                    Key {
                        objectName: "deleteKey"
                        width: root.keyWidth * 1.6 + root.gap * 0.6
                        height: root.keyHeight
                        label: qsTr("DEL")
                        accessibleName: qsTr("Delete")
                        labelSize: 20
                        onTapped: root.deleteTyped()
                    }
                }
            }
        }

        Key {
            objectName: "spaceKey"
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.keyWidth * 6 + root.gap * 5
            height: root.keyHeight
            label: qsTr("SPACE")
            accessibleName: qsTr("Space")
            labelSize: 22
            enabled: root.lettersEnabled
            onTapped: root.letterTyped(" ")
        }
    }

    // One key: a white cap with the ink outline and a small hard shadow.
    component Key: Item {
        id: key

        property string label: ""
        property string accessibleName: ""
        property real labelSize: 28

        signal tapped

        objectName: "key" + label
        Accessible.role: Accessible.Button
        Accessible.name: key.accessibleName !== "" ? key.accessibleName : key.label
        Accessible.focusable: true
        Accessible.onPressAction: if (key.enabled)
            key.tapped()

        Rectangle {
            x: 3
            y: 3
            width: key.width
            height: key.height
            radius: 9
            color: Theme.ink
        }

        Rectangle {
            width: key.width
            height: key.height
            radius: 9
            color: key.enabled ? Theme.white : Theme.disabledFill
            border.width: 3
            border.color: Theme.ink
            // A press sinks the key towards its shadow, as the buttons do.
            transform: Translate {
                x: tap.pressed ? 2 : 0
                y: tap.pressed ? 2 : 0
            }

            Text {
                anchors.centerIn: parent
                text: key.label
                color: key.enabled ? Theme.ink : Theme.disabledText
                font.family: Theme.displayFont
                font.pixelSize: key.labelSize
            }
        }

        TapHandler {
            id: tap

            enabled: key.enabled
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: {
                Haptics.tap();
                key.tapped();
            }
        }
    }
}
