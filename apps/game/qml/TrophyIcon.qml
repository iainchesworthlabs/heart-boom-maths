// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The trophy on the TOP 10 button and the Game Center row, drawn on a 28 grid.
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

        // The cup.
        ShapePath {
            fillColor: Theme.coin
            strokeColor: Theme.ink
            strokeWidth: 2.5
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M7 4 H21 V11 A7 7 0 0 1 7 11 Z"
            }
        }
        // The handles.
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.ink
            strokeWidth: 2.5

            PathSvg {
                path: "M7 7 H3 V9 A4 4 0 0 0 7 13 M21 7 H25 V9 A4 4 0 0 1 21 13"
            }
        }
        // The stem and the foot.
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.ink
            strokeWidth: 3
            capStyle: ShapePath.RoundCap

            PathSvg {
                path: "M14 18 V22 M9 25 H19"
            }
        }
    }
}
