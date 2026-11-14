// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The settings gear, drawn on a 30 by 30 grid and scaled to the item.
Item {
    id: root

    property color color: Theme.ink

    width: 30
    height: 30

    Shape {
        width: 30
        height: 30
        scale: root.width / 30
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.color
            strokeColor: root.color
            strokeWidth: 2
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M17.5 2.2 L20.6 6.6 L25.8 7.8 L24.9 13 L27.8 17.5 L23.4 20.6 L22.2 25.8 L17 24.9 L12.5 27.8 L9.4 23.4 L4.2 22.2 L5.1 17 L2.2 12.5 L6.6 9.4 L7.8 4.2 L13 5.1 Z"
            }
        }
        ShapePath {
            fillColor: Theme.white
            strokeColor: "transparent"

            PathSvg {
                path: "M10 15 A5 5 0 1 0 20 15 A5 5 0 1 0 10 15 Z"
            }
        }
    }
}
