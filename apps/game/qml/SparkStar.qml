// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The ten-point star that rides the burning end of the fuse bar.
Item {
    id: root

    property color fillColor: Theme.background
    property color strokeColor: Theme.ink
    property real strokeWidth: 2.5

    // Drawn on a 28 by 28 grid and scaled to the item.
    width: 18
    height: 18

    Shape {
        width: 28
        height: 28
        scale: root.width / 28
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.fillColor
            strokeColor: root.strokeColor
            strokeWidth: root.strokeWidth
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M14 0 L18 10 L28 8 L21 16 L27 25 L17 22 L14 28 L11 22 L1 25 L7 16 L0 8 L10 10 Z"
            }
        }
    }
}
