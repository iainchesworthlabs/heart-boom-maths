# SPDX-License-Identifier: GPL-3.0-or-later
#
#   hbm_configure_android_app(<target>)
#
# Makes the application target the program of an Android phone or tablet. qt_add_executable has already made it a shared
# library (the activity loads it), and androiddeployqt (the `apk` and `aab` targets) turns that library, Qt's libraries and
# plugins, the QML modules the program imports and the files of apps/game/android into a package. There is no
# `cmake --install` and no deploy script on Android.
#
# The package is unsigned in the release configuration and signed with Android's debug key in the debug one. A package
# signed with the upload key is made only by the tag build (QT_ANDROID_SIGN_AAB and the QT_ANDROID_KEYSTORE_* variables,
# docs/releasing.md): the key is the owner's and is never in the repository.
#
# The application id is HEART_BOOM_MATHS_APPLICATION_ID, the same string as the iOS bundle identifier. The version name is
# the project version. The version code is the CI run number, so that it only ever goes up (Google Play refuses a code that
# is not higher than every earlier upload): ICL_BUILD_NUMBER, then GITHUB_RUN_NUMBER (the same two variables, in the same
# order, as cmake/HbmBuildInfo.cmake reads for the number About shows), and 1 on a build that is not on CI. A build that
# must go out under a number of its own passes -DHEART_BOOM_MATHS_ANDROID_VERSION_CODE=<number>.

set(HEART_BOOM_MATHS_ANDROID_VERSION_CODE "" CACHE STRING
    "versionCode of the Android package (empty: the CI run number, 1 off CI)")

# 28 is the lowest Qt 6.12 supports. The target and compile levels are the newest Google Play takes; a level the SDK in use
# lacks fails androiddeployqt by name. The SDK platform is pinned in .github/toolchain-versions.json (android.platform).
set(HEART_BOOM_MATHS_ANDROID_MIN_SDK 28)
set(HEART_BOOM_MATHS_ANDROID_TARGET_SDK 36)

function(hbm_configure_android_app target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "hbm_configure_android_app: '${target}' is not a target")
    endif()

    if(HEART_BOOM_MATHS_ANDROID_VERSION_CODE)
        set(version_code "${HEART_BOOM_MATHS_ANDROID_VERSION_CODE}")
    else()
        set(version_code 1)
        foreach(variable IN ITEMS ICL_BUILD_NUMBER GITHUB_RUN_NUMBER)
            if(DEFINED ENV{${variable}} AND "$ENV{${variable}}" MATCHES "^[0-9]+$")
                set(version_code "$ENV{${variable}}")
                break()
            endif()
        endforeach()
    endif()
    # Google Play's ceiling is 2100000000; a zero would read as "not set" to the tools.
    if(NOT version_code MATCHES "^[1-9][0-9]*$" OR version_code GREATER 2100000000)
        message(FATAL_ERROR "The Android version code '${version_code}' is not a whole number from 1 to 2100000000.")
    endif()
    message(STATUS "Android package: ${HEART_BOOM_MATHS_APPLICATION_ID} ${PROJECT_VERSION} (version code ${version_code})")

    # A package source directory is copied over Qt's own template by androiddeployqt: it holds AndroidManifest.xml (what
    # the program asks of the system) and res/ (the launcher icon and the launch colour).
    set_target_properties(${target} PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/apps/game/android"
        QT_ANDROID_PACKAGE_NAME "${HEART_BOOM_MATHS_APPLICATION_ID}"
        QT_ANDROID_APP_NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE "${version_code}"
        QT_ANDROID_MIN_SDK_VERSION "${HEART_BOOM_MATHS_ANDROID_MIN_SDK}"
        QT_ANDROID_TARGET_SDK_VERSION "${HEART_BOOM_MATHS_ANDROID_TARGET_SDK}"
        QT_ANDROID_COMPILE_SDK_VERSION "${HEART_BOOM_MATHS_ANDROID_TARGET_SDK}")
endfunction()
