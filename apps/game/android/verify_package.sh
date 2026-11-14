#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Looks inside a finished Android package and fails on anything the build was meant to rule out. It reads the package alone
# (aapt2 from the SDK's build-tools), so it holds for what a person would install, not for what the build tree says:
# androiddeployqt merges Qt's own manifest rules, and the permissions Qt's core and network modules ask for by default are
# the thing to prove absent. The game asks for none.
#
#   verify_package.sh <package.apk|package.aab> [--version <x.y.z>] [--release]
#
#   --version  the versionName must be exactly this
#   --release  the package must not be debuggable
#
# An AAB is checked by its own, smaller rules: the bundle's manifest is in protocol buffer form, which aapt2 does not print,
# so its permissions are read as text from it. The exact proof is the APK built from the same sources, which the workflow
# checks too.
#
# Run by .github/workflows/android-build.yml. Exit 0 when everything holds, 1 when something does not, 2 for a bad call or
# a missing tool.
set -euo pipefail

package=""
version=""
release=false
while [ $# -gt 0 ]; do
  case "$1" in
    --version) version="${2:?--version needs a value}"; shift 2 ;;
    --release) release=true; shift ;;
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "verify_package.sh: unknown argument '$1'" >&2; exit 2 ;;
    *) [ -z "$package" ] || { echo "verify_package.sh: one package at a time" >&2; exit 2; }; package="$1"; shift ;;
  esac
done
[ -n "$package" ] || { echo "usage: verify_package.sh <package.apk|package.aab> [--version x.y.z] [--release]" >&2; exit 2; }
[ -f "$package" ] || { echo "verify_package.sh: $package is not a file" >&2; exit 2; }

# HEART_BOOM_MATHS_APPLICATION_ID in CMakeLists.txt, which this check holds the package to.
application_id="com.iainchesworthlabs.heartboommaths"
# qt_add_executable names the library after the target on Android (apps/game/CMakeLists.txt), with the architecture added.
library="lib/arm64-v8a/libheart_boom_maths_game_arm64-v8a.so"

status=0
bad() { echo "FAIL: $*" >&2; status=1; }
ok() { echo "ok:   $*"; }

# The entries of the package, from whichever lister the machine has.
list_entries() {
  if command -v unzip >/dev/null 2>&1; then unzip -Z1 "$package"
  elif command -v jar >/dev/null 2>&1; then jar tf "$package"
  else python3 -c 'import sys, zipfile; print(chr(10).join(zipfile.ZipFile(sys.argv[1]).namelist()))' "$package"
  fi
}
entries="$(list_entries | tr -d '\r')"
size="$(wc -c < "$package" | tr -d ' ')"
echo "package: $package ($size bytes)"

# One architecture, and the program's library is in it. (A bundle keeps its libraries under base/.)
case "$package" in
  *.aab) prefix="base/" ;;
  *) prefix="" ;;
esac
if printf '%s\n' "$entries" | grep -qx "$prefix$library"; then
  ok "the program's library $prefix$library is in the package"
else
  bad "the package has no $prefix$library"
fi
other_abis="$(printf '%s\n' "$entries" | sed -n "s#^${prefix}lib/\([^/]*\)/.*#\1#p" | sort -u | grep -vx 'arm64-v8a' || true)"
if [ -n "$other_abis" ]; then
  bad "the package carries libraries for other architectures: $(echo "$other_abis" | tr '\n' ' ')"
else
  ok "native code is arm64-v8a only"
fi

# One entry of the package, as bytes.
read_entry() {
  if command -v unzip >/dev/null 2>&1; then unzip -p "$package" "$1"
  else python3 -c 'import sys, zipfile; sys.stdout.buffer.write(zipfile.ZipFile(sys.argv[1]).read(sys.argv[2]))' "$package" "$1"
  fi
}

if [ "${package##*.}" = "aab" ]; then
  for entry in "${prefix}manifest/AndroidManifest.xml" "BundleConfig.pb"; do
    if printf '%s\n' "$entries" | grep -qx "$entry"; then ok "the bundle has $entry"; else bad "the bundle has no $entry"; fi
  done
  # The bundle's manifest is a protocol buffer, which keeps its element and attribute names as plain text, so a permission
  # cannot hide in it. Only AndroidX's own receiver permission is a uses-permission element, and android.permission.DUMP is
  # the guard that AndroidX's profile installer puts on its own receiver (an attribute that asks for nothing). Any other
  # android.permission.* is a permission the game asked for.
  found="$(read_entry "${prefix}manifest/AndroidManifest.xml" | grep -a -o 'uses-permission[A-Za-z0-9-]*\|android\.permission\.[A-Z_]*' | sort | uniq -c || true)"
  elements="$(printf '%s\n' "$found" | awk '$2 ~ /^uses-permission/ { n += $1 } END { print n + 0 }')"
  named="$(printf '%s\n' "$found" | awk '$2 ~ /^android\.permission\./ && $2 != "android.permission.DUMP" { print $2 }' | tr '\n' ' ')"
  if [ "$elements" -gt 1 ] || [ -n "$named" ]; then
    bad "the bundle's manifest has $elements uses-permission elements and names the permissions: ${named:-none besides DUMP}"
  else
    ok "the bundle's manifest asks for no permission (its $elements uses-permission element is AndroidX's own receiver permission)"
  fi
  if [ "$status" -ne 0 ]; then
    echo "verify_package.sh: $package does not hold" >&2
    exit 1
  fi
  echo "verify_package.sh: $package holds"
  exit 0
fi

# aapt2: on PATH, or the newest build-tools of the SDK.
find_tool() {
  local name="$1" found="" sdk dir candidate
  if found="$(command -v "$name" 2>/dev/null)"; then printf '%s' "$found"; return 0; fi
  sdk="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
  if [ -n "$sdk" ] && [ -d "$sdk/build-tools" ]; then
    # Newest first: build-tools directories are version numbers.
    while IFS= read -r dir; do
      for candidate in "$dir/$name" "$dir/$name.exe" "$dir/$name.bat"; do
        if [ -f "$candidate" ]; then printf '%s' "$candidate"; return 0; fi
      done
    done < <(find "$sdk/build-tools" -mindepth 1 -maxdepth 1 -type d | sort -Vr)
  fi
  return 1
}
aapt2="$(find_tool aapt2)" || { echo "verify_package.sh: aapt2 not found (put the SDK's build-tools on PATH or set ANDROID_SDK_ROOT)" >&2; exit 2; }
echo "aapt2: $aapt2"

badging="$("$aapt2" dump badging "$package" | tr -d '\r')"
manifest="$("$aapt2" dump xmltree --file AndroidManifest.xml "$package" | tr -d '\r')"

# The application id and the version.
if printf '%s\n' "$badging" | grep -q "^package: name='$application_id' "; then
  ok "the application id is $application_id"
else
  bad "the application id is not $application_id: $(printf '%s\n' "$badging" | sed -n 1p)"
fi
if [ -n "$version" ]; then
  if printf '%s\n' "$badging" | grep -q "^package: .* versionName='$version'"; then
    ok "versionName is $version"
  else
    bad "versionName is not $version: $(printf '%s\n' "$badging" | sed -n 1p)"
  fi
fi
code="$(printf '%s\n' "$badging" | sed -n "1s/.* versionCode='\([0-9]*\)'.*/\1/p")"
if [ -n "$code" ] && [ "$code" -gt 0 ]; then ok "versionCode is $code"; else bad "versionCode is missing or zero"; fi

# The platform levels: nothing older than Qt 6.12's floor, and a target that Google Play takes.
min_sdk="$(printf '%s\n' "$badging" | sed -n "s/^minSdkVersion:'\([0-9]*\)'.*/\1/p")"
target_sdk="$(printf '%s\n' "$badging" | sed -n "s/^targetSdkVersion:'\([0-9]*\)'.*/\1/p")"
if [ "${min_sdk:-0}" -ge 28 ]; then ok "minSdkVersion is $min_sdk"; else bad "minSdkVersion is '${min_sdk:-}', Qt 6.12 needs 28 or later"; fi
if [ "${target_sdk:-0}" -ge 35 ]; then ok "targetSdkVersion is $target_sdk"; else bad "targetSdkVersion is '${target_sdk:-}', Google Play wants 35 or later"; fi

# Permissions, from the merged manifest itself, so that every kind is seen (uses-permission, uses-permission-sdk-23 and the
# rest). The game asks for none. The one line allowed is AndroidX's own signature-level permission, in the application's
# own name, which it adds for receivers registered at run time: it asks nothing of the person and is granted to nobody else.
asked="$(printf '%s\n' "$manifest" | awk '
  /^[[:space:]]*E: uses-permission/ { inside = 1; next }
  inside && /A: .*:name\(/ { sub(/.*="/, ""); sub(/".*/, ""); print; inside = 0 }
  /^[[:space:]]*E: / && !/E: uses-permission/ { inside = 0 }' | sort -u)"
unexpected=""
while IFS= read -r name; do
  [ -n "$name" ] || continue
  case "$name" in
    "$application_id.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION") ;;
    *) unexpected="$unexpected $name" ;;
  esac
done <<<"$asked"
if printf '%s\n' "$manifest" | grep -q "E: uses-permission" && [ -z "$asked" ]; then
  bad "the manifest has a uses-permission element that this check could not read"
elif [ -n "$unexpected" ]; then
  bad "the package asks for permissions:$unexpected (the manifest is apps/game/android/AndroidManifest.xml; Qt's core module adds INTERNET and storage permissions wherever its placeholder is)"
else
  ok "the package asks for no permission (AndroidX's own receiver permission is the only line, and asks nothing of a person)"
fi

# Backup is off: what a child types never leaves the device, Google's cloud backup included.
if printf '%s\n' "$manifest" | grep -q 'allowBackup(0x[0-9a-f]*)=false'; then ok "allowBackup is false"; else bad "allowBackup is not false"; fi

# Portrait, which the design is for (docs/DECISIONS.md, 8). The value 1 is SCREEN_ORIENTATION_PORTRAIT.
if printf '%s\n' "$manifest" | grep -q 'screenOrientation(0x[0-9a-f]*)=1$'; then ok "the activity is locked to portrait"; else bad "the activity is not locked to portrait"; fi

# Debuggable or not.
if printf '%s\n' "$badging" | grep -q "^application-debuggable"; then
  if [ "$release" = true ]; then bad "the package is debuggable; a release must not be"; else ok "the package is debuggable (a development build)"; fi
else
  ok "the package is not debuggable"
fi

# The icon and a launcher activity.
if printf '%s\n' "$badging" | grep -q "^application-icon-"; then ok "the package has a launcher icon"; else bad "the package has no launcher icon"; fi
if printf '%s\n' "$badging" | grep -q "^launchable-activity:"; then ok "the package has a launchable activity"; else bad "the package has no launchable activity"; fi
if printf '%s\n' "$badging" | grep -q "^application-label:'Heart Boom Maths'"; then ok "the label is Heart Boom Maths"; else bad "the label is not Heart Boom Maths"; fi

if [ "$status" -ne 0 ]; then
  echo "verify_package.sh: $package does not hold" >&2
  exit 1
fi
echo "verify_package.sh: $package holds"
