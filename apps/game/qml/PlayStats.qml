// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The points, the multiplier and the coin wallet. On a phone and a tablet the points sit in a white card; in the left panel
// of a desktop window they are plain text.
//
// The multiplier badge has three looks: a red "x2!" while the step is above 1, a plain "x1", and while a wrong answer is
// held a dashed "x2 -> x1" that shows the step falling. The dash and the arrow are the shape cue besides the colour.
ColumnLayout {
    id: root

    required property PlayMetrics metrics
    property real points: 0
    property int multiplier: 1
    // The step before a wrong answer took it down: shown as a change when it is above multiplier.
    property int multiplierBefore: 1
    property int wallet: 0
    property int walletDelta: 0
    property bool plainPoints: false

    readonly property bool falling: multiplierBefore > multiplier

    // The step last drawn, to tell a rise from a fall.
    property int lastMultiplier: 1

    spacing: metrics.statRowGap

    // The points.
    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: pointsRow.implicitHeight + 2 * root.metrics.pointsPadY + (root.plainPoints ? 0 : 2 * root.metrics.pointsBorder)
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("%1 points").arg(root.points)

        ComicCard {
            anchors.fill: parent
            visible: !root.plainPoints
            borderWidth: root.metrics.pointsBorder
            radius: root.metrics.pointsRadius
            shadowOffset: 0
        }

        RowLayout {
            id: pointsRow

            anchors.fill: parent
            anchors.leftMargin: root.metrics.pointsPadX + (root.plainPoints ? 0 : root.metrics.pointsBorder)
            anchors.rightMargin: root.metrics.pointsPadX + (root.plainPoints ? 0 : root.metrics.pointsBorder)
            anchors.topMargin: root.metrics.pointsPadY + (root.plainPoints ? 0 : root.metrics.pointsBorder)
            anchors.bottomMargin: root.metrics.pointsPadY + (root.plainPoints ? 0 : root.metrics.pointsBorder)

            // The number and its label share one baseline, as on the boards.
            Text {
                objectName: "pointsValue"
                Layout.alignment: Qt.AlignBaseline
                text: Theme.formatNumber(root.points)
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: root.metrics.pointsFont
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                objectName: "pointsLabel"
                Layout.alignment: Qt.AlignBaseline
                text: qsTr("POINTS")
                color: Theme.ink
                font.family: Theme.bodyFont
                font.pixelSize: root.metrics.pointsLabelFont
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: root.metrics.statGap

        // The multiplier.
        Badge {
            id: multiplierBadge

            objectName: "multiplierBadge"
            Layout.fillWidth: true
            Layout.preferredWidth: root.falling ? 1.5 : 1
            verticalPadding: root.metrics.badgePadY
            text: root.falling ? "" : (root.multiplier > 1 ? qsTr("x%1!").arg(root.multiplier) : qsTr("x%1").arg(root.multiplier))
            fillColor: root.multiplier > 1 && !root.falling ? Theme.red : Theme.white
            textColor: root.multiplier > 1 && !root.falling ? Theme.white : Theme.ink
            dashed: root.falling
            borderWidth: root.metrics.badgeBorder
            radius: root.metrics.badgeRadius
            pixelSize: root.falling ? root.metrics.badgeFont * 0.92 : root.metrics.badgeFont
            Accessible.name: root.falling ? qsTr("Multiplier falls from times %1 to times %2").arg(root.multiplierBefore).arg(root.multiplier) : qsTr("Multiplier times %1").arg(root.multiplier)

            // The step has just risen: the badge grows and settles.
            scale: 1
            SequentialAnimation {
                id: grow

                NumberAnimation {
                    target: multiplierBadge
                    property: "scale"
                    to: 1.25
                    duration: 140
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    target: multiplierBadge
                    property: "scale"
                    to: 1
                    duration: 220
                    easing.type: Easing.OutBack
                }
            }
            Connections {
                target: root

                function onMultiplierChanged() {
                    if (root.multiplier > root.lastMultiplier) {
                        grow.restart();
                    }
                    root.lastMultiplier = root.multiplier;
                }
            }

            // "x2 -> x1": the change, drawn.
            Row {
                visible: root.falling
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("x%1").arg(root.multiplierBefore)
                    color: Theme.ink
                    font.family: Theme.displayFont
                    font.pixelSize: root.metrics.badgeFont * 0.92
                    font.letterSpacing: 1
                }
                ArrowIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.metrics.badgeFont * 0.8
                    height: root.metrics.badgeFont * 0.5
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("x%1").arg(root.multiplier)
                    color: Theme.ink
                    font.family: Theme.displayFont
                    font.pixelSize: root.metrics.badgeFont * 0.92
                    font.letterSpacing: 1
                }
            }
        }

        CoinWallet {
            objectName: "coinWallet"
            Layout.fillWidth: true
            Layout.preferredWidth: root.walletDelta !== 0 ? 1.5 : 1
            Layout.preferredHeight: multiplierBadge.implicitHeight
            value: root.wallet
            delta: root.walletDelta
            coinSize: root.metrics.coinSize
            pixelSize: root.metrics.coinFont
            deltaPixelSize: root.metrics.deltaFont
            borderWidth: root.metrics.badgeBorder
            radius: root.metrics.badgeRadius
        }
    }
}
