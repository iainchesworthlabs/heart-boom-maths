// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A chevron that points left, the back button's mark. Turn it half a circle for one that points right. Drawn on a 22 grid.
Item {
    id: root

    property color color: Theme.ink
    property real strokeWidth: 4

    width: 22
    height: 22

    Shape {
        width: 22
        height: 22
        scale: Math.min(root.width, root.height) / 22
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M14 3 L6 11 L14 19"
            }
        }
    }
}
