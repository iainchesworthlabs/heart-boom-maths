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

add_executable(heart_boom_maths_core_tests
    test_contract.cpp
    test_endless.cpp
    test_json.cpp
    test_question_generator.cpp
    test_rng.cpp
    test_save.cpp
    test_save_fuzz.cpp
    test_save_manager.cpp
    test_session.cpp
    test_session_property.cpp
    test_tuning.cpp
    test_version.cpp)
target_include_directories(heart_boom_maths_core_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(heart_boom_maths_core_tests
    PRIVATE heart_boom_maths::core Catch2::Catch2WithMain icl::warnings icl::coverage)

# Every Catch2 case becomes a CTest test carrying the `unit` label (the labels are
# unit, integration, system, ui, perf and fuzz).
catch_discover_tests(heart_boom_maths_core_tests PROPERTIES LABELS unit)
