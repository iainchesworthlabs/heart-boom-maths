// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

// The play screen (boards CP2 to CP5, CP9, CP10 and CP13). It draws what the controller exposes and what AnswerFeedback
// holds, and works out no rule.
//
// The core deals the next question the moment an answer is given, so for the second that a right or wrong answer is held
// the screen draws the held question and tiles from `feedback` instead of the live ones. The timer, the points and the
// coins are always the live ones. Taps are ignored while an answer is held, because the tiles on screen are no longer the
// ones the core is asking about.
//
// Three arrangements, chosen from the size of the window: compact (a phone), portrait (a tablet) and landscape (a desktop
// window, with the stats in a left panel and the tiles in one row).
Item {
    id: root

    required property GameController game
    required property AnswerFeedback feedback

    readonly property string mode: Theme.sizeClassFor(width) === "compact" ? "compact" : (width >= height ? "landscape" : "portrait")

    // ---- What is on screen ---------------------------------------------------------------------------------------
    readonly property bool rightHeld: feedback.kind === AnswerFeedback.Right
    readonly property bool wrongHeld: feedback.kind === AnswerFeedback.Wrong
    readonly property bool holding: rightHeld || wrongHeld
    readonly property bool boomed: feedback.kind === AnswerFeedback.Boom
    readonly property bool playing: game.state === GameController.Playing
    readonly property bool alertShown: game.alert && playing

    readonly property var shownParts: holding ? feedback.parts : game.questionParts
    readonly property var shownTiles: holding ? feedback.tiles : game.tiles
    readonly property int shownRemoved: holding ? feedback.removedTiles : game.removedTiles
    readonly property int shownQuestionNumber: holding ? feedback.questionNumber : game.questionNumber
    // The number that landed in the slot, while a right answer is held.
    readonly property string slotValue: rightHeld && feedback.chosenTile >= 0 && feedback.chosenTile < feedback.tiles.length ? String(feedback.tiles[feedback.chosenTile]) : ""

    readonly property var tileLooks: [tileLook(0), tileLook(1), tileLook(2), tileLook(3)]
    readonly property string hurryText: game.secondsLeft === 1 ? qsTr("HURRY! 1 SECOND LEFT!") : qsTr("HURRY! %1 SECONDS LEFT!").arg(game.secondsLeft)
    readonly property string flashText: wrongHeld ? "−" + Math.abs(feedback.secondsDelta) + qsTr(" s") : ""
    readonly property string gainText: rightHeld && feedback.secondsDelta > 0 ? "+" + feedback.secondsDelta + qsTr(" s") : ""
    readonly property real ticksTotal: Math.max(1, game.ticksTotal)
    readonly property real fuseFraction: game.ticksLeft / ticksTotal
    readonly property real fuseAlertFraction: game.alertTicks / ticksTotal
    readonly property real fuseLostFraction: wrongHeld ? Math.abs(feedback.secondsDelta) * game.ticksPerSecond / ticksTotal : 0
    readonly property int walletDelta: game.hintUsed ? -game.hintCost : 0
    readonly property bool hintEnabled: playing && !holding && game.hintAvailable

    function tileLook(position) {
        if (((shownRemoved >> position) & 1) !== 0) {
            return "gone";
        }
        if (wrongHeld) {
            return position === feedback.chosenTile ? "wrong" : (position === feedback.correctTile ? "right" : "dimmed");
        }
        if (rightHeld) {
            return position === feedback.chosenTile ? "right" : "dimmed";
        }
        return "normal";
    }

    PlayMetrics {
        id: sizes

        mode: root.mode
    }

    HalftoneBackground {
        anchors.fill: parent
        gridSize: sizes.dotGrid
        dotRadius: sizes.dotRadius
    }

    // The screen shakes when the bomb goes off.
    Item {
        id: stage

        width: parent.width
        height: parent.height

        property real shakeX: 0

        transform: Translate {
            x: stage.shakeX
        }

        SequentialAnimation {
            running: root.boomed

            NumberAnimation {
                target: stage
                property: "shakeX"
                to: 14
                duration: 50
            }
            NumberAnimation {
                target: stage
                property: "shakeX"
                to: -14
                duration: 90
            }
            NumberAnimation {
                target: stage
                property: "shakeX"
                to: 9
                duration: 80
            }
            NumberAnimation {
                target: stage
                property: "shakeX"
                to: -6
                duration: 70
            }
            NumberAnimation {
                target: stage
                property: "shakeX"
                to: 0
                duration: 60
            }
        }

        // The arrangement follows the size of the window.
        Loader {
            id: arrangement

            objectName: "arrangement"
            anchors.fill: parent
            sourceComponent: sizes.wide ? wideArrangement : columnArrangement
        }
    }

    // Phone and tablet: one column.
    Component {
        id: columnArrangement

        ColumnLayout {
            objectName: "columnLayout"
            anchors.fill: parent
            anchors.topMargin: Math.min(sizes.paddingTop, root.height * 0.07)
            anchors.leftMargin: sizes.paddingSide
            anchors.rightMargin: sizes.paddingSide
            anchors.bottomMargin: Math.min(sizes.paddingBottom, root.height * 0.05)
            spacing: sizes.gap

            PlayTopBar {
                Layout.fillWidth: true
                metrics: sizes
                level: root.game.level
                endless: root.game.endless
                questionNumber: root.shownQuestionNumber
                questionsPerLevel: root.game.questionsPerLevel
                onPauseClicked: root.game.pause()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: sizes.rowGap

                PlayHeart {
                    Layout.alignment: Qt.AlignVCenter
                    size: sizes.heartSize
                    secondsLeft: root.game.secondsLeft
                    alert: root.alertShown
                    boom: root.boomed
                    flashText: root.flashText
                }

                PlayStats {
                    Layout.fillWidth: true
                    metrics: sizes
                    points: root.game.points
                    multiplier: root.game.multiplier
                    multiplierBefore: root.wrongHeld ? root.feedback.multiplierBefore : root.game.multiplier
                    wallet: root.game.wallet
                    walletDelta: root.walletDelta
                }
            }

            PlayFuse {
                Layout.fillWidth: true
                Layout.preferredHeight: sizes.fuseHeight
                metrics: sizes
                fraction: root.fuseFraction
                alertFraction: root.fuseAlertFraction
                alert: root.alertShown
                lostFraction: root.fuseLostFraction
                secondsLeft: root.game.secondsLeft
                gainText: root.gainText
            }

            FeedbackTag {
                objectName: "alertBanner"
                Layout.fillWidth: true
                visible: root.alertShown
                tone: "alert"
                text: root.hurryText
                pixelSize: sizes.bannerFont
                borderWidth: sizes.bannerBorder
                shadowOffset: sizes.bannerShadow
            }

            // The question, with the hint button or the line that follows an answer under it, centred in what is left.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Column {
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: sizes.messageGap

                    QuestionCard {
                        objectName: "questionCard"
                        width: parent.width
                        height: contentHeight + 2 * sizes.cardPaddingY + 2 * sizes.cardBorder
                        parts: root.shownParts
                        slotValue: root.slotValue
                        fontSize: sizes.cardFont
                        multiLineFontSize: sizes.cardFontMulti
                        slotWidth: sizes.slotWidth
                        slotHeight: sizes.slotHeight
                        slotBorder: sizes.slotBorder
                        slotRadius: sizes.slotRadius
                        partSpacing: sizes.partSpacing
                        borderWidth: sizes.cardBorder
                        radius: sizes.cardRadius
                        shadowOffset: sizes.cardShadow
                    }

                    Item {
                        width: parent.width
                        height: sizes.hintHeight

                        PlayHint {
                            objectName: "hintButton"
                            anchors.centerIn: parent
                            visible: !root.holding && !root.boomed
                            metrics: sizes
                            cost: root.game.hintCost
                            used: root.game.hintUsed
                            removedCount: Theme.bitCount(root.game.removedTiles)
                            enabled: root.hintEnabled
                            onClicked: root.game.useHint()
                        }

                        FeedbackTag {
                            objectName: "wrongTag"
                            anchors.centerIn: parent
                            visible: root.wrongHeld
                            tone: "wrong"
                            text: qsTr("NOT QUITE! IT'S %1").arg(root.feedback.correctValue)
                            pixelSize: sizes.bannerFont
                            borderWidth: sizes.bannerBorder
                            shadowOffset: sizes.bannerShadow
                        }

                        FeedbackTag {
                            objectName: "rightTag"
                            anchors.centerIn: parent
                            visible: root.rightHeld
                            tone: "right"
                            text: root.feedback.points > 0 ? "+" + root.feedback.points : qsTr("RIGHT!")
                            pixelSize: sizes.bannerFont
                            borderWidth: sizes.bannerBorder
                            shadowOffset: sizes.bannerShadow
                        }
                    }
                }
            }

            AnswerGrid {
                objectName: "answerGrid"
                Layout.fillWidth: true
                metrics: sizes
                values: root.shownTiles
                looks: root.tileLooks
                interactive: root.playing && !root.holding
                onAnswered: position => root.game.answer(position)
            }
        }
    }

    // Desktop window: a left panel and the tiles in one row.
    Component {
        id: wideArrangement

        ColumnLayout {
            objectName: "wideLayout"
            anchors.fill: parent
            anchors.topMargin: sizes.paddingTop
            anchors.leftMargin: sizes.paddingSide
            anchors.rightMargin: sizes.paddingSide
            anchors.bottomMargin: sizes.paddingBottom
            spacing: sizes.gap

            PlayTopBar {
                Layout.fillWidth: true
                metrics: sizes
                level: root.game.level
                endless: root.game.endless
                questionNumber: root.shownQuestionNumber
                questionsPerLevel: root.game.questionsPerLevel
                onPauseClicked: root.game.pause()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: sizes.columnGap

                ComicCard {
                    Layout.preferredWidth: sizes.panelWidth
                    Layout.fillHeight: true
                    borderWidth: sizes.panelBorder
                    radius: sizes.panelRadius
                    shadowOffset: sizes.panelShadow

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: sizes.panelPadding
                        spacing: sizes.panelGap

                        PlayHeart {
                            Layout.alignment: Qt.AlignHCenter
                            size: sizes.heartSize
                            secondsLeft: root.game.secondsLeft
                            alert: root.alertShown
                            boom: root.boomed
                            flashText: root.flashText
                        }

                        PlayFuse {
                            Layout.fillWidth: true
                            Layout.preferredHeight: sizes.fuseHeight
                            metrics: sizes
                            fraction: root.fuseFraction
                            alertFraction: root.fuseAlertFraction
                            alert: root.alertShown
                            lostFraction: root.fuseLostFraction
                            secondsLeft: root.game.secondsLeft
                            gainText: root.gainText
                        }

                        PlayStats {
                            Layout.fillWidth: true
                            metrics: sizes
                            plainPoints: true
                            points: root.game.points
                            multiplier: root.game.multiplier
                            multiplierBefore: root.wrongHeld ? root.feedback.multiplierBefore : root.game.multiplier
                            wallet: root.game.wallet
                            walletDelta: root.walletDelta
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        PlayHint {
                            objectName: "hintButton"
                            Layout.fillWidth: true
                            metrics: sizes
                            cost: root.game.hintCost
                            used: root.game.hintUsed
                            removedCount: Theme.bitCount(root.game.removedTiles)
                            enabled: root.hintEnabled
                            onClicked: root.game.useHint()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: sizes.gap

                    FeedbackTag {
                        objectName: "alertBanner"
                        Layout.fillWidth: true
                        visible: root.alertShown
                        tone: "alert"
                        text: root.hurryText
                        pixelSize: sizes.bannerFont
                        borderWidth: sizes.bannerBorder
                        shadowOffset: sizes.bannerShadow
                    }

                    QuestionCard {
                        objectName: "questionCard"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        parts: root.shownParts
                        slotValue: root.slotValue
                        fontSize: sizes.cardFont
                        multiLineFontSize: sizes.cardFontMulti
                        slotWidth: sizes.slotWidth
                        slotHeight: sizes.slotHeight
                        slotBorder: sizes.slotBorder
                        slotRadius: sizes.slotRadius
                        partSpacing: sizes.partSpacing
                        borderWidth: sizes.cardBorder
                        radius: sizes.cardRadius
                        shadowOffset: sizes.cardShadow

                        FeedbackTag {
                            objectName: "wrongTag"
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 22
                            visible: root.wrongHeld
                            tone: "wrong"
                            text: qsTr("NOT QUITE! IT'S %1").arg(root.feedback.correctValue)
                            pixelSize: sizes.bannerFont
                            borderWidth: sizes.bannerBorder
                            shadowOffset: sizes.bannerShadow
                        }

                        FeedbackTag {
                            objectName: "rightTag"
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 22
                            visible: root.rightHeld
                            tone: "right"
                            text: root.feedback.points > 0 ? "+" + root.feedback.points : qsTr("RIGHT!")
                            pixelSize: sizes.bannerFont
                            borderWidth: sizes.bannerBorder
                            shadowOffset: sizes.bannerShadow
                        }
                    }

                    AnswerGrid {
                        objectName: "answerGrid"
                        Layout.fillWidth: true
                        metrics: sizes
                        values: root.shownTiles
                        looks: root.tileLooks
                        interactive: root.playing && !root.holding
                        onAnswered: position => root.game.answer(position)
                    }
                }
            }
        }
    }

    // The edge of the play area pulses in the alert: a red frame, one more cue besides the red fuse and the words.
    Rectangle {
        id: edge

        objectName: "alertEdge"
        anchors.fill: parent
        visible: root.alertShown
        color: "transparent"
        border.width: 8
        border.color: Theme.red
        enabled: false

        SequentialAnimation on opacity {
            running: edge.visible
            loops: Animation.Infinite

            NumberAnimation {
                from: 1
                to: 0.45
                duration: 450
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                from: 0.45
                to: 1
                duration: 450
                easing.type: Easing.InOutSine
            }
        }
    }

    // A short white flash when the multiplier steps up: the visual twin of the rising tone.
    Rectangle {
        id: flash

        objectName: "streakFlash"
        anchors.fill: parent
        color: Theme.white
        opacity: 0
        enabled: false

        SequentialAnimation {
            id: flashAnimation

            NumberAnimation {
                target: flash
                property: "opacity"
                to: 0.5
                duration: 70
            }
            NumberAnimation {
                target: flash
                property: "opacity"
                to: 0
                duration: 220
            }
        }
    }

    Connections {
        target: root.feedback

        function onHeldChanged() {
            if (root.feedback.multiplierRose) {
                flashAnimation.restart();
            }
        }
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.playing
        onActivated: root.game.pause()
    }
}
