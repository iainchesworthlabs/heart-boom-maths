// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// One line of the top 10: the rank (a medal for the first three), the name and the score. The entry that has just been
// saved is `highlighted`: a blue row with white text and a NEW tag, so the row is marked by its tag and colour together and
// the tag still reads on a screen that shows no colour.
Item {
    id: root

    property int rank: 1
    property string name: ""
    property string scoreText: ""
    property bool highlighted: false
    // Pixels per design unit; the row is 50 high at 1.
    property real unit: 1

    // The medal colours of the first three places. Plain values: they belong to this list and to no other screen.
    readonly property var medalColors: ["#F5B800", "#C9D1D9", "#D98A4A"]
    readonly property bool hasMedal: rank >= 1 && rank <= 3

    implicitHeight: 50 * unit

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("%1, %2, %3 points").arg(rank).arg(name).arg(scoreText) + (highlighted ? qsTr(", new") : "")

    ComicCard {
        anchors.fill: parent
        fillColor: root.highlighted ? Theme.blue : Theme.white
        borderWidth: Math.max(3, 4 * root.unit)
        radius: 14 * root.unit
        shadowOffset: 4 * root.unit

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12 * root.unit
            anchors.rightMargin: 12 * root.unit
            spacing: 12 * root.unit

            Item {
                Layout.preferredWidth: 34 * root.unit
                Layout.preferredHeight: 34 * root.unit
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    objectName: "medal"
                    anchors.fill: parent
                    visible: root.hasMedal
                    radius: width / 2
                    color: root.hasMedal ? root.medalColors[root.rank - 1] : "transparent"
                    border.width: Math.max(2, 3 * root.unit)
                    border.color: Theme.ink
                }

                Text {
                    objectName: "rankText"
                    anchors.centerIn: parent
                    text: String(root.rank)
                    color: root.hasMedal ? Theme.ink : (root.highlighted ? Theme.white : Theme.ink)
                    font.family: Theme.displayFont
                    font.pixelSize: 22 * root.unit
                }
            }

            Text {
                objectName: "nameText"
                // As wide as the name, and no wider, so the NEW tag follows it; a name too long for the row shrinks and
                // is elided.
                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth
                Layout.alignment: Qt.AlignVCenter
                text: root.name
                elide: Text.ElideRight
                color: root.highlighted ? Theme.white : Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: 26 * root.unit
                font.letterSpacing: 2 * root.unit
            }

            Badge {
                objectName: "newTag"
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: 2 * root.unit
                visible: root.highlighted
                text: qsTr("NEW")
                fillColor: Theme.background
                borderWidth: Math.max(2, 3 * root.unit)
                radius: 8 * root.unit
                pixelSize: 16 * root.unit
                letterSpacing: 1
                horizontalPadding: 8 * root.unit
            }

            // The room between the name (and its tag) and the score, which stays at the right edge.
            Item {
                Layout.fillWidth: true
            }

            Text {
                objectName: "scoreText"
                Layout.alignment: Qt.AlignVCenter
                text: root.scoreText
                color: root.highlighted ? Theme.white : Theme.ink
                font.family: Theme.displayFont
                font.pixelSize: 28 * root.unit
                font.letterSpacing: 1
            }
        }
    }
}
