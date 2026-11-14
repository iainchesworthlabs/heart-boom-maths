# App Store listing (draft)

> **Draft text for the owner's review.** Nothing here has been filed or submitted. Every line marked "verify" must be checked against Apple's current text on the day of submission. Rules in [Compliance](../COMPLIANCE.md) apply; the steps are in the [release checklist](../release-checklist.md). Character limits are those known when this was written (verify).

## Name

| Candidate | Length | Note |
|---|---|---|
| Heart Boom Maths | 16 | The chosen name. Availability in both stores and in the trademark registers is **unchecked and is a human task** (an earlier name was taken) |
| Heart Boom Math | 15 | United States spelling, if the listing is localised for that store |
| Heart Boom Sums | 15 | Fallback if the first is unavailable |

The name limit is 30 characters (verify). The name under the icon comes from `HEART_BOOM_MATHS_DISPLAY_NAME`.

## Subtitle (limit 30)

`Quick sums against the clock` (28)

## Promotional text (limit 170)

`Answer ten number sentences a level before the heart bomb pops into stars. Short levels, no ads, no accounts, and nothing leaves your device.` (verify the count when pasting)

## Description (limit 4,000)

```text
Heart Boom Maths is a quick mental-arithmetic game for children from 5 to 12.

A heart-shaped bomb is ticking. Answer the number sentence before it pops into a burst of stars! Each level has ten questions. Tap the right answer from four tiles. Right answers add time and points, a run of right answers builds a multiplier, and a wrong answer costs a few seconds, so think fast and think well.

- 12 levels that grow from adding small numbers to adding and taking away numbers up to 1,000, with missing-number and longer sums along the way
- Endless mode once every level is cleared
- Earn coins and spend them on hints that remove two wrong answers
- A top 10 of the best scores, kept on the device
- Short levels, about a minute each, with no daily rewards and no pressure to keep playing
- Sound and music can be switched off, every sound has a matching picture, and colour is never the only clue

No ads. No purchases. No accounts. No analytics. The game makes no network requests of its own, and names typed in the game never leave the device.

Heart Boom Maths is free software, made by Iain Chesworth Labs. Its source code is at github.com/iainchesworthlabs/heart-boom-maths.
```

Owner to decide: price (the text assumes a free app) and whether to add the Game Center sentence below. Add it only if score submission ships and is allowed at the chosen age rating:

```text
On iPhone and iPad, a grown-up can switch on an optional Game Center leaderboard in Settings. It is off by default and sends one number, the score.
```

## Keywords (limit 100)

`maths,math,sums,mental,arithmetic,addition,subtraction,kids,children,number,quiz,brain,practice` (verify the count)

Do not repeat the app name or the category in the keywords.

## Categories

| | Suggestion | Note |
|---|---|---|
| Primary | Games | Owner decides between Games and Education |
| Secondary | Education | Owner decides |

If the app enters the Kids Category, a Kids subcategory is also chosen.

## URLs and other fields

| Field | Value |
|---|---|
| Privacy policy URL | The published [privacy policy](../privacy-policy.md) page (address to confirm) |
| Support URL | `https://github.com/iainchesworthlabs/heart-boom-maths/issues`, or an address the owner supplies |
| Marketing URL | Optional. The repository page |
| Copyright line | Owner to supply (the repository files say "Iain Chesworth Labs contributors") |
| Price | Owner to decide |
| Trader status | Apple asks developers who sell in the European Union to declare whether they are a trader, and shows contact details publicly if so. Owner to decide and supply (verify) |
| Review notes | Say there is no sign-in, so no demo account. Explain the parental gate (a number in words typed back as digits), that the game makes no network requests of its own, and that the Game Center switch is off by default. List the build number |

## Age rating questionnaire

The goal is the lowest rating, 4+. Apple's questionnaire and rating levels change from time to time, so treat the topics as a guide and answer the questions as they appear (verify).

| Topic | Answer | Why |
|---|---|---|
| Cartoon or fantasy violence | None | The bomb pops into a star burst. No character is harmed and nothing is shown that is frightening. **This is the answer most likely to be challenged**, so the review notes should describe the art. If Apple's questionnaire will not accept None, the 4+ target needs a decision from the owner and the designer |
| Realistic violence, weapons, blood | None | |
| Sexual content or nudity, profanity, horror, mature or suggestive themes | None | |
| Alcohol, tobacco, drugs | None | |
| Gambling and contests | None | Coins have no money value and cannot be bought |
| Medical or wellness content | None | |
| Messaging or chat, user-generated content | None | |
| Unrestricted web access | No | Links open only behind a parental gate |
| Advertising | None | |
| In-app purchases | None | |
| Parental controls or age assurance features inside the app | None | The game defers to Screen Time and the device |

## App Privacy ("nutrition label")

Answer **Data Not Collected**. The game has no analytics, advertising or accounts, and the app makes no network requests of its own, so no data is sent to the developer or to anyone else.

Check these two points (verify):

- Game Center. The game sends a score to Apple's own service, only when a grown-up turns the switch on. Confirm with Apple's definition of "collect" that this need not be declared. If it must, the likely answer is Gameplay Content, not linked to the user, not used for tracking.
- The status of the Game Center switch must match the release. If score submission is removed (`NullScoreService` everywhere), nothing to declare.

Tracking: No. Required-reason APIs: confirm in the Xcode privacy report that the build, including Qt's own privacy manifest, lists no tracking domains (verify).

## Kids Category and children's rules

Entering the Kids Category is a separate decision (see [Compliance](../COMPLIANCE.md)). Points that apply if the owner enters it (verify against Apple's current Kids Category text):

- An age band is chosen at submission (Apple offers bands such as 5 and under, 6 to 8 and 9 to 11). The game is for 5 to 12, so the owner decides which band to declare.
- A parental gate in front of any link out, purchase or permission request. The game has one, on every link out and on the Game Center switch. Apple may judge the form too easy (risk in [Decisions](../DECISIONS.md)).
- No third-party analytics or advertising. The game has none.
- A privacy policy, linked from the listing and from the app.
- Confirm whether Game Center is allowed in the Kids Category. If not, remove score submission.

## Export compliance

Encryption: the game uses no encryption beyond what the operating system provides. `ITSAppUsesNonExemptEncryption` is `false` in the property list. Before answering, confirm that the final binary links no TLS or crypto library of its own, such as one pulled in by a Qt networking module (verify).

## In-app purchases and advertising

None. There is no store code, no ad code, no subscription and no way to buy coins.

## Screenshots

Boards are named only by identifier. Use a made-up nickname such as ACE in any top 10 shown, never a real name. Show a muted game at least once, to show the visual cues.

| Order | Screen | Board |
|---|---|---|
| 1 | Playing a question with the heart bomb and four tiles | One of CP2 to CP5 or CP13 |
| 2 | A streak with the multiplier flame | One of CP2 to CP5 or CP13 |
| 3 | The last ten seconds, the alert state | One of CP2 to CP5 or CP13 |
| 4 | Level complete with the points breakdown | CP6 |
| 5 | The top 10 | CP12 |
| 6 | The title screen | CP1 |

The owner picks which of CP2 to CP5 and CP13 shows each play moment. The Settings screen (CP14) and the About screen (CP15) are not needed.

| Device | Size to supply | Note |
|---|---|---|
| iPhone | The largest required size, currently 6.9 inch (verify the pixel size) | Portrait |
| iPad | The 13 inch size, if iPad is supported (verify) | Use the regular layout, boards CP9 and CP10 |

Screenshots come from the real app, not from the boards.
