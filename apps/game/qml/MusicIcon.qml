// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// Two pink notes joined by a beam, the mark of the Music row. Drawn on a 34 grid.
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
            fillColor: "transparent"
            strokeColor: Theme.ink
            strokeWidth: 2.5
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M13 24 V6 L26 3 V21"
            }
        }
        ShapePath {
            fillColor: Theme.heartPink
            strokeColor: Theme.ink
            strokeWidth: 2.5

            PathSvg {
                path: "M4.5 25 A5 4 0 1 0 14.5 25 A5 4 0 1 0 4.5 25 Z"
            }
        }
        ShapePath {
            fillColor: Theme.heartPink
            strokeColor: Theme.ink
            strokeWidth: 2.5

            PathSvg {
                path: "M17.5 22 A5 4 0 1 0 27.5 22 A5 4 0 1 0 17.5 22 Z"
            }
        }
    }
}
