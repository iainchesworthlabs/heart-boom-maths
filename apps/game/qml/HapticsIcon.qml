// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A phone with a buzz on each side, the mark of the Haptics row. Drawn on a 34 grid.
Item {
    id: root

    width: 34
    height: 34

    Shape {
        width: 34
        height: 34
        scale: Math.min(root.width, root.height) / 34
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        // The phone.
        ShapePath {
            fillColor: Theme.blue
            strokeColor: Theme.ink
            strokeWidth: 2.5
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M13 4 H21 Q23 4 23 6 V28 Q23 30 21 30 H13 Q11 30 11 28 V6 Q11 4 13 4 Z"
            }
        }
        // The buzz, a short arc on each side.
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.ink
            strokeWidth: 2.5
            capStyle: ShapePath.RoundCap

            PathSvg {
                path: "M6.5 12 Q4.5 17 6.5 22 M27.5 12 Q29.5 17 27.5 22"
            }
        }
    }
}
