# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/icl" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Chooses the vcpkg checkout for this configure. Must run before project().
include(IclVcpkgRoot)
icl_resolve_vcpkg("${CMAKE_CURRENT_SOURCE_DIR}" HEART_BOOM_MATHS)

project(heart_boom_maths
    VERSION 0.1.0
    DESCRIPTION "Cross-platform app and game for iOS, Android, WebAssembly and desktop"
    HOMEPAGE_URL "https://github.com/iainchesworthlabs/heart-boom-maths"
    LANGUAGES CXX)

set(ICL_PROJECT_PREFIX HEART_BOOM_MATHS)

# The application id on Android and the bundle identifier on iOS: com.iainchesworthlabs.<app>, the App ID registered with
# Apple and what the provisioning profile names. Defined once; the iOS workflows repeat it as BUNDLE_ID and
# apps/ios/packaging/verify_app.sh checks that the two agree.
set(HEART_BOOM_MATHS_APPLICATION_ID "com.iainchesworthlabs.heartboommaths")
set(HEART_BOOM_MATHS_DISPLAY_NAME "Heart Boom Maths")

# The iOS application's code is Objective-C++ (apps/ios).
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers apply to every target, so they are configured first.
include(IclSanitizers)
include(IclWarnings)
include(IclHardening)
include(IclTracy)
include(IclCoverage)
include(IclPlatform)

option(HEART_BOOM_MATHS_BUILD_TESTS "Build the tests" ON)
option(HEART_BOOM_MATHS_BUILD_CLI "Build the command-line application" ON)
option(HEART_BOOM_MATHS_BUILD_IOS_APP "Build the iPhone and iPad application (iOS only)" ON)

if(HEART_BOOM_MATHS_BUILD_TESTS)
    include(CTest)
    find_package(Catch2 3 CONFIG REQUIRED)
    include(Catch)
endif()

add_subdirectory(libs/core)

if(HEART_BOOM_MATHS_BUILD_CLI)
    add_subdirectory(apps/cli)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND HEART_BOOM_MATHS_BUILD_IOS_APP)
    add_subdirectory(apps/ios)
endif()
