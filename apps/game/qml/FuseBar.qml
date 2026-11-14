// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The fuse: a bar that shrinks with the time left, with a spark star at the burning end. The last stretch, the alert
// zone, is striped so it can be told from the rest without colour; in the alert the whole fuse turns red as well.
//
// It draws what it is given. The fractions come from the controller (ticksLeft and ticksTotal, and alertTicks, which is
// a different share of the bar in endless levels).
Item {
    id: root

    // Time left as a share of the level's time, 0 to 1.
    property real fraction: 1
    // The alert zone's share of the whole bar, 0 to 1.
    property real alertFraction: 1 / 6
    property bool alert: false
    // A stretch just lost to a wrong answer, drawn in red beyond the burning end, as a share of the bar.
    property real lostFraction: 0
    property int secondsLeft: 0

    property real borderWidth: Theme.outlineSmall
    property real zoneBorderWidth: 3
    property real stripePeriod: 12
    property real starSize: 18

    readonly property real trackWidth: Math.max(0, width - 2 * borderWidth)
    readonly property real trackHeight: Math.max(0, height - 2 * borderWidth)
    readonly property real fillWidth: trackWidth * Math.max(0, Math.min(1, fraction))
    readonly property real zoneWidth: trackWidth * Math.max(0, Math.min(1, alertFraction))

    function stripePath(zoneW, zoneH, period) {
        const spacing = period * Math.SQRT2;
        const thickness = spacing / 2;
        let path = "";
        for (let x = -zoneH; x < zoneW; x += spacing) {
            path += "M " + x + " " + zoneH + " L " + (x + thickness) + " " + zoneH + " L " + (x + thickness + zoneH) + " 0 L " + (x + zoneH) + " 0 Z ";
        }
        return path;
    }

    implicitHeight: 26

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Time left")
    Accessible.description: qsTr("%1 seconds").arg(root.secondsLeft)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.white
    }

    Item {
        id: track

        x: root.borderWidth
        y: root.borderWidth
        width: root.trackWidth
        height: root.trackHeight
        clip: true

        Rectangle {
            objectName: "fuseFill"
            width: root.fillWidth
            height: parent.height
            color: root.alert ? Theme.red : Theme.orange
        }

        // The time a wrong answer just took.
        Rectangle {
            objectName: "fuseLost"
            x: root.fillWidth
            width: Math.min(root.trackWidth * root.lostFraction, root.trackWidth - root.fillWidth)
            height: parent.height
            visible: root.lostFraction > 0
            color: Theme.red
        }

        Item {
            objectName: "fuseZone"
            width: root.zoneWidth
            height: parent.height
            clip: true

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    fillColor: root.alert ? Qt.rgba(1, 1, 1, 0.55) : Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.6)
                    strokeColor: "transparent"

                    PathSvg {
                        path: root.stripePath(root.zoneWidth, root.trackHeight, root.stripePeriod)
                    }
                }
            }

            Rectangle {
                anchors.right: parent.right
                width: root.zoneBorderWidth
                height: parent.height
                color: Theme.ink
            }
        }

        SparkStar {
            objectName: "fuseSpark"
            x: root.fillWidth - width / 2
            width: root.starSize
            height: root.starSize
            // The red stretch a wrong answer took replaces the star while it shows, as on the board.
            visible: root.fraction > 0 && root.lostFraction <= 0
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: "transparent"
        border.width: root.borderWidth
        border.color: Theme.ink
    }
}
