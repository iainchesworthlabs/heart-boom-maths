// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A comic star burst: a spiky outer polygon with a white inner polygon, and a title (and an optional line above it)
// shrunk to fit inside the inner polygon with a little padding.
//
// The boards draw each burst by hand, with spikes of different lengths and a mirror-symmetric outline, so a `variant`
// names one of those outlines. A point is a pair (u, v): the offset from the centre of the item as a share of the outer
// radius on each axis, so the same outline fills a burst of any size. With no variant the outline is a regular star of
// `spikes` spikes (the tests and a stand-in screen use it).
//
// The text is fitted to the ellipse through the inner polygon's valleys, which is the largest region that every spike
// leaves free, so it can never touch an outline. `fitsInside` and `fitScale` report the result for tests.
Item {
    id: root

    property string text: ""
    property string subText: "" // a small line above the title, optional
    property color outerColor: Theme.blue
    property color innerColor: Theme.white
    property color textColor: Theme.red
    property color subTextColor: Theme.ink
    // The nominal sizes; the text shrinks from these when the burst is too small, and never grows.
    property real textSize: 58
    property real subTextSize: 24
    property real letterSpacing: 3
    // The title leans (degrees; negative leans left). The fit is worked out for the unrotated text, and a lean of a few
    // degrees stays inside the padding.
    property real textRotation: 0
    property real padding: 4 // free space between the text and the inner polygon
    property real outerOutline: 4
    property real innerOutline: 3
    // "", "hero" (alternating spikes, the title), "wide" (fourteen spikes across a wide card), "squashed" (twelve spikes,
    // game over and the win), or "boom" (ten spikes, the bomb going off).
    property string variant: ""
    property int spikes: 12
    // How deep the valleys are, as a share of the spike radius (a regular star only).
    property real valleyDepth: 0.8
    // How far the inner polygon is pulled in, as a share of the outer one.
    property real innerShare: spec !== null ? spec.innerShare : 0.88

    // The hand-drawn outlines. `fit` is the share of the inner polygon on each axis that the text may use.
    readonly property var shapes: ({
            "hero": {
                "innerShare": 0.8,
                "fit": [0.74, 0.74],
                "points": [0, -1, 0.191, -0.715, 0.470, -0.814, 0.524, -0.524, 0.866, -0.500, 0.715, -0.191, 0.940, 0, 0.715, 0.191, 0.866, 0.500, 0.524, 0.524, 0.470, 0.814, 0.191, 0.715, 0, 1, -0.191, 0.715, -0.470, 0.814, -0.524, 0.524, -0.866, 0.500, -0.715, 0.191, -0.940, 0, -0.715, -0.191, -0.866, -0.500, -0.524, -0.524, -0.470, -0.814, -0.191, -0.715]
            },
            "wide": {
                "innerShare": 0.88,
                "fit": [0.8, 0.8],
                "points": [0, -1, 0.178, -0.780, 0.408, -0.846, 0.499, -0.626, 0.782, -0.623, 0.721, -0.348, 0.916, -0.210, 0.800, 0, 0.975, 0.222, 0.721, 0.348, 0.735, 0.587, 0.499, 0.626, 0.434, 0.901, 0.178, 0.780, 0, 0.940, -0.178, 0.780, -0.434, 0.901, -0.499, 0.626, -0.735, 0.587, -0.721, 0.348, -0.975, 0.222, -0.800, 0, -0.916, -0.210, -0.721, -0.348, -0.782, -0.623, -0.499, -0.626, -0.408, -0.846, -0.178, -0.780]
            },
            "squashed": {
                "innerShare": 0.88,
                "fit": [0.8, 0.8],
                "points": [0, -1, 0.207, -0.773, 0.470, -0.814, 0.566, -0.566, 0.866, -0.500, 0.773, -0.207, 0.940, 0, 0.773, 0.207, 0.866, 0.500, 0.566, 0.566, 0.470, 0.814, 0.207, 0.773, 0, 1, -0.207, 0.773, -0.470, 0.814, -0.566, 0.566, -0.866, 0.500, -0.773, 0.207, -0.940, 0, -0.773, -0.207, -0.866, -0.500, -0.566, -0.566, -0.470, -0.814, -0.207, -0.773]
            },
            "boom": {
                "innerShare": 0.86,
                "fit": [0.77, 0.78],
                "points": [0, -1, 0.241, -0.741, 0.546, -0.753, 0.631, -0.459, 0.951, -0.309, 0.780, 0, 0.885, 0.287, 0.631, 0.459, 0.588, 0.809, 0.241, 0.741, 0, 0.930, -0.241, 0.741, -0.588, 0.809, -0.631, 0.459, -0.885, 0.287, -0.780, 0, -0.951, -0.309, -0.631, -0.459, -0.546, -0.753, -0.241, -0.741]
            }
        })
    readonly property var spec: shapes[variant] !== undefined ? shapes[variant] : null

    readonly property real centreX: width / 2
    readonly property real centreY: height / 2
    readonly property real outerRadiusX: Math.max(1, width / 2 - outerOutline)
    readonly property real outerRadiusY: Math.max(1, height / 2 - outerOutline)
    readonly property real fitShareX: spec !== null ? spec.fit[0] : valleyDepth
    readonly property real fitShareY: spec !== null ? spec.fit[1] : valleyDepth
    readonly property real valleyRadiusX: outerRadiusX * innerShare * fitShareX - innerOutline / 2 - padding
    readonly property real valleyRadiusY: outerRadiusY * innerShare * fitShareY - innerOutline / 2 - padding

    // The text block at its nominal size, and the factor that fits it.
    readonly property real contentWidth: lines.implicitWidth
    readonly property real contentHeight: lines.implicitHeight
    readonly property real fitScale: {
        if (contentWidth <= 0 || contentHeight <= 0) {
            return 1;
        }
        const ratio = Math.pow(contentWidth / 2 / valleyRadiusX, 2) + Math.pow(contentHeight / 2 / valleyRadiusY, 2);
        return ratio <= 1 ? 1 : 1 / Math.sqrt(ratio);
    }
    // True when the corners of the scaled text block sit inside the valley ellipse.
    readonly property bool fitsInside: Math.pow(contentWidth * fitScale / 2 / valleyRadiusX, 2) + Math.pow(contentHeight * fitScale / 2 / valleyRadiusY, 2) <= 1.0001

    // The corners of the polygon, as {x, y} in the item's own coordinates: the variant's points, or 2 * spikes points
    // alternating spike and valley. `scaleOfOuter` is 1 for the outer outline and `innerShare` for the inner one.
    function burstPoints(scaleOfOuter) {
        const corners = [];
        if (root.spec !== null) {
            const points = root.spec.points;
            for (let i = 0; i < points.length; i += 2) {
                corners.push({
                    "x": root.centreX + points[i] * root.outerRadiusX * scaleOfOuter,
                    "y": root.centreY + points[i + 1] * root.outerRadiusY * scaleOfOuter
                });
            }
            return corners;
        }
        const count = root.spikes * 2;
        for (let i = 0; i < count; i++) {
            const angle = -Math.PI / 2 + i * Math.PI / root.spikes;
            const reach = (i % 2 === 0 ? 1 : root.valleyDepth) * scaleOfOuter;
            corners.push({
                "x": root.centreX + Math.cos(angle) * root.outerRadiusX * reach,
                "y": root.centreY + Math.sin(angle) * root.outerRadiusY * reach
            });
        }
        return corners;
    }

    function burstPath(scaleOfOuter) {
        return root.burstPoints(scaleOfOuter).map((corner, i) => (i === 0 ? "M " : "L ") + corner.x.toFixed(2) + " " + corner.y.toFixed(2)).join(" ") + " Z";
    }

    implicitWidth: 342
    implicitHeight: 150

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.outerColor
            strokeColor: Theme.ink
            strokeWidth: root.outerOutline
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.burstPath(1)
            }
        }

        ShapePath {
            fillColor: root.innerColor
            strokeColor: Theme.ink
            strokeWidth: root.innerOutline
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.burstPath(root.innerShare)
            }
        }
    }

    Column {
        id: lines

        anchors.centerIn: parent
        spacing: 4
        scale: root.fitScale
        rotation: root.textRotation

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.subText !== ""
            text: root.subText
            color: root.subTextColor
            font.family: Theme.displayFont
            font.pixelSize: root.subTextSize
            font.letterSpacing: 2
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.text
            horizontalAlignment: Text.AlignHCenter
            color: root.textColor
            font.family: Theme.displayFont
            font.pixelSize: root.textSize
            font.letterSpacing: root.letterSpacing
        }
    }
}
