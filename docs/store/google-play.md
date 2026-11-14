# Google Play listing (draft)

> **Draft text for the owner's review.** Nothing here has been filed or submitted. Every line marked "verify" must be checked against Google's current text in Play Console on the day of submission. Rules in [Compliance](../COMPLIANCE.md) apply; the steps are in the [release checklist](../release-checklist.md). Character limits are those known when this was written (verify). The shared copy is in the [App Store draft](app-store.md); this page records only what differs.

## App name (limit 30)

The candidates are the same as on the App Store: Heart Boom Maths, Heart Boom Math, Heart Boom Sums. Availability is **unchecked and is a human task**. Play does not allow promotional words in the title (verify).

## Short description (limit 80)

`Answer ten sums a level before the heart bomb pops. No ads, no accounts.` (72)

## Full description (limit 4,000)

Use the description in the [App Store draft](app-store.md), without the Game Center paragraph: v1 on Android does not use Play Games Services.

## Category and tags

| Field | Suggestion | Note |
|---|---|---|
| Application type | Game | Owner decides between Game and App |
| Category | Educational (games), or Puzzle | Owner decides |
| Tags | Up to five from Play's list (verify) | Pick those about maths and puzzles |

## Contact details and policy

| Field | Value |
|---|---|
| Privacy policy URL | The published [privacy policy](../privacy-policy.md) page (address to confirm) |
| Email | Owner to supply. Play shows it publicly |
| Website | Optional. The repository page |
| Price | Owner to decide (the text assumes a free app) |
| Developer account type | Owner to decide. Google has extra testing rules for newer personal accounts, and an organisation account needs an identity check (verify) |

## Target audience and content

| Question | Answer |
|---|---|
| Target age groups | The groups that cover 5 to 12 (Play offers 5 and under, 6 to 8, and 9 to 12). Verify the group names |
| Does the app appeal to children | Yes. It is made for them |
| Ads | No ads |
| App access | All functionality is available without sign-in. No login details to give |

Choosing any group under 13 puts the app under the Families policy. Points that apply (verify against the current Families policy text, see [Compliance](../COMPLIANCE.md)):

- A privacy policy, linked from the listing.
- No ads, so no ad SDK needs to be certified. The game has none.
- No collection of personal data from children. The game collects none.
- No request for sensitive permissions, and no location. The game asks for none. Check the merged manifest for every release (see the checklist).
- Content must suit all the age groups chosen. A game for 5 and under is judged by that group.
- Links out of the game open only behind a parental gate.
- Google also offers an optional "Designed for Families" label and a "Teacher approved" programme. Both are optional and out of scope for v1.

## Content rating (IARC questionnaire)

Category: game. Answers, from what the app does (verify the questions as shown):

| Topic | Answer |
|---|---|
| Violence | No. The bomb pops into a star burst and nothing is harmed. The word "bomb" and the explosion art are the likeliest point of challenge, so describe the art if asked. If the result is above the lowest rating, the owner and the designer decide whether to change the art |
| Blood, fear, sexuality, language, drugs, gambling | No |
| Simulated gambling | No. Coins cannot be bought or cashed out |
| Users can interact or exchange content | No |
| Shares the user's location | No |
| Digital purchases | No |
| Unrestricted web access | No |

Expected result: the lowest rating in each region (PEGI 3, ESRB Everyone) per [Compliance](../COMPLIANCE.md).

## Data safety form

| Question | Answer |
|---|---|
| Does the app collect or share any of the required user data types | No |
| Is all user data encrypted in transit | Not asked when nothing is collected (verify) |
| Can users request that data is deleted | Not applicable. The app has no accounts, and the saved file is removed by uninstalling or clearing the app's storage |
| Independent security review | No |

The privacy policy URL on the form must be the same as on the listing.

## Permissions

None. A game that makes no network request of its own should not need the internet permission. Check the merged manifest of the release bundle and compare it with this page (verify). If a permission appears, find out why before release, and update the form and the policy.

## Advertising, purchases and Play Games

None of the three. There is no ad code, no in-app product and no Play Games Services (`NullScoreService` on Android).

## Export

Google has no export compliance form. The US export-laws declaration in Play Console is answered with no encryption beyond the operating system's (verify).

## Graphics and screenshots

Screens as in the [App Store draft](app-store.md), from the real app, with a made-up nickname in any top 10.

| Asset | Size | Note |
|---|---|---|
| App icon | 512 by 512 | Owner or designer supplies |
| Feature graphic | 1024 by 500 | Owner or designer supplies |
| Phone screenshots | Two to eight, portrait | Same six screens as the App Store shot list (verify the size rules) |
| Tablet screenshots | Optional but advised | Regular layout, boards CP9 and CP10 |
