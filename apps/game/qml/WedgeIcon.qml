// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// The heavy, short, slanted "›" at the right of ABOUT: a filled wedge pointing right, drawn because the fonts' own glyph
// is not this shape. A 12.8 by 27.7 grid scaled to the item.
Item {
    id: root

    property color color: Theme.white

    width: 12.8
    height: 27.7

    Shape {
        width: 12.8
        height: 27.7
        scale: Math.min(root.width / 12.8, root.height / 27.7)
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.color
            strokeColor: root.color
            strokeWidth: 0.6
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M 3.8 0 L 12.6 9 L 11.3 17 L 0.2 27.7 L 0.6 19.9 L 9.3 13.9 L 4 9 Z"
            }
        }
    }
}
