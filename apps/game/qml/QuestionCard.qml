// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick

// The question on a white card: the sentence with its operators in blue and a dashed red "?" slot where the answer goes.
// A sentence of two terms reads on one line; three and four terms read on two, the terms and then "= ?", and a line that
// is still too wide for the card shrinks to fit. When `slotValue` is set, the answer has landed in the slot and the slot
// turns green with a tick (the held look of a right answer).
//
// The parts come from the controller (`questionParts`) or from the held answer; this item decides nothing about them.
ComicCard {
    id: root

    property var parts: []
    property string slotValue: ""

    property real fontSize: 52
    property real multiLineFontSize: 42
    property real slotWidth: 64
    property real slotHeight: 76
    property real slotBorder: 4
    property real slotRadius: 14
    property real partSpacing: 10
    property real rowSpacing: 8
    property real contentPadding: 12
    property real verticalPadding: 26

    readonly property bool multiLine: parts.length > 5
    readonly property real activeFontSize: multiLine ? multiLineFontSize : fontSize
    readonly property var rows: multiLine ? [parts.slice(0, parts.length - 2), parts.slice(parts.length - 2)] : [parts]
    readonly property string spoken: {
        const words = {
            "+": qsTr("plus"),
            "-": qsTr("minus"),
            "=": qsTr("equals"),
            "?": qsTr("what")
        };
        return parts.map(part => words[part] !== undefined ? words[part] : part).join(" ");
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: root.spoken

    // The height of the sentence, so a screen that sizes the card to its text can add its own padding.
    readonly property real contentHeight: sentence.implicitHeight

    Column {
        id: sentence

        anchors.centerIn: parent
        spacing: root.rowSpacing

        Repeater {
            model: root.rows

            delegate: Item {
                id: rowBox

                objectName: "questionLine"
                required property var modelData

                // The second line ("= ?") sits under the middle of the terms, not at the left edge.
                anchors.horizontalCenter: parent.horizontalCenter

                // Wider than the card allows: shrink the whole line.
                readonly property real available: root.width - 2 * (root.contentPadding + root.borderWidth)
                readonly property real fitted: line.implicitWidth > available && line.implicitWidth > 0 ? available / line.implicitWidth : 1

                // Every part of a line is as tall as the tallest of them (the slot, or the digits when the line has no
                // slot), and its content sits in the middle of that, as in the board's centred flex row. Without it the
                // digits would hang from the top of the line, well above the middle of the slot.
                readonly property real digitHeight: probe.implicitHeight
                readonly property real partHeight: modelData.indexOf("?") >= 0 ? Math.max(root.slotHeight, digitHeight) : digitHeight

                width: line.implicitWidth * fitted
                height: line.implicitHeight * fitted

                Text {
                    id: probe

                    visible: false
                    text: "0"
                    font.family: Theme.bodyFont
                    font.pixelSize: root.activeFontSize
                }

                Row {
                    id: line

                    anchors.centerIn: parent
                    spacing: root.partSpacing
                    scale: rowBox.fitted

                    Repeater {
                        model: rowBox.modelData

                        delegate: Item {
                            id: part

                            required property string modelData
                            readonly property bool isSlot: modelData === "?"

                            width: isSlot ? Math.max(root.slotWidth, root.slotValue !== "" ? filledText.implicitWidth + 24 : 0) : word.implicitWidth
                            height: rowBox.partHeight

                            Text {
                                id: word

                                objectName: "questionTerm"
                                visible: !part.isSlot
                                anchors.verticalCenter: parent.verticalCenter
                                text: Theme.operatorGlyph(part.modelData)
                                color: Theme.isOperator(part.modelData) ? Theme.blue : Theme.ink
                                font.family: Theme.bodyFont
                                font.pixelSize: root.activeFontSize
                            }

                            DashedBox {
                                objectName: "questionSlot"
                                anchors.centerIn: parent
                                width: parent.width
                                height: root.slotHeight
                                visible: part.isSlot && root.slotValue === ""
                                lineColor: Theme.red
                                lineWidth: root.slotBorder
                                radius: root.slotRadius

                                Text {
                                    anchors.centerIn: parent
                                    text: "?"
                                    color: Theme.red
                                    font.family: Theme.bodyFont
                                    font.pixelSize: root.activeFontSize
                                }
                            }

                            // The answer has landed: a solid green slot holding the number.
                            Rectangle {
                                objectName: "questionSlotFilled"
                                anchors.centerIn: parent
                                width: parent.width
                                height: root.slotHeight
                                visible: part.isSlot && root.slotValue !== ""
                                radius: root.slotRadius
                                color: Theme.green
                                border.width: root.slotBorder
                                border.color: Theme.ink

                                Text {
                                    id: filledText

                                    anchors.centerIn: parent
                                    text: root.slotValue
                                    color: Theme.ink
                                    font.family: Theme.bodyFont
                                    font.pixelSize: root.activeFontSize * 0.8
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
