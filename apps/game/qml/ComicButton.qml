// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The one button of the game. Three looks: primary (blue), danger (red) and secondary (white); a button that is not
// enabled turns grey whatever its variant. It answers a tap, a click and the Return, Enter and Space keys, takes focus
// from the keyboard with a visible ring, and tells a screen reader its name and that it is a button.
//
// Size it with width and height. It never reports less than 44 in either direction as its implicit size, and a layout
// that sets a smaller one is a mistake the ui tests catch.
Item {
    id: root

    // The label, drawn in capitals as the boards show it. Leave it empty and put an icon in as a child instead.
    property string text: ""
    // What a screen reader says. Defaults to the text; give one for an icon.
    property string accessibleName: ""
    // "primary", "danger" or "secondary".
    property string variant: "primary"

    property bool displayLabel: true // true: the heading font (Bangers). false: the body font (Lilita One).
    property real pixelSize: 17
    property real letterSpacing: 0
    property real borderWidth: Theme.outlineSmall
    property real radius: Theme.radiusSmall
    property real shadowOffset: 0
    property real horizontalPadding: 18

    // Overrides of the variant's colours, when a screen needs one.
    property color fillColor: root.enabled ? variantFill : Theme.disabledFill
    property color textColor: root.enabled ? variantText : Theme.disabledText

    readonly property color variantFill: variant === "danger" ? Theme.red : (variant === "secondary" ? Theme.white : Theme.blue)
    readonly property color variantText: variant === "secondary" ? Theme.ink : Theme.white
    readonly property bool pressed: tap.pressed
    readonly property real minimumHitSize: 44

    default property alias content: custom.data

    signal clicked

    implicitWidth: Math.max(minimumHitSize, label.implicitWidth + 2 * horizontalPadding + 2 * borderWidth)
    implicitHeight: Math.max(minimumHitSize, label.implicitHeight + 2 * borderWidth)
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: root.accessibleName !== "" ? root.accessibleName : root.text
    Accessible.focusable: true
    Accessible.onPressAction: if (root.enabled)
        root.clicked()

    Keys.onPressed: event => {
        if (root.enabled && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)) {
            event.accepted = true;
            root.clicked();
        }
    }

    // The keyboard focus ring.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        visible: root.activeFocus
        radius: root.radius + 6
        color: "transparent"
        border.width: 3
        border.color: Theme.ink
    }

    ComicCard {
        id: face

        anchors.fill: parent
        fillColor: root.fillColor
        borderWidth: root.borderWidth
        radius: root.radius
        shadowOffset: root.shadowOffset
        // A press sinks the button towards its shadow, a second cue besides colour.
        transform: Translate {
            x: root.pressed ? root.shadowOffset / 2 : 0
            y: root.pressed ? root.shadowOffset / 2 : 0
        }

        Text {
            id: label

            anchors.centerIn: parent
            visible: root.text !== ""
            text: root.text
            color: root.textColor
            font.family: root.displayLabel ? Theme.displayFont : Theme.bodyFont
            font.pixelSize: root.pixelSize
            font.letterSpacing: root.letterSpacing
        }

        Item {
            id: custom

            anchors.fill: parent
        }
    }

    TapHandler {
        id: tap

        enabled: root.enabled
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.clicked()
    }
}
