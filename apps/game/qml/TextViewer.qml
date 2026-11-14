// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// A long text in a white card that scrolls: the licence and the third-party notices. A finger drags it, the mouse wheel
// scrolls it, and from a keyboard the arrow, Page Up, Page Down, Home and End keys do. The bar at the right shows where
// the reading is and appears only when the text is longer than the card.
ScreenFrame {
    id: root

    property string text: ""
    readonly property alias flick: flick

    titleSize: 38
    titleSpacing: 2
    designHeight: 600

    ComicCard {
        objectName: "viewerCard"
        Layout.fillWidth: true
        Layout.fillHeight: true
        shadowOffset: Theme.shadowSmall
        borderWidth: Theme.outlineLarge
        radius: Theme.radiusLarge

        Flickable {
            id: flick

            objectName: "viewerFlick"
            anchors.fill: parent
            anchors.margins: 14 * root.unit
            clip: true
            contentWidth: width
            contentHeight: body.height
            boundsBehavior: Flickable.StopAtBounds
            focus: true
            activeFocusOnTab: true

            readonly property real step: 48 * root.unit

            function scrollBy(distance) {
                contentY = Math.max(0, Math.min(contentHeight - height, contentY + distance));
            }

            Keys.onUpPressed: scrollBy(-step)
            Keys.onDownPressed: scrollBy(step)
            Keys.onPressed: event => {
                switch (event.key) {
                case Qt.Key_PageUp:
                    scrollBy(-height * 0.9);
                    break;
                case Qt.Key_PageDown:
                    scrollBy(height * 0.9);
                    break;
                case Qt.Key_Home:
                    contentY = 0;
                    break;
                case Qt.Key_End:
                    contentY = Math.max(0, contentHeight - height);
                    break;
                default:
                    return;
                }
                event.accepted = true;
            }

            Text {
                id: body

                objectName: "viewerText"
                width: flick.width - 12 * root.unit
                text: root.text
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 16 * root.unit
            }
        }

        // Where the reading is.
        Rectangle {
            objectName: "viewerBar"
            visible: flick.contentHeight > flick.height
            anchors.right: parent.right
            anchors.rightMargin: 2
            y: 14 * root.unit + (flick.height - height) * (flick.contentHeight > flick.height ? flick.contentY / (flick.contentHeight - flick.height) : 0)
            width: 6
            height: Math.max(32, flick.height * flick.height / Math.max(1, flick.contentHeight))
            radius: 3
            color: Theme.ink
        }
    }
}
