// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick

// The design tokens of the comic look and the small helpers every screen shares. Colours, outline widths, radii, the hard
// offset shadow and the two fonts live here and nowhere else.
QtObject {
    id: theme

    // ---- Colours -----------------------------------------------------------------------------------------------
    readonly property color background: "#FFE14D"
    readonly property color halftone: "#F5B800"
    readonly property color ink: "#111111"
    readonly property color red: "#D62839"
    readonly property color blue: "#1D6FE0"
    readonly property color green: "#5BD67F"
    readonly property color orange: "#FF8A00"
    readonly property color heartPink: "#FF5FA2"
    readonly property color white: "#FFFFFF"
    readonly property color metal: "#9AA5B1"
    readonly property color rope: "#8A6A3A"
    readonly property color coin: "#F5B800"
    readonly property color disabledFill: "#E8E8E8"
    readonly property color disabledText: "#4A4A4A"
    readonly property color switchOff: "#D9D9D9" // the track of a switch that is off

    // ---- Shape (compact; a screen scales these up for the regular size class) --------------------------------------
    readonly property int outlineSmall: 4 // small controls
    readonly property int outlineLarge: 5 // cards and large buttons
    readonly property int radiusSmall: 12
    readonly property int radiusLarge: 22
    readonly property int shadowSmall: 6
    readonly property int shadowLarge: 8 // the hard offset shadow: no blur, ink coloured

    // ---- Fonts: loaded from application resources, never from a network ----------------------------------------------
    readonly property FontLoader displayLoader: FontLoader {
        source: "qrc:/heart_boom_maths/fonts/Bangers-Regular.ttf"
    }
    readonly property FontLoader bodyLoader: FontLoader {
        source: "qrc:/heart_boom_maths/fonts/LilitaOne-Regular.ttf"
    }
    readonly property string displayFont: displayLoader.name // headings, big numbers, the timer
    readonly property string bodyFont: bodyLoader.name // everything else
    readonly property bool fontsReady: displayLoader.status === FontLoader.Ready && bodyLoader.status === FontLoader.Ready

    // ---- Size classes ------------------------------------------------------------------------------------------
    // compact: a phone in portrait. regular: tablets and desktop windows.
    readonly property int compactMaxWidth: 600

    function sizeClassFor(windowWidth) {
        return windowWidth < compactMaxWidth ? "compact" : "regular";
    }

    // ---- Text helpers ----------------------------------------------------------------------------------------
    // The timer reads m:ss, for example 0:42.
    function formatTime(seconds) {
        const whole = Math.max(0, Math.floor(seconds));
        const rest = whole % 60;
        return Math.floor(whole / 60) + ":" + (rest < 10 ? "0" : "") + rest;
    }

    // 3860 reads 3,860 whatever the language of the device.
    function formatNumber(value) {
        return Number(value).toLocaleString(Qt.locale("en_US"), "f", 0);
    }

    // The build date as About shows it: 2026-11-14 reads 14 Nov 2026. Anything that is not a date (a local build says
    // "dev") is shown as it is.
    function formatBuildDate(text) {
        const months = [qsTr("Jan"), qsTr("Feb"), qsTr("Mar"), qsTr("Apr"), qsTr("May"), qsTr("Jun"), qsTr("Jul"), qsTr("Aug"), qsTr("Sep"), qsTr("Oct"), qsTr("Nov"), qsTr("Dec")];
        const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(text);
        if (match === null) {
            return text;
        }
        const month = parseInt(match[2], 10);
        if (month < 1 || month > 12) {
            return text;
        }
        return parseInt(match[3], 10) + " " + months[month - 1] + " " + match[1];
    }

    // The core writes a take-away as "-"; the fonts have a real minus sign.
    function operatorGlyph(part) {
        return part === "-" ? "−" : part;
    }

    function isOperator(part) {
        return part === "+" || part === "-" || part === "=";
    }

    // The words for a count of tiles, for "TWO TILES GONE".
    function countWord(count) {
        const words = [qsTr("NO"), qsTr("ONE"), qsTr("TWO"), qsTr("THREE"), qsTr("FOUR")];
        return count >= 0 && count < words.length ? words[count] : String(count);
    }

    // Bits set in a mask: the tiles a hint removed.
    function bitCount(mask) {
        let count = 0;
        for (let bits = mask; bits > 0; bits = bits >> 1) {
            count += bits & 1;
        }
        return count;
    }
}
