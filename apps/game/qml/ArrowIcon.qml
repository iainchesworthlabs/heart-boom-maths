// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A right-pointing arrow, drawn because neither font has one. A 24 by 14 grid scaled to the item.
Item {
    id: root

    property color color: Theme.ink
    property real strokeWidth: 3

    width: 24
    height: 14

    Shape {
        width: 24
        height: 14
        scale: Math.min(root.width / 24, root.height / 14)
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M2 7 H21 M15 1.5 L21 7 L15 12.5"
            }
        }
    }
}
