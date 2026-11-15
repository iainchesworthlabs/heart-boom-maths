// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// Everything the window shows: the shell inside the device's safe area, and over it the layer of cues that belong to the
// whole window (the red alert frame and the streak flash), which is not inset. Main.qml is a Window and the tests cannot
// load one, so the composition lives here and both load the same file.
Item {
    id: root

    required property GameController game

    // Everything sits inside the device's safe area (SafeArea.margins, Qt 6.9 and later); the page runs under the edges.
    SafeAreaFrame {
        id: frame

        objectName: "safeAreaFrame"
        anchors.fill: parent
        content: AppShell {
            id: shell

            objectName: "appShell"
            anchors.fill: parent
            game: root.game
        }
    }

    AlertFrame {
        id: windowCues

        objectName: "windowCues"
        anchors.fill: parent
        alert: shell.alertShown
        unsafeBottom: frame.unsafeBottom
    }

    Connections {
        target: shell

        function onStreakStepped() {
            windowCues.flash();
        }
    }
}
