// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The two bars of a pause button, drawn on an 18 by 18 grid and scaled to the item.
Item {
    id: root

    property color color: Theme.ink

    width: 18
    height: 18

    Shape {
        width: 18
        height: 18
        scale: root.width / 18
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.color
            strokeWidth: 3.5
            capStyle: ShapePath.RoundCap

            PathSvg {
                path: "M6 3 V15 M12 3 V15"
            }
        }
    }
}
