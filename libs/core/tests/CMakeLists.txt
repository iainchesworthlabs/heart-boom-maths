# SPDX-License-Identifier: GPL-3.0-or-later
# The PRD's own numbers become a header for the tuning test; see cmake/HbmTuningData.cmake.
hbm_generate_prd_tables_header(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/prd-tables.json"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/prd_tables.hpp")

# The version 1 save fixture becomes a header, so the test reads no file at run time and needs no path from the build.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/data/save-v1.json" HBM_SAVE_V1_FIXTURE)
# The licence line is written in two pieces so that REUSE does not read it as this file's.
string(CONCAT hbm_save_fixtures_text
    "// SPDX-License-" "Identifier: GPL-3.0-or-later\n"
    "#pragma once\n"
    "#include <string_view>\n"
    "inline constexpr std::string_view SAVE_V1_FIXTURE = R\"fixture(@HBM_SAVE_V1_FIXTURE@)fixture\";\n")
file(CONFIGURE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/save_fixtures.hpp"
    CONTENT "${hbm_save_fixtures_text}"
    @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/save-v1.json")

# Every file in data/replays becomes a byte array in a header, so the replay test reads no file at run time. The files are
# read as hex because a long string literal is more than some compilers accept. A new or changed file re-runs CMake.
file(GLOB hbm_replay_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/replays/*.json")
list(SORT hbm_replay_files)
if(NOT hbm_replay_files)
    message(FATAL_ERROR "No golden replays in ${CMAKE_CURRENT_SOURCE_DIR}/data/replays")
endif()
set(hbm_replay_arrays "")
set(hbm_replay_table "")
set(hbm_replay_index 0)
foreach(replay_file IN LISTS hbm_replay_files)
    get_filename_component(replay_name "${replay_file}" NAME_WE)
    file(READ "${replay_file}" replay_hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," replay_bytes "${replay_hex}")
    string(APPEND hbm_replay_arrays
        "inline constexpr char GOLDEN_REPLAY_${hbm_replay_index}[] = {${replay_bytes}};\n")
    string(APPEND hbm_replay_table
        "    {\"${replay_name}\", std::string_view(GOLDEN_REPLAY_${hbm_replay_index}, sizeof GOLDEN_REPLAY_${hbm_replay_index})},\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${replay_file}")
    math(EXPR hbm_replay_index "${hbm_replay_index} + 1")
endforeach()
# The licence line is written in two pieces so that REUSE does not read it as this file's.
string(CONCAT hbm_replay_header_text
    "// SPDX-License-" "Identifier: GPL-3.0-or-later\n"
    "#pragma once\n"
    "#include <string_view>\n"
    "struct GoldenReplay\n{\n    std::string_view name;\n    std::string_view text;\n};\n"
    "${hbm_replay_arrays}"
    "inline constexpr GoldenReplay GOLDEN_REPLAYS[] = {\n${hbm_replay_table}};\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/golden_replays.hpp" "${hbm_replay_header_text}")

add_executable(heart_boom_maths_core_tests
    test_contract.cpp
    test_core_property.cpp
    test_endless.cpp
    test_json.cpp
    test_profile.cpp
    test_question_generator.cpp
    test_replay.cpp
    test_rng.cpp
    test_save.cpp
    test_save_fuzz.cpp
    test_save_manager.cpp
    test_session.cpp
    test_session_property.cpp
    test_tuning.cpp
    test_version.cpp)
target_include_directories(heart_boom_maths_core_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
# What the test of the profiling marks expects of this build; the build's own choice is made by ICL_ENABLE_TRACY.
target_compile_definitions(heart_boom_maths_core_tests PRIVATE HBM_TEST_EXPECT_TRACY=$<BOOL:${ICL_ENABLE_TRACY}>)
target_link_libraries(heart_boom_maths_core_tests
    PRIVATE heart_boom_maths::core Catch2::Catch2WithMain icl::warnings icl::coverage)

# Every Catch2 case becomes a CTest test carrying the `unit` label (the labels are
# unit, integration, system, ui, perf and fuzz).
catch_discover_tests(heart_boom_maths_core_tests PROPERTIES LABELS unit)

# The text harness plays a whole game in a terminal (see docs/testing.md). These tests run it as a program, so they show
# that the rules can be played from the first screen to the win, and into endless play, with nothing but the core.
add_executable(heart_boom_maths_harness harness.cpp)
target_link_libraries(heart_boom_maths_harness PRIVATE heart_boom_maths::core icl::warnings icl::coverage)

add_test(NAME harness.full-game COMMAND heart_boom_maths_harness --auto perfect --seed 12 --quiet)
add_test(NAME harness.endless COMMAND heart_boom_maths_harness --auto perfect --seed 13 --endless-to 20 --quiet)
add_test(NAME harness.replays-current
    COMMAND heart_boom_maths_harness --check "${CMAKE_CURRENT_SOURCE_DIR}/data/replays")
set_tests_properties(harness.full-game PROPERTIES PASS_REGULAR_EXPRESSION "FINAL score=[0-9]+ level=12 ")
set_tests_properties(harness.endless PROPERTIES PASS_REGULAR_EXPRESSION "FINAL score=[0-9]+ level=20 ")
set_tests_properties(harness.full-game harness.endless harness.replays-current PROPERTIES LABELS integration)

# The performance tests of the rules (label perf), only when a perf preset asks for them. Not linked with coverage, and the
# program replaces the global operator new to count allocations (perf/perf_alloc.cpp), so it never joins the ordinary tests.
if(HEART_BOOM_MATHS_BUILD_PERF_TESTS)
    add_executable(heart_boom_maths_core_perf perf/perf_core.cpp perf/perf_alloc.cpp)
    target_include_directories(heart_boom_maths_core_perf PRIVATE perf "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(heart_boom_maths_core_perf
        PRIVATE HBM_PERF_BUDGETS_FILE="${PROJECT_SOURCE_DIR}/tools/perf-budgets.json"
                _CRT_SECURE_NO_WARNINGS) # MSVC calls getenv unsafe; the harness reads two switches with it
    target_link_libraries(heart_boom_maths_core_perf
        PRIVATE heart_boom_maths::core Catch2::Catch2WithMain icl::warnings)
    catch_discover_tests(heart_boom_maths_core_perf
        TEST_PREFIX "perf."
        TEST_SPEC "[perf]"
        PROPERTIES LABELS perf TIMEOUT 900
            ENVIRONMENT "HBM_PERF_RESULTS_DIR=${PROJECT_BINARY_DIR}/perf-results")
endif()
