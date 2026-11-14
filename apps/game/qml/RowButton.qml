// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// A button that spans the width with its label on the left and, on the right, either a small line of text ("asks a
// grown-up first") or an arrow. ABOUT in Settings and the rows of the About screen are this.
ComicButton {
    id: root

    property string label: ""
    // Small text at the right edge.
    property string hint: ""
    property bool arrow: false
    property real hintPixelSize: 16
    property real unit: 1

    // What a screen reader says: the label, then the small text, since it tells the child what the button will do.
    accessibleName: root.hint === "" ? root.label : qsTr("%1, %2").arg(root.label).arg(root.hint)

    Text {
        objectName: "rowLabel"
        anchors.left: parent.left
        anchors.leftMargin: root.horizontalPadding * root.unit
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.textColor
        font.family: root.displayLabel ? Theme.displayFont : Theme.bodyFont
        font.pixelSize: root.pixelSize
        font.letterSpacing: root.letterSpacing
    }

    Text {
        objectName: "rowHint"
        anchors.right: parent.right
        anchors.rightMargin: root.horizontalPadding * root.unit
        anchors.verticalCenter: parent.verticalCenter
        visible: root.hint !== ""
        text: root.hint
        color: root.textColor
        font.family: Theme.bodyFont
        font.pixelSize: root.hintPixelSize
    }

    WedgeIcon {
        objectName: "rowArrow"
        anchors.right: parent.right
        anchors.rightMargin: root.horizontalPadding * root.unit
        anchors.verticalCenter: parent.verticalCenter
        visible: root.arrow
        width: 12.8 * root.unit
        height: 27.7 * root.unit
        color: root.textColor
    }
}
