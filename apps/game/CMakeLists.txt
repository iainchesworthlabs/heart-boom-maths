# SPDX-License-Identifier: GPL-3.0-or-later
# The Qt Quick application. Only this directory may hold a clock, a QObject or a QML type; libs/core stays free of Qt.
# The top-level CMakeLists.txt adds it when a Qt 6 kit is found (HEART_BOOM_MATHS_BUILD_GAME).
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickShapes QuickLayouts)
qt_standard_project_setup(REQUIRES 6.8)

include(IclQtApp)
include(HbmAndroid)
include(HbmBuildInfo)
include(HbmNotices)

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
    src/services/file_save_store.cpp
    src/services/notices.cpp
    src/services/system_link_opener.cpp)
target_include_directories(heart_boom_maths_app PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(heart_boom_maths_app
    PUBLIC heart_boom_maths::core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickShapes Qt6::QuickLayouts
    PRIVATE icl::warnings icl::hardening icl::coverage)
icl_relax_generated_warnings(heart_boom_maths_app)

# Version, build number, commit and Qt version, written at configure time (see cmake/HbmBuildInfo.cmake).
hbm_generate_build_info("${CMAKE_CURRENT_BINARY_DIR}/generated/build_info_data.hpp"
    TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/src/services/build_info_data.hpp.in"
    APP_NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
    QT_VERSION "${Qt6_VERSION}")

# One notices text for About: Qt, the dependencies (the configure fails when one has no licence file) and the fonts.
hbm_generate_notices_text("${CMAKE_CURRENT_BINARY_DIR}/generated/notices.txt"
    NAME "${HEART_BOOM_MATHS_DISPLAY_NAME}"
    QT_VERSION "${Qt6_VERSION}"
    EXCLUDE catch2
    LICENCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Bangers-OFL.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/LilitaOne-OFL.txt"
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
    src/services/backed_audio_service.cpp
    src/services/first_gesture_filter.cpp)
find_package(Qt6Multimedia "${Qt6_VERSION}" QUIET CONFIG HINTS "${Qt6_DIR}/..")
if(Qt6Multimedia_FOUND)
    target_sources(heart_boom_maths_app PRIVATE src/services/qt_audio_backend.cpp)
    target_link_libraries(heart_boom_maths_app PRIVATE Qt6::Multimedia)
else()
    message(STATUS "Qt Multimedia was not found: the game will be silent (install the qtmultimedia module of the Qt kit)")
    target_sources(heart_boom_maths_app PRIVATE src/services/no_audio_backend.cpp)
endif()
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

# The QML files sit at the root of the module (HeartBoomMaths/Main.qml), as the tools expect: qmllint finds the types of
# the other files in the directory only then. One file per line, so two branches that each add a screen rarely collide.
set(HEART_BOOM_MATHS_QML_FILES
    qml/AboutScreen.qml
    qml/AnswerGrid.qml
    qml/AnswerTile.qml
    qml/AppShell.qml
    qml/ArrowIcon.qml
    qml/BackIcon.qml
    qml/Badge.qml
    qml/ChevronIcon.qml
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
    qml/PlayHeart.qml
    qml/PlayHint.qml
    qml/PlayMetrics.qml
    qml/PlayScreen.qml
    qml/PlayStats.qml
    qml/PlayTopBar.qml
    qml/QuestionCard.qml
    qml/RowButton.qml
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
# Theme is a singleton: one set of tokens for every screen.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# The URI differs from the program name: on a case-insensitive file system "HeartBoomMaths" and "heart-boom-maths" must
# not name one directory in the build tree.
qt_add_qml_module(heart_boom_maths_app
    URI HeartBoomMaths
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/HeartBoomMaths"
    QML_FILES ${HEART_BOOM_MATHS_QML_FILES})

# The fonts are application resources, never fetched from a network. Their licence texts travel with them. They sit in the
# library so the tests that load the QML find them too.
qt_add_resources(heart_boom_maths_app fonts
    PREFIX /heart_boom_maths/fonts
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    FILES
        fonts/Bangers-Regular.ttf
        fonts/LilitaOne-Regular.ttf)

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
        NOTICE_EXCLUDE catch2)
    # The font licence asks for its notice and text to travel with the font.
    if(APPLE)
        set(font_licence_dir "heart-boom-maths.app/Contents/Resources/doc")
    else()
        set(font_licence_dir "share/doc/heart-boom-maths")
    endif()
    install(FILES fonts/Bangers-OFL.txt fonts/LilitaOne-OFL.txt DESTINATION "${font_licence_dir}")
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
        tests/test_event_sounds.cpp
        tests/test_file_save_store.cpp
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

    # The sound rules over a recording backend, the factory, the first-gesture filter and the placeholder sound files.
    target_sources(heart_boom_maths_game_tests PRIVATE tests/test_audio.cpp)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME game.audio_placeholders
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/assets/make_placeholder_sounds.py" --check
                --output-dir "${CMAKE_CURRENT_SOURCE_DIR}/audio")
        set_tests_properties(game.audio_placeholders PROPERTIES LABELS unit TIMEOUT 120)
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

    # The screens: the shell and the play screen load offscreen at both size classes, a scripted game drives the controller
    # and the tests read the QML object tree. Its own main starts a QGuiApplication, which the tests above do not need. A
    # QML warning fails the test (tests/ui_main.cpp). The `ui` tag of the cases becomes the CTest label.
    add_executable(heart_boom_maths_ui_tests
        tests/ui_main.cpp
        tests/ui_support.cpp
        tests/test_answer_feedback.cpp
        tests/test_ui_components.cpp
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
endif()
