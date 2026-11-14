# SPDX-License-Identifier: GPL-3.0-or-later
# The PRD's own numbers become a header for the tuning test; see cmake/HbmTuningData.cmake.
hbm_generate_prd_tables_header(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/prd-tables.json"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/prd_tables.hpp")

add_executable(heart_boom_maths_core_tests
    test_contract.cpp
    test_endless.cpp
    test_greeting.cpp
    test_parse.cpp
    test_platform.cpp
    test_session.cpp
    test_session_property.cpp
    test_tuning.cpp)
target_include_directories(heart_boom_maths_core_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(heart_boom_maths_core_tests
    PRIVATE heart_boom_maths::core Catch2::Catch2WithMain icl::warnings icl::coverage)

# Every Catch2 case becomes a CTest test carrying the `unit` label (the labels are
# unit, integration, system, ui, perf and fuzz).
catch_discover_tests(heart_boom_maths_core_tests PROPERTIES LABELS unit)
