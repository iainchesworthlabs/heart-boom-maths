// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A cloud, the mark of the iCloud sync row. Drawn on a 28 grid in the same ink outline as the other Settings icons.
Item {
    id: root

    width: 28
    height: 28

    Shape {
        width: 28
        height: 28
        scale: Math.min(root.width, root.height) / 28
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: Theme.blue
            strokeColor: Theme.ink
            strokeWidth: 2.5
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M7 23 A5 5 0 0 1 7 13 A7 7 0 0 1 20 11 A6 6 0 0 1 21 23 Z"
            }
        }
    }
}
