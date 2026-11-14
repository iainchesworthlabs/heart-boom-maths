// SPDX-License-Identifier: GPL-3.0-or-later

// Starts the game in the page: asks Qt's loader (qtloader.js) for the WebAssembly module, shows the loading message
// until the first frame, and says so when the game cannot start or has stopped. A file of its own, not a script inside
// the page, so the page's content security policy can refuse inline script.

(function () {
    "use strict";

    const loading = document.getElementById("loading");
    const text = document.getElementById("loading-text");
    const screen = document.getElementById("screen");

    function fail(message) {
        loading.classList.add("failed");
        loading.style.display = "flex";
        screen.style.display = "none";
        text.textContent = message;
    }

    if (typeof WebAssembly !== "object" || typeof qtLoad !== "function" || typeof window.heart_boom_maths_entry !== "function") {
        fail("This browser cannot run the game. Try a recent version of Chrome, Edge, Firefox or Safari.");
        return;
    }

    qtLoad({
        qt: {
            entryFunction: window.heart_boom_maths_entry,
            containerElements: [screen],
            onLoaded: function () {
                loading.style.display = "none";
                screen.style.display = "block";
            },
            onExit: function () {
                fail("The game has stopped. Reload the page to play again.");
            }
        }
    }).catch(function (error) {
        console.error(error);
        fail("The game could not start. Reload the page to try again.");
    });
})();
