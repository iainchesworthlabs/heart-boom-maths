// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// About (board CP15): the version and build, the font licences, and four rows: Licence and Third-party notices open
// a text that scrolls over this screen, and Privacy policy and Source code ask the controller to open a web address, which
// puts the parental gate in front of it. Nothing here opens an address itself.
//
// A focus scope: the shell gives the screen the keyboard, and the scope passes it on to the frame (or to the text viewer
// while one is open), so the Android back key reaches the screen that is on top.
FocusScope {
    id: root

    required property GameController game

    // The text on show over this screen: "" for none, "licence" or "notices".
    property string viewer: ""

    signal backClicked

    readonly property var info: game.buildInfo

    onViewerChanged: {
        if (root.viewer !== "") {
            viewerScreen.flick.forceActiveFocus();
        } else {
            frame.forceActiveFocus();
        }
    }

    function openLicence() {
        root.viewer = "licence";
    }

    function openNotices() {
        root.viewer = "notices";
    }

    ScreenFrame {
        id: frame

        anchors.fill: parent
        focus: true
        title: qsTr("ABOUT")
        designHeight: 800
        columnSpacing: 14
        escapeEnabled: root.viewer === ""
        onBackClicked: root.backClicked()

        Flickable {
            id: flick

            objectName: "aboutFlick"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: column.implicitHeight + 8 * frame.unit
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ColumnLayout {
                id: column

                // The shadows of the cards reach past the column on the right and below.
                width: flick.width - 8 * frame.unit
                spacing: 14 * frame.unit

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14 * frame.unit

                    HeartBomb {
                        objectName: "aboutHeart"
                        size: 64 * frame.unit
                        showSeconds: false
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2 * frame.unit

                        Text {
                            objectName: "aboutName"
                            Layout.fillWidth: true
                            text: root.info.appName.toUpperCase()
                            elide: Text.ElideRight
                            color: Theme.ink
                            font.family: Theme.displayFont
                            font.pixelSize: 30 * frame.unit
                            font.letterSpacing: 1 * frame.unit
                        }
                        Text {
                            objectName: "aboutVersion"
                            Layout.fillWidth: true
                            text: qsTr("Version %1 · build %2").arg(root.info.version).arg(root.info.buildNumber)
                            elide: Text.ElideRight
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 18 * frame.unit
                        }
                    }
                }

                ComicCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: facts.implicitHeight + 28 * frame.unit + 2 * borderWidth
                    shadowOffset: Theme.shadowSmall

                    ColumnLayout {
                        id: facts

                        anchors.fill: parent
                        anchors.margins: 14 * frame.unit + 5
                        anchors.leftMargin: 18 * frame.unit + 5
                        anchors.rightMargin: 18 * frame.unit + 5
                        spacing: 4 * frame.unit

                        Repeater {
                            model: [
                                {
                                    "name": "revision",
                                    "label": qsTr("Revision"),
                                    "value": root.info.commit
                                },
                                {
                                    "name": "branch",
                                    "label": qsTr("Branch"),
                                    "value": root.info.branch
                                },
                                {
                                    "name": "built",
                                    "label": qsTr("Built"),
                                    "value": Theme.formatBuildDate(root.info.date)
                                }
                            ]

                            delegate: RowLayout {
                                id: fact

                                required property var modelData

                                Layout.fillWidth: true

                                Text {
                                    text: fact.modelData.label
                                    color: Theme.ink
                                    font.family: Theme.bodyFont
                                    font.pixelSize: 18 * frame.unit
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                Text {
                                    objectName: fact.modelData.name + "Value"
                                    text: fact.modelData.value
                                    color: Theme.ink
                                    font.family: Theme.bodyFont
                                    font.pixelSize: 18 * frame.unit
                                }
                            }
                        }
                    }
                }

                ComicCard {
                    objectName: "fontLicences"
                    Layout.fillWidth: true
                    Layout.preferredHeight: fonts.implicitHeight + 28 * frame.unit + 2 * borderWidth
                    shadowOffset: Theme.shadowSmall

                    ColumnLayout {
                        id: fonts

                        anchors.fill: parent
                        anchors.margins: 14 * frame.unit + 5
                        anchors.leftMargin: 18 * frame.unit + 5
                        anchors.rightMargin: 18 * frame.unit + 5
                        spacing: 8 * frame.unit

                        Text {
                            text: qsTr("FONT LICENCES")
                            color: Theme.ink
                            font.family: Theme.displayFont
                            font.pixelSize: 24 * frame.unit
                            font.letterSpacing: 1 * frame.unit
                        }
                        // The names are in red, and the text says what each is, so the colour only adds to it.
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            textFormat: Text.StyledText
                            text: qsTr("<font color=\"%1\">Bangers</font> by Vernon Adams<br>© 2010 The Bangers Project Authors<br>SIL Open Font License 1.1").arg(Theme.red)
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 16 * frame.unit
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            textFormat: Text.StyledText
                            text: qsTr("<font color=\"%1\">Lilita One</font> by Juan Montoreano<br>© 2011 Juan Montoreano<br>SIL Open Font License 1.1").arg(Theme.red)
                            color: Theme.ink
                            font.family: Theme.bodyFont
                            font.pixelSize: 16 * frame.unit
                        }
                    }
                }

                RowButton {
                    objectName: "licenceButton"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56 * frame.unit
                    unit: frame.unit
                    label: qsTr("Licence")
                    variant: "secondary"
                    displayLabel: false
                    pixelSize: 20 * frame.unit
                    borderWidth: Theme.outlineSmall
                    radius: 16
                    shadowOffset: 5
                    horizontalPadding: 12
                    onClicked: root.openLicence()
                }

                RowButton {
                    objectName: "noticesButton"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56 * frame.unit
                    unit: frame.unit
                    label: qsTr("Third-party notices")
                    variant: "secondary"
                    displayLabel: false
                    pixelSize: 20 * frame.unit
                    borderWidth: Theme.outlineSmall
                    radius: 16
                    shadowOffset: 5
                    horizontalPadding: 12
                    onClicked: root.openNotices()
                }

                RowButton {
                    objectName: "privacyButton"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56 * frame.unit
                    unit: frame.unit
                    label: qsTr("Privacy policy")
                    hint: qsTr("asks a grown-up first")
                    variant: "secondary"
                    displayLabel: false
                    pixelSize: 20 * frame.unit
                    hintPixelSize: 16 * frame.unit
                    borderWidth: Theme.outlineSmall
                    radius: 16
                    shadowOffset: 5
                    horizontalPadding: 12
                    onClicked: root.game.requestLink(root.game.privacyPolicyUrl)
                }

                RowButton {
                    objectName: "sourceButton"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56 * frame.unit
                    unit: frame.unit
                    label: qsTr("Source code")
                    hint: qsTr("asks a grown-up first")
                    variant: "secondary"
                    displayLabel: false
                    pixelSize: 20 * frame.unit
                    hintPixelSize: 16 * frame.unit
                    borderWidth: Theme.outlineSmall
                    radius: 16
                    shadowOffset: 5
                    horizontalPadding: 12
                    onClicked: root.game.requestLink(root.game.repositoryUrl)
                }
            }
        }
    }

    TextViewer {
        id: viewerScreen

        objectName: "textViewer"
        anchors.fill: parent
        visible: root.viewer !== ""
        focus: false
        title: root.viewer === "licence" ? qsTr("LICENCE") : qsTr("NOTICES")
        text: root.viewer === "licence" ? root.licenceText : (root.game.noticesText !== "" ? root.game.noticesText : qsTr("The notices are missing from this build."))
        onBackClicked: root.viewer = ""
    }

    readonly property string licenceText: qsTr("Heart Boom Maths is free software. You may use it, study it, share it and change it under the terms of the GNU General Public License, version 3 or, at your option, any later version.\n\nIt is shared in the hope that it will be useful, but without any warranty, without even the implied warranty of merchantability or fitness for a particular purpose.\n\nThe full licence is in the LICENSE file of the source code, which the Source code button on the About screen leads to. The fonts have their own licence, the SIL Open Font License 1.1, and so do the other parts the game is built from: see Third-party notices.")
}
