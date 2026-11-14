// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window

// The application window. The controller is created by the application and given to this window as an initial property;
// all navigation is in AppShell.qml.
Window {
    id: root

    required property GameController game

    width: 390
    height: 844
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr("Heart Boom Maths")
    color: Theme.background

    // Everything sits inside the device's safe area (SafeArea.margins, Qt 6.9 and later); the page runs under the edges.
    SafeAreaFrame {
        objectName: "safeAreaFrame"
        anchors.fill: parent
        content: AppShell {
            objectName: "appShell"
            anchors.fill: parent
            game: root.game
        }
    }
}
