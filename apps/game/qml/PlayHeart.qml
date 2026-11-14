// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The heart bomb of the play screen with the red "-3 s" flash that a wrong answer pins to its corner. The flash is the
// visual twin of the soft buzz.
Item {
    id: root

    property real size: 120
    property int secondsLeft: 0
    property bool alert: false
    property bool boom: false
    // Shown while it has a value, for example "-3 s".
    property string flashText: ""

    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    HeartBomb {
        id: bomb

        objectName: "heartBomb"
        anchors.fill: parent
        size: root.size
        secondsLeft: root.secondsLeft
        alert: root.alert
        boom: root.boom
    }

    Badge {
        objectName: "timeFlash"
        visible: root.flashText !== ""
        x: root.width - width + 6 * root.size / 120
        y: -6 * root.size / 120
        rotation: 8
        text: root.flashText
        fillColor: Theme.red
        textColor: Theme.white
        pixelSize: 26 * root.size / 120 * (root.size > 150 ? 0.8 : 1)
        horizontalPadding: 8
        scale: visible ? 1 : 0.5

        Behavior on scale {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutBack
            }
        }
    }
}
