// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// PLACEHOLDER. A bare page so the app can be played from the first screen to the game-over card before the real screens
// exist: a star-burst title, a line of text and the buttons given as children. Each real screen (Title, Level clear, Win,
// Game over and the rest) replaces its use of this in AppShell.qml and this file goes when none is left.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    default property alias actions: actionColumn.data

    HalftoneBackground {
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 420)
        spacing: 24

        StarBurst {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width
            Layout.preferredHeight: parent.width * 150 / 342
            text: root.title
            textSize: 48 * parent.width / 342
            letterSpacing: 3
        }

        Text {
            Layout.fillWidth: true
            visible: root.subtitle !== ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.subtitle
            color: Theme.ink
            font.family: Theme.bodyFont
            font.pixelSize: 22
        }

        ColumnLayout {
            id: actionColumn

            Layout.fillWidth: true
            spacing: 14
        }
    }
}
