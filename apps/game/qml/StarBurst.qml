// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A comic star burst: a spiky outer polygon with a white inner polygon, and a title (and an optional line above it)
// shrunk to fit inside the inner polygon with a little padding.
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
    property int spikes: 12
    // How deep the valleys are, as a share of the spike radius.
    property real valleyDepth: 0.8
    // How far the inner polygon is pulled in, as a share of the outer one.
    property real innerShare: 0.88

    readonly property real centreX: width / 2
    readonly property real centreY: height / 2
    readonly property real outerRadiusX: Math.max(1, width / 2 - outerOutline)
    readonly property real outerRadiusY: Math.max(1, height / 2 - outerOutline)
    readonly property real valleyRadiusX: outerRadiusX * innerShare * valleyDepth - innerOutline / 2 - padding
    readonly property real valleyRadiusY: outerRadiusY * innerShare * valleyDepth - innerOutline / 2 - padding

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

    // A closed polygon through 2 * spikes points, alternating spike and valley, as an SVG path.
    function burstPath(scaleOfOuter) {
        const count = root.spikes * 2;
        let path = "";
        for (let i = 0; i < count; i++) {
            const angle = -Math.PI / 2 + i * Math.PI / root.spikes;
            const reach = (i % 2 === 0 ? 1 : root.valleyDepth) * scaleOfOuter;
            const x = root.centreX + Math.cos(angle) * root.outerRadiusX * reach;
            const y = root.centreY + Math.sin(angle) * root.outerRadiusY * reach;
            path += (i === 0 ? "M " : " L ") + x.toFixed(2) + " " + y.toFixed(2);
        }
        return path + " Z";
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
