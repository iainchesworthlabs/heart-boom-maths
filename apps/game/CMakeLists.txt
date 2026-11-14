# SPDX-License-Identifier: GPL-3.0-or-later
# The Qt Quick application. Only this directory may hold a clock, a QObject or a QML type; libs/core stays free of Qt.
# The top-level CMakeLists.txt adds it when a Qt 6 kit is found (HEART_BOOM_MATHS_BUILD_GAME).
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.8)

include(IclQtApp)
include(HbmBuildInfo)
include(HbmNotices)

# Everything the screens bind to, as a library so the tests link it without the QML module. It holds no screen.
add_library(heart_boom_maths_app STATIC
    src/event_sounds.cpp
    src/game_controller.cpp
    src/parental_gate.cpp
    src/tick_driver.cpp
    src/services/build_info.cpp
    src/services/file_save_store.cpp
    src/services/notices.cpp
    src/services/system_link_opener.cpp)
target_include_directories(heart_boom_maths_app PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(heart_boom_maths_app
    PUBLIC heart_boom_maths::core Qt6::Core Qt6::Gui
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
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts/LilitaOne-OFL.txt")
qt_add_resources(heart_boom_maths_app notices
    PREFIX /heart_boom_maths
    BASE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/notices.txt")

qt_add_executable(heart_boom_maths_game src/main.cpp)
# The program is named heart-boom-maths while the target is heart_boom_maths_game, so the QML module and the target stay
# distinct. (Not on Android, where the name of the library is the target's: androiddeployqt and the manifest go by it.)
if(NOT ANDROID)
    set_target_properties(heart_boom_maths_game PROPERTIES OUTPUT_NAME heart-boom-maths)
endif()

# The QML files sit at the root of the module (HeartBoomMaths/Main.qml), as the tools expect: qmllint finds the types of
# the other files in the directory only then.
set(HEART_BOOM_MATHS_QML_FILES qml/Main.qml)
foreach(qml_file IN LISTS HEART_BOOM_MATHS_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()

# The URI differs from the program name: on a case-insensitive file system "HeartBoomMaths" and "heart-boom-maths" must
# not name one directory in the build tree.
qt_add_qml_module(heart_boom_maths_game
    URI HeartBoomMaths
    VERSION 1.0
    QML_FILES ${HEART_BOOM_MATHS_QML_FILES})

# One directory per operating system holds the code that cannot be shared; CMake adds the one that matches the target.
icl_platform_sources(heart_boom_maths_game)

# The fonts are application resources, never fetched from a network. Their licence texts travel with them.
qt_add_resources(heart_boom_maths_game fonts
    PREFIX /heart_boom_maths/fonts
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    FILES
        fonts/Bangers-Regular.ttf
        fonts/LilitaOne-Regular.ttf)

target_include_directories(heart_boom_maths_game PRIVATE src)
target_link_libraries(heart_boom_maths_game
    PRIVATE heart_boom_maths_app heart_boom_maths::core Qt6::Quick icl::warnings icl::hardening icl::coverage)
# qmlcachegen, rcc and moc write C++ files that the strict warning set rejects with some compilers.
icl_relax_generated_warnings(heart_boom_maths_game)

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

    # The build information script, run with a CI build number to see it end up in the header.
    add_test(NAME game.build_info_script
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/build_info_check"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/build_info_check.cmake")
    set_tests_properties(game.build_info_script PROPERTIES LABELS unit TIMEOUT 60)

    # Starts the real application, waits for the first frame and exits.
    add_test(NAME game.smoke COMMAND heart_boom_maths_game --smoke-test)
    set_tests_properties(game.smoke PROPERTIES LABELS ui TIMEOUT 60 ENVIRONMENT "${game_test_environment}")

    # The version is answered before Qt starts, so the test needs no window system at all.
    add_test(NAME game.version COMMAND heart_boom_maths_game --version)
    set_tests_properties(game.version PROPERTIES
        LABELS integration
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=there-is-no-such-plugin"
        PASS_REGULAR_EXPRESSION "^heart-boom-maths ${PROJECT_VERSION} \\(")

    # qmllint over the QML files; any warning fails (qml/.qmllint.ini).
    add_test(NAME game.qmllint
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target heart_boom_maths_game_qmllint)
    set_tests_properties(game.qmllint PROPERTIES LABELS ui TIMEOUT 120)
endif()
