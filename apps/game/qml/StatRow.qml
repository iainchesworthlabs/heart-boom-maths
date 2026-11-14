// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// One line of a score card: a label on the left and its number on the right. The two texts have the objectNames of the row
// with "Label" and "Value" added, so a test reads them by name.
RowLayout {
    id: root

    property string label: ""
    property string value: ""
    property color textColor: Theme.ink
    property real pixelSize: 22
    // The heading font (Bangers) for the total at the foot of a card; the body font otherwise.
    property bool display: false
    property real letterSpacing: display ? 1 : 0
    // Extra content between the label and the number, such as a NEW BEST! badge.
    default property alias extra: extraSlot.data

    spacing: 8

    Text {
        objectName: root.objectName + "Label"
        Layout.alignment: Qt.AlignBaseline
        text: root.label
        color: root.textColor
        font.family: root.display ? Theme.displayFont : Theme.bodyFont
        font.pixelSize: root.pixelSize
        font.letterSpacing: root.letterSpacing
    }

    RowLayout {
        id: extraSlot

        Layout.alignment: Qt.AlignVCenter
    }

    Item {
        Layout.fillWidth: true
    }

    Text {
        objectName: root.objectName + "Value"
        Layout.alignment: Qt.AlignBaseline
        text: root.value
        color: root.textColor
        font.family: root.display ? Theme.displayFont : Theme.bodyFont
        font.pixelSize: root.pixelSize
        font.letterSpacing: root.letterSpacing
    }
}
