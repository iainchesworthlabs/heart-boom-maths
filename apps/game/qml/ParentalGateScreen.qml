// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The parental gate (board CP17): a number written in words, which a grown-up types back with the keypad (or a keyboard)
// before a web address opens or Game Center turns on. OPEN stays grey until the number typed is the number asked for.
// The controller owns the number and the check; this screen only shows the words and passes the digits on.
//
// A wrong number is a wrong answer as soon as three digits are typed, or Return is pressed with the wrong number: the
// screen says so in words, with a mark, and the controller deals a new number. Nothing opens and nothing is kept.
ScreenFrame {
    id: root

    required property GameController game

    // The digits typed so far, and whether the last number typed was wrong.
    property string typed: ""
    property bool failed: false

    readonly property bool complete: root.game.gateMatches(root.typed)
    readonly property bool forGameCenter: root.game.gatePurpose === GameController.GameCenter
    readonly property string prompt: {
        if (root.forGameCenter) {
            return qsTr("To turn on Game Center, type this number:");
        }
        if (root.game.gateUrl === root.game.privacyPolicyUrl) {
            return qsTr("To open the privacy policy, type this number:");
        }
        if (root.game.gateUrl === root.game.repositoryUrl) {
            return qsTr("To open the source code, type this number:");
        }
        return qsTr("To open this link, type this number:");
    }

    title: qsTr("ASK A GROWN-UP")
    titleSize: 38
    titleSpacing: 2
    backName: qsTr("Cancel")
    designHeight: 780
    columnSpacing: 14

    function press(digit) {
        if (root.typed.length >= root.game.gateDigits) {
            return;
        }
        root.failed = false;
        root.typed += digit;
        // A number of the right length that is not the number asked for is a wrong answer now: the keypad cannot be used to
        // work out the number a digit at a time, and OPEN never has to explain why it stays grey.
        if (root.typed.length === root.game.gateDigits && !root.complete) {
            root.submit();
        }
    }

    function erase() {
        root.typed = root.typed.slice(0, -1);
    }

    function submit() {
        // Fewer digits than the number has are not an answer yet, so Return on a half-typed number does nothing.
        if (root.typed.length < root.game.gateDigits) {
            return;
        }
        if (!root.game.submitGate(root.typed)) {
            root.failed = true;
            root.typed = "";
        }
    }

    Keys.onPressed: event => {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            root.press(String(event.key - Qt.Key_0));
        } else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
            root.erase();
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.submit();
        } else {
            return;
        }
        event.accepted = true;
    }

    ComicCard {
        objectName: "gatePrompt"
        Layout.fillWidth: true
        Layout.preferredHeight: promptColumn.implicitHeight + 32 * root.unit + 2 * borderWidth
        shadowOffset: Theme.shadowSmall

        ColumnLayout {
            id: promptColumn

            anchors.fill: parent
            anchors.topMargin: 16 * root.unit
            anchors.bottomMargin: 16 * root.unit
            anchors.leftMargin: 18 * root.unit
            anchors.rightMargin: 18 * root.unit
            spacing: 10 * root.unit

            Text {
                objectName: "gatePromptText"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.prompt
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 20 * root.unit
                lineHeight: 24 * root.unit
                lineHeightMode: Text.FixedHeight
            }

            Text {
                objectName: "gateWords"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.game.gateWords.toUpperCase()
                color: Theme.red
                font.family: Theme.displayFont
                font.pixelSize: 34 * root.unit
                font.letterSpacing: 2 * root.unit
                lineHeight: 35.7 * root.unit
                lineHeightMode: Text.FixedHeight
            }
        }
    }

    // What has been typed, with the caret after it. (The text's own letter spacing follows its last digit, so the caret is
    // pulled back by as much.)
    ComicCard {
        objectName: "gateDisplay"
        Layout.fillWidth: true
        Layout.preferredHeight: 68 * root.unit
        borderWidth: Theme.outlineLarge
        radius: 18
        shadowOffset: Theme.shadowSmall
        Accessible.role: Accessible.StaticText
        Accessible.name: root.typed === "" ? qsTr("Nothing typed yet") : qsTr("Typed: %1").arg(root.typed.split("").join(" "))

        Row {
            anchors.centerIn: parent
            spacing: -6 * root.unit

            Text {
                objectName: "gateTyped"
                text: root.typed
                color: Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: 44 * root.unit
                font.letterSpacing: 6 * root.unit
            }
            Text {
                text: "|"
                color: Theme.red
                font.family: Theme.displayFont
                font.pixelSize: 44 * root.unit
            }
        }
    }

    GridLayout {
        objectName: "gateKeypad"
        Layout.fillWidth: true
        columns: 3
        columnSpacing: 10 * root.unit
        rowSpacing: 10 * root.unit
        uniformCellWidths: true

        Repeater {
            model: ["1", "2", "3", "4", "5", "6", "7", "8", "9"]

            delegate: ComicButton {
                id: key

                required property string modelData

                objectName: "key" + modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 60 * root.unit
                variant: "secondary"
                text: modelData
                pixelSize: 34 * root.unit
                letterSpacing: 1
                borderWidth: Theme.outlineSmall
                radius: 16
                shadowOffset: 4
                onClicked: root.press(modelData)
            }
        }

        Item {
            Layout.fillWidth: true
        }

        ComicButton {
            objectName: "key0"
            Layout.fillWidth: true
            Layout.preferredHeight: 60 * root.unit
            variant: "secondary"
            text: "0"
            pixelSize: 34 * root.unit
            letterSpacing: 1
            borderWidth: Theme.outlineSmall
            radius: 16
            shadowOffset: 4
            onClicked: root.press("0")
        }

        ComicButton {
            objectName: "keyDelete"
            Layout.fillWidth: true
            Layout.preferredHeight: 60 * root.unit
            variant: "secondary"
            text: qsTr("DEL")
            accessibleName: qsTr("Delete the last digit")
            pixelSize: 24 * root.unit
            letterSpacing: 1
            borderWidth: Theme.outlineSmall
            radius: 16
            shadowOffset: 4
            onClicked: root.erase()
        }
    }

    // The room between the keypad and the buttons, where a wrong number is told. It is kept free when there is nothing to
    // tell, so nothing above it moves.
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        // A mark and words, not a colour alone.
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8 * root.unit

            Rectangle {
                objectName: "gateFailedMark"
                visible: root.failed
                Layout.preferredWidth: 26 * root.unit
                Layout.preferredHeight: 26 * root.unit
                radius: width / 2
                color: Theme.ink

                Text {
                    anchors.centerIn: parent
                    text: "!"
                    color: Theme.white
                    font.family: Theme.displayFont
                    font.pixelSize: 22 * root.unit
                }
            }

            Text {
                objectName: "gateFailedText"
                visible: root.failed
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Not that number. Here is a new one.")
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: 20 * root.unit
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }
    }

    // CANCEL and OPEN share what is left in 1 to 1.4, after each has the room its border and padding always take: the
    // board's two widths are 140 and 188 of 342. They are placed by hand because a layout's stretch factors do not give
    // this split.
    Item {
        id: buttons

        readonly property real gap: 14 * root.unit
        readonly property real fixedWidth: 22 * root.unit
        readonly property real shared: width - gap - 2 * fixedWidth
        readonly property real cancelWidth: fixedWidth + shared / 2.4

        Layout.fillWidth: true
        Layout.preferredHeight: 60 * root.unit

        ComicButton {
            objectName: "cancelButton"
            width: buttons.cancelWidth
            height: parent.height
            variant: "secondary"
            text: qsTr("CANCEL")
            pixelSize: 32 * root.unit
            letterSpacing: 2
            borderWidth: Theme.outlineLarge
            radius: 18
            shadowOffset: Theme.shadowSmall
            onClicked: root.backClicked()
        }

        ComicButton {
            objectName: "openButton"
            x: buttons.cancelWidth + buttons.gap
            width: buttons.width - x
            height: parent.height
            enabled: root.complete
            fillColor: enabled ? variantFill : Theme.switchOff
            textColor: enabled ? variantText : Theme.lockedText
            variant: "primary"
            text: root.forGameCenter ? qsTr("TURN ON") : qsTr("OPEN")
            pixelSize: 36 * root.unit
            letterSpacing: 3
            borderWidth: Theme.outlineLarge
            radius: 18
            shadowOffset: Theme.shadowSmall
            onClicked: root.submit()
        }
    }
}
