// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window

// The application window. The controller is created by the application and given to this window as an initial property;
// all navigation is in AppShell.qml and the window's layers are in AppRoot.qml.
Window {
    id: root

    required property GameController game

    width: 390
    height: 844
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    // Qt 6.9 and later keep an iOS window inside the safe area unless it asks to be expanded under the cut-out and the home
    // indicator; SafeAreaFrame then lays the content out inside SafeArea.margins and the page runs under the edges. Other
    // platforms ignore the hint or already draw edge to edge, and a desktop window keeps its title bar.
    flags: Qt.platform.os === "ios" ? (Qt.Window | Qt.ExpandedClientAreaHint) : Qt.Window
    title: qsTr("Heart Boom Maths")
    color: Theme.background

    // The shell sits inside the device's safe area (SafeArea.margins, Qt 6.9 and later) and the page runs under the edges;
    // the red alert frame and the streak flash cover the whole window. AppRoot.qml puts them together.
    AppRoot {
        objectName: "appRoot"
        anchors.fill: parent
        game: root.game
    }
}
