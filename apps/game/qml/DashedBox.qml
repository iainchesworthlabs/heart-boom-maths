// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A rounded box with a dashed outline drawn inside its bounds, as a CSS border would be. Used for the question's "?" slot,
// the ghost of a tile a hint removed and the dashed multiplier badge. A dash is also a shape cue: a dashed edge reads as
// "empty" or "changed" without relying on colour.
Item {
    id: root

    property color lineColor: Theme.ink
    property real lineWidth: Theme.outlineSmall
    property real radius: Theme.radiusSmall
    property color fillColor: "transparent"
    // Dash and gap lengths, in multiples of the line width.
    property var dashPattern: [2, 1.2]

    // The path of a rounded rectangle, pulled in by half a line so the stroke sits inside the box.
    readonly property string outlinePath: {
        const inset = lineWidth / 2;
        const w = Math.max(0, width - lineWidth);
        const h = Math.max(0, height - lineWidth);
        const r = Math.max(0, Math.min(radius - inset, w / 2, h / 2));
        return "M " + (inset + r) + " " + inset + " h " + (w - 2 * r) + " a " + r + " " + r + " 0 0 1 " + r + " " + r + " v " + (h - 2 * r) + " a " + r + " " + r + " 0 0 1 " + (-r) + " " + r + " h " + (-(w - 2 * r)) + " a " + r + " " + r + " 0 0 1 " + (-r) + " " + (-r) + " v " + (-(h - 2 * r)) + " a " + r + " " + r + " 0 0 1 " + r + " " + (-r) + " Z";
    }

    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: root.lineWidth
            fillColor: root.fillColor
            strokeStyle: ShapePath.DashLine
            dashPattern: root.dashPattern
            capStyle: ShapePath.FlatCap

            PathSvg {
                path: root.outlinePath
            }
        }
    }
}
