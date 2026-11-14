// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The top of the play screen: the pause button, the level (with an ENDLESS tag past level 12) and the question count.
RowLayout {
    id: root

    required property PlayMetrics metrics
    property int level: 0
    property bool endless: false
    property int questionNumber: 0
    property int questionsPerLevel: 0

    signal pauseClicked

    spacing: metrics.mode === "compact" ? 10 : 16

    ComicButton {
        objectName: "pauseButton"
        Layout.preferredWidth: root.metrics.pauseSize
        Layout.preferredHeight: root.metrics.pauseSize
        variant: "secondary"
        accessibleName: qsTr("Pause")
        borderWidth: root.metrics.pauseBorder
        radius: root.metrics.pauseRadius
        onClicked: root.pauseClicked()

        PauseIcon {
            anchors.centerIn: parent
            width: root.metrics.pauseIcon
            height: root.metrics.pauseIcon
        }
    }

    Text {
        objectName: "levelLabel"
        text: qsTr("LEVEL %1").arg(root.level)
        color: Theme.ink
        font.family: Theme.displayFont
        font.pixelSize: root.metrics.levelFont
        font.letterSpacing: root.metrics.levelSpacing
    }

    Badge {
        objectName: "endlessBadge"
        visible: root.endless
        text: qsTr("ENDLESS")
        fillColor: Theme.ink
        textColor: Theme.background
        borderWidth: 0
        radius: 10
        pixelSize: root.metrics.endlessFont
        letterSpacing: 2
        horizontalPadding: 10
        verticalPadding: 3
    }

    Item {
        Layout.fillWidth: true
    }

    Text {
        objectName: "questionCount"
        text: qsTr("%1 / %2").arg(root.questionNumber).arg(root.questionsPerLevel)
        color: Theme.ink
        font.family: Theme.bodyFont
        font.pixelSize: root.metrics.countFont
    }
}
