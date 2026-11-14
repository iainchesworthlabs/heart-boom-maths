// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window

// A stub that proves the application, its fonts and its QML module build and start. The screens replace it from M3.
Window {
    id: root

    width: 390
    height: 844
    visible: true
    title: qsTr("Heart Boom Maths")
    color: "white"

    FontLoader {
        id: titleFont

        source: "qrc:/heart_boom_maths/fonts/Bangers-Regular.ttf"
    }

    Text {
        anchors.centerIn: parent
        text: root.title
        color: "black"
        font.family: titleFont.name
        font.pixelSize: 48
    }
}
