// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A loudspeaker with two sound waves, the mark of the Sound effects row. Drawn on a 34 grid.
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

        ShapePath {
            fillColor: Theme.blue
            strokeColor: Theme.ink
            strokeWidth: 2.5
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M5 13 H11 L19 6 V28 L11 21 H5 Z"
            }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.ink
            strokeWidth: 2.5
            capStyle: ShapePath.RoundCap

            PathSvg {
                path: "M23 12 Q27 17 23 22 M27 8 Q35 17 27 26"
            }
        }
    }
}
