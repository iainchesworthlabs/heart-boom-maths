// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// One row of Settings: an icon (the child of the row), a title with a line under it, and the switch. The switch shows
// `checked` and says `toggleRequested` when tapped; the screen decides what happens.
ComicCard {
    id: root

    property string title: ""
    property string hint: ""
    property bool checked: false
    // The name of the object the tests and the switch's reader find it by.
    property string switchObjectName: ""
    property real unit: 1

    // The icon goes in this box.
    default property alias icon: iconBox.data

    signal toggleRequested

    borderWidth: Theme.outlineLarge
    radius: Theme.radiusLarge
    shadowOffset: Theme.shadowSmall
    implicitHeight: Math.max(toggle.height, textColumn.implicitHeight) + 28 * root.unit + 2 * borderWidth

    Item {
        anchors.fill: parent
        anchors.margins: 14 * root.unit + root.borderWidth

        Item {
            id: iconBox

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 34 * root.unit
            height: 34 * root.unit
        }

        Column {
            id: textColumn

            anchors.left: iconBox.right
            anchors.leftMargin: 14 * root.unit
            anchors.right: toggle.left
            anchors.rightMargin: 8 * root.unit
            anchors.verticalCenter: parent.verticalCenter

            Text {
                objectName: "settingTitle"
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 22 * root.unit
            }
            Text {
                objectName: "settingHint"
                width: parent.width
                text: root.hint
                // A note may take a second line for the words a grown-up needs; the board's one-line notes are unchanged.
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 15 * root.unit
            }
        }

        ComicSwitch {
            id: toggle

            objectName: root.switchObjectName
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 100 * root.unit
            height: 50 * root.unit
            checked: root.checked
            accessibleName: root.title
            onClicked: root.toggleRequested()
        }
    }
}
