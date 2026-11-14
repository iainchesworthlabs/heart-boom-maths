#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Check that the built iOS bundle of the game (a .app, or an exported .ipa) is the game the repository describes.
#
#   apps/game/ios/verify_game_app.sh <name.app | package.ipa> --bundle-id <id> --executable <name>
#       [--game-center on|off] [--entitlements <heart-boom-maths.entitlements>] [--version <x.y.z>] [--signed [--store]]
#
# It first runs apps/ios/packaging/verify_app.sh, which checks what every bundle of this project must be (the identifier,
# the icon, the launch screen, arm64, no link outside the system, the encryption key, and for --signed the signature and
# the profile). Then it checks what only the game promises:
#
#   * the orientation keys: an iPhone is held upright, an iPad turns any way, and the game takes the whole screen;
#   * the device capabilities say arm64 and gamekit, and Info.plist holds no usage text of any kind (the game asks for no
#     permission);
#   * the Game Center entitlement, in one of the two states the build can be in (--game-center, default on, the state of
#     HEART_BOOM_MATHS_IOS_GAME_CENTER): on, it is in the entitlements file (--entitlements is then required) and with
#     --signed in the signature; off, with --signed the signature must not carry it. The state found is printed;
#   * the executable links GameKit (the score) and AVFoundation (the sound), and links none of the frameworks of
#     advertising, tracking, purchases, location or notifications, which v1 must never use. Frameworks and symbols of
#     networking are listed as notes and do not fail: Qt itself can bring some in without the game ever calling it. The
#     proof that the game makes no request of its own is the traffic capture of docs/COMPLIANCE.md, on a device;
#   * every sound file of apps/game/audio is in the executable, as a Qt resource: its name is found in the resource name
#     table of the executable (Qt writes names as UTF-16, big-endian). The files themselves may be compressed there, so the
#     size says nothing; a missing name means the game would start silent;
#   * the sizes of the bundle and of the executable are printed, and written to the job summary on CI.
#
# macOS only (plutil, otool, nm, codesign).
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
verify_common="$here/../../ios/packaging/verify_app.sh"

target=""
entitlements=""
game_center=on
signed=false
common_args=()
while [ $# -gt 0 ]; do
  case "$1" in
    --entitlements) entitlements="${2:?}"; shift 2 ;;
    --game-center) game_center="${2:?}"; shift 2 ;;
    --signed) signed=true; common_args+=("$1"); shift ;;
    --bundle-id|--executable|--version) common_args+=("$1" "${2:?}"); shift 2 ;;
    --store) common_args+=("$1"); shift ;;
    -h|--help) sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "verify_game_app.sh: unknown argument '$1'" >&2; exit 2 ;;
    *) target="$1"; shift ;;
  esac
done
[ -e "$target" ] || { echo "usage: verify_game_app.sh <app|ipa> --bundle-id <id> --executable <name> [--game-center on|off] [--entitlements <file>] [--version v] [--signed [--store]]" >&2; exit 2; }
case "$game_center" in on | off) ;; *) echo "verify_game_app.sh: --game-center is on or off, not '$game_center'" >&2; exit 2 ;; esac
if [ "$game_center" = on ]; then
  [ -f "$entitlements" ] || { echo "verify_game_app.sh: --entitlements <file> is required and must exist when --game-center is on" >&2; exit 2; }
fi
fail() { echo "FAILED: $*" >&2; exit 1; }

executable=""
for ((i = 0; i < ${#common_args[@]}; i++)); do
  if [ "${common_args[$i]}" = "--executable" ]; then executable="${common_args[$((i + 1))]}"; fi
done
[ -n "$executable" ] || { echo "verify_game_app.sh: --executable is required" >&2; exit 2; }

"$verify_common" "$target" "${common_args[@]}"

work=""
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT
app="$target"
case "$target" in
  *.ipa)
    work="$(mktemp -d)"
    unzip -q "$target" -d "$work"
    app="$(find "$work/Payload" -maxdepth 1 -name '*.app' -type d | head -n 1)"
    [ -n "$app" ] || fail "no Payload/*.app in $target"
    ;;
esac
plist="$app/Info.plist"
exe="$app/$executable"

# The value of a key as JSON (a string, an array, true or false), or nothing when the key is absent.
json() { plutil -extract "$1" json -o - "$plist" 2>/dev/null | tr -d '[:space:]' || true; }
# A single true or false has no JSON form for plutil (it wants a dictionary or an array), so read those as raw text.
raw() { plutil -extract "$1" raw -o - "$plist" 2>/dev/null | tr -d '[:space:]' || true; }

[ "$(json UISupportedInterfaceOrientations)" = '["UIInterfaceOrientationPortrait"]' ] \
  || fail "iPhone orientations are $(json UISupportedInterfaceOrientations); the game is portrait only on a phone"
ipad="$(json 'UISupportedInterfaceOrientations~ipad')"
for orientation in Portrait PortraitUpsideDown LandscapeLeft LandscapeRight; do
  case "$ipad" in
    *"\"UIInterfaceOrientation$orientation\""*) ;;
    *) fail "iPad orientations are $ipad; UIInterfaceOrientation$orientation is missing" ;;
  esac
done
[ "$(raw UIRequiresFullScreen)" = "true" ] || fail "UIRequiresFullScreen is '$(raw UIRequiresFullScreen)'"
capabilities="$(json UIRequiredDeviceCapabilities)"
case "$capabilities" in
  *'"arm64"'*'"gamekit"'* | *'"gamekit"'*'"arm64"'*) ;;
  *) fail "UIRequiredDeviceCapabilities is $capabilities; it must name arm64 and gamekit" ;;
esac
usage="$(plutil -p "$plist" | grep -E 'Usage(Description)?"? =>' | head -n 1 || true)"
[ -z "$usage" ] || fail "Info.plist has a usage text ($usage): the game asks for no permission"
echo "    Info.plist: portrait on a phone, any way up on a tablet, full screen, arm64 and gamekit, no usage text"

if $signed; then
  signed_entitlements="$(codesign -d --entitlements :- "$app" 2>/dev/null || true)"
  if grep -q "com.apple.developer.game-center" <<<"$signed_entitlements"; then found=on; else found=off; fi
fi
if [ "$game_center" = on ]; then
  plutil -lint "$entitlements" >/dev/null || fail "$entitlements is not a valid property list"
  [ "$(/usr/libexec/PlistBuddy -c "Print :com.apple.developer.game-center" "$entitlements" 2>/dev/null || true)" = "true" ] \
    || fail "$entitlements has no com.apple.developer.game-center = true"
  if $signed; then
    [ "$found" = on ] || fail "the signature does not carry the Game Center entitlement: the App ID or the profile lacks the capability"
    echo "    entitlements: Game Center ON, in the file and in the signature"
  else
    echo "    entitlements: Game Center ON, in the file (an unsigned build embeds none)"
  fi
else
  if $signed; then
    [ "$found" = off ] || fail "Game Center was asked to be off, but the signature carries the entitlement"
    echo "    entitlements: Game Center OFF, the signature carries no Game Center entitlement (sign-in to Game Center will fail)"
  else
    echo "    entitlements: Game Center OFF (an unsigned build embeds none)"
  fi
fi

frameworks="$(otool -L "$exe" | tail -n +2 | awk '{print $1}')"
grep -q "/GameKit.framework/" <<<"$frameworks" || fail "the executable does not link GameKit (the Game Center score)"
grep -q "/AVFoundation.framework/" <<<"$frameworks" \
  || fail "the executable does not link AVFoundation (the sound of the game: Qt Multimedia cannot link on iOS)"
forbidden='AdSupport|AdServices|AppTrackingTransparency|StoreKit|CoreLocation|UserNotifications|PushKit|iAd|GoogleMobileAds|Firebase|Crashlytics'
bad="$(grep -E "/($forbidden)\.framework/" <<<"$frameworks" || true)"
[ -z "$bad" ] || fail "the executable links a framework v1 must never use (ads, tracking, purchases, location, notifications): $bad"
echo "    executable: links GameKit and AVFoundation; no advertising, tracking, purchase, location or notification framework"

# The sound files are compiled into the executable as Qt resources. Looking for each file's name in the UTF-16 name table
# is the cheap proof that they were linked in (a static library's resources are dropped by the linker when nothing
# refers to them), without a device to play them on.
audio_dir="$here/../audio"
audio_count=0
for wav in "$audio_dir"/*.wav; do
  [ -e "$wav" ] || fail "no sound file in $audio_dir"
  name="$(basename "$wav")"
  # A name is stored as 16-bit characters, big-endian: "ab" is the bytes 00 61 00 62.
  perl -0777 -e 'my $n = shift @ARGV; my $p = join("", map { "\\x00" . quotemeta($_) } split(//, $n)); exit(<> =~ /$p/ ? 0 : 1)' \
    "$name" "$exe" \
    || fail "the sound file $name is not in the executable: the game would start silent"
  audio_count=$((audio_count + 1))
done
echo "    audio: $audio_count sound files found in the executable as resources"

# Notes only: what could reach the network, as the linker sees it.
network_frameworks="$(grep -E '/(CFNetwork|Network|NetworkExtension|WebKit|SafariServices)\.framework/' <<<"$frameworks" | sed 's|.*/||' | tr '\n' ' ' || true)"
qt_network_symbols="$(nm -gU "$exe" 2>/dev/null | grep -c 'QNetwork' || true)"
session_symbols="$(nm -u "$exe" 2>/dev/null | grep -c -E 'NSURLSession|NSURLConnection' || true)"
echo "    note: network frameworks linked: ${network_frameworks:-none}; QNetwork symbols: $qt_network_symbols; NSURLSession or NSURLConnection references: $session_symbols"

if [ -e "$app/PrivacyInfo.xcprivacy" ]; then
  echo "    note: the bundle has a PrivacyInfo.xcprivacy"
else
  echo "    note: the bundle has no PrivacyInfo.xcprivacy (Apple may ask for the required-reason API declarations at upload)"
fi

bundle_kb="$(du -sk "$app" | awk '{print $1}')"
exe_kb="$(( $(stat -f %z "$exe") / 1024 ))"
echo "    size: bundle ${bundle_kb} KiB, executable ${exe_kb} KiB"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    echo "### iOS game bundle"
    echo
    echo "| | |"
    echo "|---|---|"
    echo "| Game Center entitlement | ${found:-$game_center} |"
    echo "| Bundle | ${bundle_kb} KiB |"
    echo "| Executable | ${exe_kb} KiB |"
    echo "| Network frameworks linked | ${network_frameworks:-none} |"
    echo "| QNetwork symbols | $qt_network_symbols |"
    echo "| NSURLSession or NSURLConnection references | $session_symbols |"
  } >> "$GITHUB_STEP_SUMMARY"
fi
echo "OK: $target (game checks)"
