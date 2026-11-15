# SPDX-License-Identifier: GPL-3.0-or-later
# The Qt Quick application. Only this directory may hold a clock, a QObject or a QML type; libs/core stays free of Qt.
# The top-level CMakeLists.txt adds it when a Qt 6 kit is found (HEART_BOOM_MATHS_BUILD_GAME).
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickShapes QuickLayouts)
qt_standard_project_setup(REQUIRES 6.8)

include(IclQtApp)
include(HbmAndroid)
include(HbmBuildInfo)
include(HbmNotices)
include(HbmWebSite)

# Everything the screens bind to, and the screens themselves as one static QML module (HeartBoomMaths), so the application
# and the ui tests load the same QML. The controller and AnswerFeedback are the module's C++ types; qml_types.hpp makes the
# controller known to qmllint without a macro in its own header.
add_library(heart_boom_maths_app STATIC
    src/answer_feedback.cpp
    src/event_sounds.cpp
    src/game_controller.cpp
    src/level_text.cpp
    src/parental_gate.cpp
    src/qml_types.hpp
    src/tick_driver.cpp
    src/services/build_info.cpp
    src/services/cloud_sync.cpp
    src/services/file_save_store.cpp
    src/services/game_center_score_service.cpp
    src/services/notices.cpp
    src/services/system_link_opener.cpp)
target_include_directories(heart_boom_maths_app PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(heart_boom_maths_app
    PUBLIC heart_boom_maths::core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickShapes Qt6::QuickLayouts
    PRIVATE icl::warnings icl::hardening icl::coverage)
icl_relax_generated_warnings(heart_boom_maths_app)

# The profiling marks of the app layer, and in a build with Tracy the scene graph's zones and the scripted run. With
# Tracy off the marks are empty and nothing of Tracy is built (cmake/HbmProfiling.cmake, docs/profiling.md).
hbm_add_profiling(heart_boom_maths_app)

# Version, build number, commit and Qt version, written at configure time (see cmake/HbmBuildInfo.cmake).
hbm_generate_build_info("${CMAKE_CURRENT_BINARY_DIR}/generated/build_info_data.hpp"
    TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/src/services/build_info_data.hpp.in"
    APP_NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
    QT_VERSION "${Qt6_VERSION}")

# One notices text for About: Qt, the dependencies (the configure fails when one has no licence file) and the fonts.
# Tracy is part of what ships when the build has it (dormant, a DLL or a library in the program), so its licence is in the
# notices; it is a feature of the manifest and not a dependency of it.
set(hbm_notice_packages)
if(ICL_ENABLE_TRACY)
    set(hbm_notice_packages tracy)
endif()
hbm_generate_notices_text("${CMAKE_CURRENT_BINARY_DIR}/generated/notices.txt"
    NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
    QT_VERSION "${Qt6_VERSION}"
    PACKAGES ${hbm_notice_packages}
    EXCLUDE catch2
    LICENCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Bangers-OFL.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/LilitaOne-OFL.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/HBMDisplay-OFL.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/HBMBody-OFL.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/audio/Sounds.txt")
qt_add_resources(heart_boom_maths_app notices
    PREFIX /heart_boom_maths
    BASE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/notices.txt")

# Sound. The rules of when a sound plays are plain C++ over a thin backend. The backend that talks to the sound device
# needs Qt Multimedia, an add-on module of the Qt kit; without it the game still builds and starts silent. The module's
# own package is looked for, not the component of Qt6, because a missing component must not fail the whole find.
target_sources(heart_boom_maths_app PRIVATE
    src/services/audio_assets.cpp
    src/services/audio_factory.cpp
    src/services/audio_resource.cpp
    src/services/backed_audio_service.cpp
    src/services/clip_audio_backend.cpp
    src/services/first_gesture_filter.cpp)
# HEART_BOOM_MATHS_USE_QT_MULTIMEDIA=OFF builds the silent fallback even when the kit has the module. iOS never uses the
# module: its FFmpeg backend cannot link there, so the iPhone and iPad sound is the AVFoundation backend of
# src/platform/ios/avf_audio_backend.mm (docs/audio.md), whatever this option says.
option(HEART_BOOM_MATHS_USE_QT_MULTIMEDIA "Link Qt Multimedia for sound when the kit has it (not on iOS)" ON)
# A variable of our own decides, because a Qt kit can set Qt6Multimedia_FOUND itself while the other packages load.
set(hbm_use_qt_audio OFF)
if(HEART_BOOM_MATHS_USE_QT_MULTIMEDIA AND NOT IOS)
    find_package(Qt6Multimedia "${Qt6_VERSION}" QUIET CONFIG HINTS "${Qt6_DIR}/..")
    if(Qt6Multimedia_FOUND)
        set(hbm_use_qt_audio ON)
    endif()
endif()
if(hbm_use_qt_audio)
    target_sources(heart_boom_maths_app PRIVATE src/services/qt_audio_backend.cpp)
    target_link_libraries(heart_boom_maths_app PRIVATE Qt6::Multimedia)
elseif(IOS)
    # make_device_audio_backend() is defined by the platform file of the application (icl_platform_sources below), over
    # AVFoundation, which ios/ios.cmake links.
    message(STATUS "Sound on iOS: the AVFoundation backend")
else()
    message(STATUS "Qt Multimedia is not used: the game will be silent (install the qtmultimedia module of the Qt kit, or turn HEART_BOOM_MATHS_USE_QT_MULTIMEDIA on)")
    target_sources(heart_boom_maths_app PRIVATE src/services/no_audio_backend.cpp)
endif()
# Where the save lives. The file store is for every platform with a file system; the browser has none that lasts, so its
# save is one value of the page's storage (QSettings), behind an interface a desktop test can fake. The platform decides
# which one src/platform/<os>/browser.cpp answers.
target_sources(heart_boom_maths_app PRIVATE
    src/services/app_save_store.cpp
    src/services/key_value_backend.cpp
    src/services/qsettings_backend.cpp
    src/services/web_save_store.cpp)

# Touch feedback. The rules of when a haptic is felt are plain C++ over a small backend and a timer; what each platform is
# asked for is a table (haptic_maps.cpp), and src/platform/<os>/haptics_platform.* only hands a beat to the system (UIKit on
# iOS, performHapticFeedback on Android, navigator.vibrate in a browser, nothing on the desktop).
target_sources(heart_boom_maths_app PRIVATE
    src/event_haptics.cpp
    src/services/backed_haptics_service.cpp
    src/services/haptic_maps.cpp
    src/services/haptic_patterns.cpp
    src/services/qt_haptic_timer.cpp)

# The files are made by tools/assets/make_placeholder_sounds.py and are replaced by a sound designer's work.
set(HEART_BOOM_MATHS_AUDIO_FILES
    audio/coin_chime.wav
    audio/soft_buzz.wav
    audio/rising_tone.wav
    audio/ticking.wav
    audio/cartoon_pop.wav
    audio/fanfare.wav
    audio/whoosh.wav
    audio/music_menu.wav
    audio/music_play.wav)
qt_add_resources(heart_boom_maths_app audio
    PREFIX /heart_boom_maths/audio
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/audio"
    FILES ${HEART_BOOM_MATHS_AUDIO_FILES})

# The tiles of the halftone page, made by tools/assets/make_halftone_tiles.py: one dot per tile, at 1x, 2x and 3x for each of
# the three grids. Qt picks the @2x or @3x file that matches the screen.
set(HEART_BOOM_MATHS_IMAGE_FILES
    images/halftone/dots-12.png
    images/halftone/dots-12@2x.png
    images/halftone/dots-12@3x.png
    images/halftone/dots-14.png
    images/halftone/dots-14@2x.png
    images/halftone/dots-14@3x.png
    images/halftone/dots-16.png
    images/halftone/dots-16@2x.png
    images/halftone/dots-16@3x.png)
qt_add_resources(heart_boom_maths_app images
    PREFIX /heart_boom_maths/images
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/images"
    FILES ${HEART_BOOM_MATHS_IMAGE_FILES})

# The QML files sit at the root of the module (HeartBoomMaths/Main.qml), as the tools expect: qmllint finds the types of
# the other files in the directory only then. One file per line, so two branches that each add a screen rarely collide.
set(HEART_BOOM_MATHS_QML_FILES
    qml/AboutScreen.qml
    qml/AlertFrame.qml
    qml/AnswerGrid.qml
    qml/AnswerTile.qml
    qml/AppRoot.qml
    qml/AppShell.qml
    qml/ArrowIcon.qml
    qml/BackIcon.qml
    qml/Badge.qml
    qml/ChevronIcon.qml
    qml/CloudIcon.qml
    qml/CoinCard.qml
    qml/CoinWallet.qml
    qml/ComicButton.qml
    qml/ComicCard.qml
    qml/ComicSwitch.qml
    qml/DashedBox.qml
    qml/FeedbackTag.qml
    qml/FuseBar.qml
    qml/GameOverScreen.qml
    qml/GearIcon.qml
    qml/Haptics.qml
    qml/HapticsIcon.qml
    qml/HalftoneBackground.qml
    qml/HeartBomb.qml
    qml/LevelClearScreen.qml
    qml/Main.qml
    qml/MusicIcon.qml
    qml/NameEntryScreen.qml
    qml/NameKeyboard.qml
    qml/ParentalGateScreen.qml
    qml/PauseIcon.qml
    qml/PauseOverlay.qml
    qml/Placeholder.qml
    qml/PlayFuse.qml
    qml/PlayGap.qml
    qml/PlayHeart.qml
    qml/PlayHint.qml
    qml/PlayMetrics.qml
    qml/PlayScreen.qml
    qml/PlayStats.qml
    qml/PlayTopBar.qml
    qml/PlayersScreen.qml
    qml/QuestionCard.qml
    qml/RowButton.qml
    qml/SafeAreaFrame.qml
    qml/ScorePage.qml
    qml/ScoreRow.qml
    qml/ScreenFrame.qml
    qml/SettingRow.qml
    qml/SettingsScreen.qml
    qml/SoundIcon.qml
    qml/SparkStar.qml
    qml/StarBurst.qml
    qml/StatRow.qml
    qml/TextViewer.qml
    qml/Theme.qml
    qml/TitleScreen.qml
    qml/Top10Screen.qml
    qml/TrophyIcon.qml
    qml/WinScreen.qml)
foreach(qml_file IN LISTS HEART_BOOM_MATHS_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
# Theme is a singleton: one set of tokens for every screen. Haptics is one too: the controls' way to the controller's tick.
set_source_files_properties(qml/Theme.qml qml/Haptics.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# The URI differs from the program name: on a case-insensitive file system "HeartBoomMaths" and "heart-boom-maths" must
# not name one directory in the build tree.
qt_add_qml_module(heart_boom_maths_app
    URI HeartBoomMaths
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/HeartBoomMaths"
    QML_FILES ${HEART_BOOM_MATHS_QML_FILES})

# The fonts are application resources, never fetched from a network. Their licence texts travel with them. They sit in the
# library so the tests that load the QML find them too. The two HBM fonts are the same fonts with a crossed 7; Theme.qml
# loads them instead of the originals while its `crossedSeven` switch is on.
qt_add_resources(heart_boom_maths_app fonts
    PREFIX /heart_boom_maths/fonts
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    FILES
        fonts/Bangers-Regular.ttf
        fonts/LilitaOne-Regular.ttf
        fonts/HBMDisplay-Regular.ttf
        fonts/HBMBody-Regular.ttf)

qt_add_executable(heart_boom_maths_game src/main.cpp)
# The program is named heart-boom-maths while the target is heart_boom_maths_game, so the QML module and the target stay
# distinct. (Not on Android, where the name of the library is the target's: androiddeployqt and the manifest go by it.)
if(NOT ANDROID)
    set_target_properties(heart_boom_maths_game PROPERTIES OUTPUT_NAME heart-boom-maths)
endif()
# The deploy step reads the QML the program imports from this directory.
set_target_properties(heart_boom_maths_game PROPERTIES QT_QML_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/qml")

# One directory per operating system holds the code that cannot be shared; CMake adds the one that matches the target.
icl_platform_sources(heart_boom_maths_game)

# The iOS bundle (property list, entitlements, icon, GameKit) is described beside the files it names.
if(IOS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/ios/ios.cmake")
endif()

# The browser build is a site, not a program: the page, its script and style, Qt's loader and the module in <build>/site.
if(EMSCRIPTEN)
    hbm_add_web_site(heart_boom_maths_game
        WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web"
        TITLE "${HEART_BOOM_MATHS_DISPLAY_NAME}"
        LICENCE_FILE "${PROJECT_SOURCE_DIR}/LICENSE"
        NOTICES_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/notices.txt")
endif()

target_include_directories(heart_boom_maths_game PRIVATE src)
# The static QML module needs its backing library and its plugin, which registers the types and the QML files.
target_link_libraries(heart_boom_maths_game
    PRIVATE heart_boom_maths_app heart_boom_maths_appplugin heart_boom_maths::core Qt6::Quick icl::warnings icl::hardening
            icl::coverage)
# qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
icl_relax_generated_warnings(heart_boom_maths_game)

if(ANDROID)
    # A phone or a tablet: no installer. The package is androiddeployqt's, from the target's properties and the files of
    # apps/game/android (cmake/HbmAndroid.cmake). The notices About shows are generated above, as everywhere.
    hbm_configure_android_app(heart_boom_maths_game)
endif()

if(NOT (ANDROID OR IOS OR EMSCRIPTEN))
    # Installs the program, copies the Qt libraries, plugins and QML modules next to it so the installed tree runs on a
    # machine without Qt, and adds the licence, notices and source information. catch2 is used by the tests only.
    icl_install_qt_app(heart_boom_maths_game
        BUNDLE_IDENTIFIER "${HEART_BOOM_MATHS_APPLICATION_ID}"
        DISPLAY_NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
        NOTICE_PACKAGES ${hbm_notice_packages}
        NOTICE_EXCLUDE catch2)
    # Tracy's client is a DLL beside the program on Windows (vcpkg's x64-windows triplet), and it is there in a release too,
    # dormant: the program needs it to start (docs/profiling.md).
    if(ICL_ENABLE_TRACY AND WIN32)
        install(FILES "$<TARGET_FILE:Tracy::TracyClient>" DESTINATION bin)
    endif()
    # The font licence asks for its notice and text to travel with the font.
    if(APPLE)
        set(font_licence_dir "heart-boom-maths.app/Contents/Resources/doc")
    else()
        set(font_licence_dir "share/doc/heart-boom-maths")
    endif()
    install(FILES fonts/Bangers-OFL.txt fonts/LilitaOne-OFL.txt fonts/HBMDisplay-OFL.txt fonts/HBMBody-OFL.txt DESTINATION "${font_licence_dir}")
endif()

if(HEART_BOOM_MATHS_BUILD_TESTS)
    # A display is not needed: the offscreen platform and the software renderer draw the first frame.
    set(game_test_environment QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic)

    # The app layer without a screen: the controller, the tick driver, the gate and the services. Every test runs on the
    # offscreen platform. A Qt warning aborts a test (tests/test_main.cpp). The `unit` and `integration` tags of the
    # cases become the CTest labels.
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(heart_boom_maths_game_tests
        tests/test_main.cpp
        tests/test_build_info.cpp
        tests/test_cloud_controller.cpp
        tests/test_cloud_sync.cpp
        tests/test_event_sounds.cpp
        tests/test_file_save_store.cpp
        tests/test_game_center.cpp
        tests/test_game_controller.cpp
        tests/test_level_text.cpp
        tests/test_parental_gate.cpp
        tests/test_tick_driver.cpp)
    target_link_libraries(heart_boom_maths_game_tests
        PRIVATE heart_boom_maths_app Qt6::Test Catch2::Catch2 icl::warnings icl::coverage)
    icl_relax_generated_warnings(heart_boom_maths_game_tests)
    # PRE_TEST: the list of cases is read when ctest runs, not at build time, when the Qt libraries may not be on PATH.
    catch_discover_tests(heart_boom_maths_game_tests
        TEST_PREFIX "game."
        TEST_SPEC "[unit]"
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS unit TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    catch_discover_tests(heart_boom_maths_game_tests
        TEST_PREFIX "game."
        TEST_SPEC "[integration]"
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS integration TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # The browser's save over a fake key-value store (blocked storage, a full quota, a damaged save kept under a second
    # key), the same store over QSettings on the desktop, the factory that picks the store, and the hidden-tab path.
    target_sources(heart_boom_maths_game_tests PRIVATE tests/test_web_save_store.cpp)

    # The haptic rules over a fake backend, timer and clock, the event table and the platform tables.
    target_sources(heart_boom_maths_game_tests PRIVATE tests/test_haptics.cpp)

    # The sound rules over a recording backend, the factory, the first-gesture filter and the placeholder sound files.
    target_sources(heart_boom_maths_game_tests PRIVATE tests/test_audio.cpp tests/test_clip_audio.cpp)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME game.audio_placeholders
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/assets/make_placeholder_sounds.py" --check
                --output-dir "${CMAKE_CURRENT_SOURCE_DIR}/audio")
        set_tests_properties(game.audio_placeholders PROPERTIES LABELS unit TIMEOUT 120)
        # The placeholder icon of the iOS bundle, drawn by a script, is the picture the script draws.
        add_test(NAME game.icon_placeholder
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/assets/make_placeholder_icon.py" --check
                --output "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png")
        set_tests_properties(game.icon_placeholder PROPERTIES LABELS unit TIMEOUT 120)
        # The halftone tiles are the pictures their script draws.
        add_test(NAME game.halftone_tiles
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/assets/make_halftone_tiles.py" --check)
        set_tests_properties(game.halftone_tiles PROPERTIES LABELS unit TIMEOUT 120)
    else()
        message(STATUS "No Python 3 found: game.audio_placeholders is not added")
    endif()

    # The build information script, run with a CI build number to see it end up in the header.
    add_test(NAME game.build_info_script
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/build_info_check"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/build_info_check.cmake")
    set_tests_properties(game.build_info_script PROPERTIES LABELS unit TIMEOUT 60)

    # Starts the real application, waits for the first frame and exits.
    add_test(NAME game.smoke COMMAND heart_boom_maths_game --smoke-test)
    set_tests_properties(game.smoke PROPERTIES LABELS ui TIMEOUT 60 ENVIRONMENT "${game_test_environment}")

    # The version is answered before Qt starts, so the test needs no window system at all. The pattern is not anchored,
    # because Qt Multimedia may print a plugin-loading note to the same stream before main runs.
    add_test(NAME game.version COMMAND heart_boom_maths_game --version)
    set_tests_properties(game.version PROPERTIES
        LABELS integration
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=there-is-no-such-plugin"
        PASS_REGULAR_EXPRESSION "heart-boom-maths ${PROJECT_VERSION} \\(")

    # Profiling (docs/profiling.md). The run-time switch is plain code and is tested in every build; what only a build with
    # Tracy contains (the scripted run, the reader of the scene graph's log) is tested in that build.
    target_sources(heart_boom_maths_game_tests PRIVATE tests/test_profile_switch.cpp)
    if(ICL_ENABLE_TRACY)
        target_sources(heart_boom_maths_game_tests PRIVATE tests/test_profile_run.cpp)
    endif()
    if(Python3_Interpreter_FOUND)
        # A build without Tracy has no Tracy in it, in the program or in the core (whose only Tracy include is profile.hpp,
        # tools/ci/check_core_includes.py), and a build with it does and says so, which keeps the check from going blind.
        if(ICL_ENABLE_TRACY)
            set(hbm_tracy_expectation --present)
        else()
            set(hbm_tracy_expectation --absent)
        endif()
        add_test(NAME game.tracy_binary
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/ci/check_binary_tracy.py" ${hbm_tracy_expectation}
                "$<TARGET_FILE:heart_boom_maths_game>")
        add_test(NAME game.tracy_in_core
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/ci/check_binary_tracy.py" ${hbm_tracy_expectation}
                "$<TARGET_FILE:heart_boom_maths_core>")
        set_tests_properties(game.tracy_binary game.tracy_in_core PROPERTIES LABELS unit TIMEOUT 120)
    endif()
    if(ICL_ENABLE_TRACY)
        # The dormant rule of the standard: a build that has Tracy starts nothing unless it is asked to, by a flag or by the
        # environment. A profile preset is asked by its own default, so it has no test for the quiet start.
        if(NOT HEART_BOOM_MATHS_PROFILE_AUTOSTART)
            add_test(NAME game.profiler_dormant COMMAND heart_boom_maths_game --smoke-test)
            set_tests_properties(game.profiler_dormant PROPERTIES
                LABELS integration TIMEOUT 60 ENVIRONMENT "${game_test_environment}"
                FAIL_REGULAR_EXPRESSION "Tracy profiler started")
        endif()
        add_test(NAME game.profiler_declined COMMAND heart_boom_maths_game --smoke-test --no-profile)
        set_tests_properties(game.profiler_declined PROPERTIES
            LABELS integration TIMEOUT 60 ENVIRONMENT "${game_test_environment};HBM_PROFILE=1"
            FAIL_REGULAR_EXPRESSION "Tracy profiler started")
        add_test(NAME game.profiler_asked_by_flag COMMAND heart_boom_maths_game --smoke-test --profile)
        set_tests_properties(game.profiler_asked_by_flag PROPERTIES
            LABELS integration TIMEOUT 60 ENVIRONMENT "${game_test_environment}"
            PASS_REGULAR_EXPRESSION "Tracy profiler started")
        add_test(NAME game.profiler_asked_by_environment COMMAND heart_boom_maths_game --smoke-test)
        set_tests_properties(game.profiler_asked_by_environment PROPERTIES
            LABELS integration TIMEOUT 60 ENVIRONMENT "${game_test_environment};HBM_PROFILE=1"
            PASS_REGULAR_EXPRESSION "Tracy profiler started")
        if(Python3_Interpreter_FOUND)
            # Nothing listens unless the game is asked to, and when it is, one loopback listener opens, once.
            if(HEART_BOOM_MATHS_PROFILE_AUTOSTART)
                set(hbm_quiet_arguments --no-profile)
            else()
                set(hbm_quiet_arguments)
            endif()
            add_test(NAME game.no_listener_when_not_asked
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/ci/check_listener.py" --silent --port 18087
                    "$<TARGET_FILE:heart_boom_maths_game>" ${hbm_quiet_arguments})
            add_test(NAME game.loopback_listener_when_asked
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/ci/check_listener.py" --listening --port 18088
                    "$<TARGET_FILE:heart_boom_maths_game>" --profile)
            set_tests_properties(game.no_listener_when_not_asked game.loopback_listener_when_asked PROPERTIES
                LABELS integration TIMEOUT 120 ENVIRONMENT "${game_test_environment}")
        endif()
        # The scripted run plays whole games on a real window and needs a display and a GPU: a developer's machine and
        # not CI. It is run by hand (docs/profiling.md).
    else()
        # Without Tracy the scripted run is refused, not silently ignored.
        add_test(NAME game.profile_run_refused COMMAND heart_boom_maths_game --smoke-test --profile-run 5)
        set_tests_properties(game.profile_run_refused PROPERTIES
            LABELS integration TIMEOUT 60 ENVIRONMENT "${game_test_environment}"
            PASS_REGULAR_EXPRESSION "needs a profiling build")
    endif()

    # The screens: the shell and the play screen load offscreen at both size classes, a scripted game drives the controller
    # and the tests read the QML object tree. Its own main starts a QGuiApplication, which the tests above do not need. A
    # QML warning fails the test (tests/ui_main.cpp). The `ui` tag of the cases becomes the CTest label.
    add_executable(heart_boom_maths_ui_tests
        tests/ui_main.cpp
        tests/ui_support.cpp
        tests/test_answer_feedback.cpp
        tests/test_ui_cloud.cpp
        tests/test_ui_components.cpp
        tests/test_ui_haptics.cpp
        tests/test_ui_journeys.cpp
        tests/test_ui_play.cpp
        tests/test_ui_scores.cpp
        tests/test_ui_screens.cpp)
    target_link_libraries(heart_boom_maths_ui_tests
        PRIVATE heart_boom_maths_app heart_boom_maths_appplugin Qt6::Test Catch2::Catch2 icl::warnings icl::coverage)
    target_include_directories(heart_boom_maths_ui_tests PRIVATE tests)
    # The offscreen platform has no system fonts and warns about it; the test points it at the game's own fonts. The
    # program sets the offscreen platform and the software renderer itself (tests/ui_main.cpp), because a list of
    # variables cannot be passed through catch_discover_tests.
    target_compile_definitions(heart_boom_maths_ui_tests PRIVATE HBM_TEST_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts")
    icl_relax_generated_warnings(heart_boom_maths_ui_tests)
    catch_discover_tests(heart_boom_maths_ui_tests
        TEST_PREFIX "ui."
        TEST_SPEC "[ui]"
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS ui TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    catch_discover_tests(heart_boom_maths_ui_tests
        TEST_PREFIX "ui."
        TEST_SPEC "[unit]"
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS unit TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # qmllint over the QML files; any warning fails (qml/.qmllint.ini).
    add_test(NAME game.qmllint
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target heart_boom_maths_app_qmllint)
    set_tests_properties(game.qmllint PROPERTIES LABELS ui TIMEOUT 120)

    # The performance tests (label perf), only when a perf preset asks for them (docs/testing.md, "Performance tests"). The
    # main program times the controller, the save and the screens on the offscreen platform with the software renderer; the
    # GPU program is the optional one that reads the GPU's frame time and skips itself unless HBM_PERF_GPU=1. Neither is
    # linked with coverage. (Unlike the core's, they do not replace operator new to count allocations: Qt's libraries free
    # what the program allocates, and a replacement the libraries do not share crashes them.)
    if(HEART_BOOM_MATHS_BUILD_PERF_TESTS)
        find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
        set(hbm_perf_core_dir "${PROJECT_SOURCE_DIR}/libs/core/tests/perf")
        if(WIN32)
            set(hbm_process_memory_source tests/perf/process_memory_windows.cpp)
        else()
            set(hbm_process_memory_source tests/perf/process_memory_proc.cpp)
        endif()
        set(hbm_perf_definitions
            HBM_TEST_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts"
            HBM_PERF_BUDGETS_FILE="${PROJECT_SOURCE_DIR}/tools/perf-budgets.json"
            _CRT_SECURE_NO_WARNINGS) # MSVC calls getenv unsafe; the harness reads two switches with it

        add_executable(heart_boom_maths_game_perf
            tests/perf/perf_main.cpp
            tests/perf/perf_app.cpp
            tests/perf/perf_ui.cpp
            tests/ui_support.cpp
            "${hbm_process_memory_source}")
        target_include_directories(heart_boom_maths_game_perf PRIVATE tests tests/perf "${hbm_perf_core_dir}")
        target_compile_definitions(heart_boom_maths_game_perf PRIVATE ${hbm_perf_definitions})
        target_link_libraries(heart_boom_maths_game_perf
            PRIVATE heart_boom_maths_app heart_boom_maths_appplugin Qt6::Test Catch2::Catch2 icl::warnings)
        icl_relax_generated_warnings(heart_boom_maths_game_perf)
        catch_discover_tests(heart_boom_maths_game_perf
            TEST_PREFIX "perf."
            TEST_SPEC "[perf]"
            DISCOVERY_MODE PRE_TEST
            PROPERTIES LABELS perf TIMEOUT 900
                ENVIRONMENT "HBM_PERF_RESULTS_DIR=${PROJECT_BINARY_DIR}/perf-results")

        add_executable(heart_boom_maths_game_perf_gpu
            tests/perf/perf_gpu_main.cpp
            tests/perf/perf_gpu.cpp
            tests/ui_support.cpp)
        target_include_directories(heart_boom_maths_game_perf_gpu PRIVATE tests tests/perf "${hbm_perf_core_dir}")
        target_compile_definitions(heart_boom_maths_game_perf_gpu PRIVATE ${hbm_perf_definitions})
        # QRhi is a header of Qt Gui's non-public API; the test only reads the GPU time from it.
        target_link_libraries(heart_boom_maths_game_perf_gpu
            PRIVATE heart_boom_maths_app heart_boom_maths_appplugin Qt6::Test Qt6::GuiPrivate Catch2::Catch2 icl::warnings)
        icl_relax_generated_warnings(heart_boom_maths_game_perf_gpu)
        # A test that skips itself exits with 4, which CTest reports as skipped and not as a failure.
        catch_discover_tests(heart_boom_maths_game_perf_gpu
            TEST_PREFIX "perf."
            TEST_SPEC "[perf-gpu]"
            DISCOVERY_MODE PRE_TEST
            PROPERTIES LABELS perf TIMEOUT 300 SKIP_RETURN_CODE 4
                ENVIRONMENT "HBM_PERF_RESULTS_DIR=${PROJECT_BINARY_DIR}/perf-results")
    endif()
endif()
