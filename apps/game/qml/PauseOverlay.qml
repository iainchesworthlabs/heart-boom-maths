// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The pause screen (board CP16): the time left, the level, the question and the points, then RESUME and QUIT. It covers the
// play screen while the game is paused; the clock stays stopped until RESUME.
Item {
    id: root

    required property GameController game
    // The gear for Settings on the board. Off until a Settings screen exists to open.
    property bool showSettings: false

    readonly property real contentWidth: Math.min(width - 48, 480)
    readonly property real unit: contentWidth / 342

    signal settingsRequested

    focus: visible

    HalftoneBackground {
        anchors.fill: parent
    }

    // The play screen's own controls sit underneath: this catches the taps that miss every control here.
    MouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Math.min(56, root.height * 0.07)
        anchors.bottomMargin: Math.min(36, root.height * 0.05)
        width: root.contentWidth
        spacing: 18

        StarBurst {
            objectName: "pausedBurst"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.contentWidth
            Layout.preferredHeight: root.contentWidth * 150 / 342
            text: qsTr("PAUSED")
            textSize: 58 * root.unit
            letterSpacing: 3 * root.unit
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            HeartBomb {
                objectName: "pauseHeart"
                Layout.alignment: Qt.AlignVCenter
                size: 120 * root.unit
                secondsLeft: root.game.secondsLeft
            }

            ComicCard {
                objectName: "pauseStats"
                Layout.fillWidth: true
                Layout.preferredHeight: rows.implicitHeight + 28 + 2 * borderWidth

                ColumnLayout {
                    id: rows

                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("LEVEL")
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 20
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Text {
                            objectName: "pauseLevel"
                            text: String(root.game.level)
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 20
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("QUESTION")
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 20
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Text {
                            objectName: "pauseQuestion"
                            text: qsTr("%1 / %2").arg(root.game.questionNumber).arg(root.game.questionsPerLevel)
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 20
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("POINTS")
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 20
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Text {
                            objectName: "pausePoints"
                            text: Theme.formatNumber(root.game.points)
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 20
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("The bomb waits until you tap RESUME")
            color: Theme.ink
            font.family: Theme.bodyFont
            font.pixelSize: 20
        }

        Item {
            Layout.fillHeight: true
        }

        ComicButton {
            objectName: "resumeButton"
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            variant: "danger"
            text: qsTr("RESUME")
            pixelSize: 52
            letterSpacing: 3
            borderWidth: Theme.outlineLarge
            radius: Theme.radiusLarge
            shadowOffset: Theme.shadowLarge
            onClicked: root.game.resume()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            ComicButton {
                objectName: "quitButton"
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                variant: "secondary"
                text: qsTr("QUIT")
                pixelSize: 34
                letterSpacing: 2
                borderWidth: Theme.outlineLarge
                radius: 18
                shadowOffset: Theme.shadowSmall
                onClicked: root.game.quit()
            }

            ComicButton {
                objectName: "settingsButton"
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                visible: root.showSettings
                variant: "secondary"
                accessibleName: qsTr("Settings")
                borderWidth: Theme.outlineLarge
                radius: 18
                shadowOffset: Theme.shadowSmall
                onClicked: root.settingsRequested()

                GearIcon {
                    anchors.centerIn: parent
                    width: 32
                    height: 32
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("QUIT ends this game. Your coins stay safe.")
            color: Theme.ink
            font.family: Theme.bodyFont
            font.pixelSize: 16
        }
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.visible
        onActivated: root.game.resume()
    }
}
