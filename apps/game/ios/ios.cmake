# SPDX-License-Identifier: GPL-3.0-or-later
# The iPhone and iPad bundle of the game. Included by apps/game/CMakeLists.txt when the target is iOS (the
# `ios-game-release` preset), after the heart_boom_maths_game target exists. Qt's qt_add_executable already makes the target
# a bundle on iOS; this file gives it the property list, the entitlements, the asset catalogue and the frameworks the game
# links on its own (GameKit for the Game Center score, AVFoundation for sound, CoreHaptics for the Taptic Engine check).
# iCloud sync needs no framework of its own: NSUbiquitousKeyValueStore is in Foundation.
#
# Unsigned by default: the build needs no Apple account. A signed build is the release workflow's, with the owner's
# certificate and provisioning profile (docs/ios.md, docs/releasing.md).

# The same cache variables as apps/ios/CMakeLists.txt, which is not part of this build. The build number is CFBundleVersion;
# TestFlight refuses a build whose number is not higher than every earlier one for the same version.
set(HEART_BOOM_MATHS_IOS_BUILD_NUMBER "" CACHE STRING "Build number of the iOS application (empty: the project version)")
set(HEART_BOOM_MATHS_IOS_SIGNING_TEAM "" CACHE STRING "Apple team ID the iOS application is signed for (empty: an unsigned build)")
set(HEART_BOOM_MATHS_IOS_SIGNING_PROFILE "" CACHE STRING "Name of the provisioning profile the iOS application is signed with (empty: an unsigned build)")

# Whether the bundle asks for the Game Center capability. The capability must be on the App ID and in the provisioning profile
# the build is signed with, or Xcode refuses to sign ("not in the provisioning profile"). The release workflow reads the
# profile and turns this ON only when the profile carries it, so a build can go to TestFlight before the owner has turned
# the capability on. OFF leaves out the entitlement and nothing else: the GameKit code stays, Info.plist still lists
# gamekit, and the sign-in simply never succeeds, which GameCenterScoreService treats as a player who is not signed in.
option(HEART_BOOM_MATHS_IOS_GAME_CENTER "Request the Game Center entitlement in the iOS bundle (the signing profile must carry it)" ON)

# The same rule for iCloud sync: the iCloud key-value store entitlement (com.apple.developer.ubiquity-kvstore-identifier) goes
# in only when the provisioning profile carries it, and the release workflow turns this ON only then. OFF leaves out the
# entitlement and nothing else: the code stays, but Apple requires the entitlement to use the store, so the Settings switch
# is not offered (the platform file reads HBM_ICLOUD_ENTITLED, set below). An unsigned build works with it OFF or ON.
option(HEART_BOOM_MATHS_IOS_ICLOUD "Request the iCloud key-value store entitlement in the iOS bundle (the signing profile must carry it)" ON)

if(HEART_BOOM_MATHS_IOS_BUILD_NUMBER)
    set(hbm_ios_build_number "${HEART_BOOM_MATHS_IOS_BUILD_NUMBER}")
else()
    set(hbm_ios_build_number "${PROJECT_VERSION}")
endif()

set(hbm_ios_assets "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
# The entitlements file is written for the state of the two options above, at the root of the build directory, and is the file
# Xcode signs with and verify_game_app.sh reads.
set(hbm_ios_entitlements_template "${CMAKE_CURRENT_SOURCE_DIR}/ios/heart-boom-maths.entitlements.in")
set(hbm_ios_entitlements "${CMAKE_BINARY_DIR}/heart-boom-maths.entitlements")
set(hbm_ios_privacy_manifest "${CMAKE_CURRENT_SOURCE_DIR}/ios/PrivacyInfo.xcprivacy")

# A source path ending in .xcassets is what tells the Xcode generator to compile the catalogue, not copy it.
target_sources(heart_boom_maths_game PRIVATE "${hbm_ios_assets}")
set_source_files_properties("${hbm_ios_assets}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# Apple's privacy manifest (no tracking, no collected data, the required-reason APIs the game and Qt call). It goes into the
# root of the bundle, where Apple's upload check looks for it.
target_sources(heart_boom_maths_game PRIVATE "${hbm_ios_privacy_manifest}")
set_source_files_properties("${hbm_ios_privacy_manifest}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

set_target_properties(heart_boom_maths_game PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${HEART_BOOM_MATHS_APPLICATION_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${hbm_ios_build_number}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (C) Iain Chesworth Labs contributors"
    # iPhone and iPad.
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    # The icon set Info.plist.in's CFBundleIconName names.
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

# The capabilities. Xcode embeds the entitlements file only into a signed build. The keys follow the two options, and the file
# is not given to Xcode at all when both are OFF. The value of the key-value store entry is Xcode's own: the team prefix and
# the bundle identifier, which is what the profile of the App ID holds.
set(HBM_ENTITLEMENT_ENTRIES "")
if(HEART_BOOM_MATHS_IOS_GAME_CENTER)
    string(APPEND HBM_ENTITLEMENT_ENTRIES "    <key>com.apple.developer.game-center</key>\n    <true/>\n")
endif()
if(HEART_BOOM_MATHS_IOS_ICLOUD)
    string(APPEND HBM_ENTITLEMENT_ENTRIES "    <key>com.apple.developer.ubiquity-kvstore-identifier</key>\n    <string>$(TeamIdentifierPrefix)$(CFBundleIdentifier)</string>\n")
    set(hbm_icloud_entitled 1)
else()
    set(hbm_icloud_entitled 0)
endif()
configure_file("${hbm_ios_entitlements_template}" "${hbm_ios_entitlements}" @ONLY)
if(HEART_BOOM_MATHS_IOS_GAME_CENTER OR HEART_BOOM_MATHS_IOS_ICLOUD)
    set_target_properties(heart_boom_maths_game PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${hbm_ios_entitlements}")
endif()
if(HEART_BOOM_MATHS_IOS_GAME_CENTER)
    message(STATUS "iOS Game Center entitlement: ON (${hbm_ios_entitlements})")
else()
    message(STATUS "iOS Game Center entitlement: OFF (the bundle is signed without it; sign-in to Game Center will fail)")
endif()
if(HEART_BOOM_MATHS_IOS_ICLOUD)
    message(STATUS "iOS iCloud key-value store entitlement: ON (${hbm_ios_entitlements})")
else()
    message(STATUS "iOS iCloud key-value store entitlement: OFF (the bundle is signed without it; the iCloud sync switch is not offered)")
endif()
# Whether the bundle was built with the entitlement, for src/platform/ios/cloud_platform.mm: a value it reads, not a condition.
target_compile_definitions(heart_boom_maths_game PRIVATE HBM_ICLOUD_ENTITLED=${hbm_icloud_entitled})

# Objective-C++ with reference counting, like the platform sources of apps/ios. GameKit, AVFoundation and CoreHaptics are
# the Apple frameworks the game links itself (src/platform/ios/score_platform.mm, avf_audio_backend.mm and
# haptics_platform.mm): Qt's own frameworks come with its static libraries.
set_property(TARGET heart_boom_maths_game APPEND PROPERTY COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
target_link_libraries(heart_boom_maths_game PRIVATE "-framework GameKit" "-framework AVFoundation" "-framework CoreHaptics" "-framework UIKit" "-framework Foundation")

# Signing, for a build that is signed. They sign the application target alone: signing every Xcode target forces Xcode to
# sign static libraries, which have no bundle identifier and were never meant to be signed.
if(HEART_BOOM_MATHS_IOS_SIGNING_TEAM AND HEART_BOOM_MATHS_IOS_SIGNING_PROFILE)
    set_target_properties(heart_boom_maths_game PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${HEART_BOOM_MATHS_IOS_SIGNING_TEAM}"
        XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${HEART_BOOM_MATHS_IOS_SIGNING_PROFILE}")
elseif(HEART_BOOM_MATHS_IOS_SIGNING_TEAM OR HEART_BOOM_MATHS_IOS_SIGNING_PROFILE)
    message(FATAL_ERROR "HEART_BOOM_MATHS_IOS_SIGNING_TEAM and HEART_BOOM_MATHS_IOS_SIGNING_PROFILE go together: one of them is set "
        "and the other is empty. Give both for a signed build, or neither for an unsigned one (docs/ios.md).")
endif()
