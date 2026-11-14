# SPDX-License-Identifier: GPL-3.0-or-later

# The version is written as constants in a header; code reads them with `if constexpr`
# or as plain values. CMake never defines a macro that changes which code compiles.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/heart_boom_maths/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/heart_boom_maths/version.hpp"
    @ONLY)

# Tuning::defaults() is built from libs/core/data/tuning.json when CMake configures, so the values are data and the
# core needs no JSON reader.
include(HbmTuningData)
hbm_generate_tuning_header(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/tuning.json"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/heart_boom_maths/tuning_data.hpp")

add_library(heart_boom_maths_core
    src/question_generator.cpp
    src/rng.cpp
    src/session.cpp
    src/tuning.cpp)
add_library(heart_boom_maths::core ALIAS heart_boom_maths_core)

target_include_directories(heart_boom_maths_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")
target_compile_features(heart_boom_maths_core PUBLIC cxx_std_23)
target_link_libraries(heart_boom_maths_core
    PRIVATE icl::warnings icl::hardening icl::tracy icl::coverage)

if(HEART_BOOM_MATHS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
