// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The fuse bar of the play screen and the green "+5 s" tag that a right answer hangs under its burning end.
Item {
    id: root

    required property PlayMetrics metrics
    property real fraction: 1
    property real alertFraction: 1 / 6
    property bool alert: false
    property real lostFraction: 0
    property int secondsLeft: 0
    // Shown while it has a value, for example "+5 s".
    property string gainText: ""

    implicitHeight: metrics.fuseHeight

    FuseBar {
        id: bar

        objectName: "fuseBar"
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.metrics.fuseHeight
        fraction: root.fraction
        alertFraction: root.alertFraction
        alert: root.alert
        lostFraction: root.lostFraction
        secondsLeft: root.secondsLeft
        borderWidth: root.metrics.fuseBorder
        zoneBorderWidth: root.metrics.fuseZoneBorder
        stripePeriod: root.metrics.stripePeriod
        starSize: root.metrics.starSize
    }

    Badge {
        objectName: "timeGain"
        visible: root.gainText !== ""
        x: Math.max(0, Math.min(root.width - width, bar.borderWidth + bar.fillWidth - 34))
        y: bar.height + 5
        z: 2
        rotation: -4
        text: root.gainText
        fillColor: Theme.green
        textColor: Theme.ink
        pixelSize: root.metrics.mode === "compact" ? 28 : 34
        horizontalPadding: 12
        scale: visible ? 1 : 0.5

        Behavior on scale {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutBack
            }
        }
    }
}
