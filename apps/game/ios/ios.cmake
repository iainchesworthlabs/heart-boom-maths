# SPDX-License-Identifier: GPL-3.0-or-later
# The iPhone and iPad bundle of the game. Included by apps/game/CMakeLists.txt when the target is iOS (the
# `ios-game-release` preset), after the heart_boom_maths_game target exists. Qt's qt_add_executable already makes the target
# a bundle on iOS; this file gives it the property list, the entitlements, the asset catalogue and the one framework the
# game links on its own (GameKit, for the Game Center score).
#
# Unsigned by default: the build needs no Apple account. A signed build is the release workflow's, with the owner's
# certificate and provisioning profile (docs/ios.md, docs/releasing.md).

# The same cache variables as apps/ios/CMakeLists.txt, which is not part of this build. The build number is CFBundleVersion;
# TestFlight refuses a build whose number is not higher than every earlier one for the same version.
set(HEART_BOOM_MATHS_IOS_BUILD_NUMBER "" CACHE STRING "Build number of the iOS application (empty: the project version)")
set(HEART_BOOM_MATHS_IOS_SIGNING_TEAM "" CACHE STRING "Apple team ID the iOS application is signed for (empty: an unsigned build)")
set(HEART_BOOM_MATHS_IOS_SIGNING_PROFILE "" CACHE STRING "Name of the provisioning profile the iOS application is signed with (empty: an unsigned build)")

if(HEART_BOOM_MATHS_IOS_BUILD_NUMBER)
    set(hbm_ios_build_number "${HEART_BOOM_MATHS_IOS_BUILD_NUMBER}")
else()
    set(hbm_ios_build_number "${PROJECT_VERSION}")
endif()

set(hbm_ios_assets "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
set(hbm_ios_entitlements "${CMAKE_CURRENT_SOURCE_DIR}/ios/heart-boom-maths.entitlements")

# A source path ending in .xcassets is what tells the Xcode generator to compile the catalogue, not copy it.
target_sources(heart_boom_maths_game PRIVATE "${hbm_ios_assets}")
set_source_files_properties("${hbm_ios_assets}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

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
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    # The Game Center capability. Xcode embeds the file only into a signed build.
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${hbm_ios_entitlements}")

# Objective-C++ with reference counting, like the platform sources of apps/ios. GameKit is the only Apple framework the
# game links itself: UIKit and Foundation come with Qt, and Qt's own frameworks come with its static libraries.
set_property(TARGET heart_boom_maths_game APPEND PROPERTY COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
target_link_libraries(heart_boom_maths_game PRIVATE "-framework GameKit" "-framework UIKit" "-framework Foundation")

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
