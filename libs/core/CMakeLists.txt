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
    src/json.cpp
    src/question_generator.cpp
    src/replay.cpp
    src/rng.cpp
    src/save_codec.cpp
    src/save_data.cpp
    src/save_manager.cpp
    src/save_store.cpp
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
elseif(HEART_BOOM_MATHS_BUILD_HARNESS)
    # The text harness alone, with no Catch2: the WebAssembly check builds it with Emscripten and replays the golden
    # files under Node (docs/web.md). With the tests on, tests/CMakeLists.txt builds the same program.
    add_executable(heart_boom_maths_harness tests/harness.cpp)
    target_link_libraries(heart_boom_maths_harness PRIVATE heart_boom_maths::core icl::warnings icl::coverage)
    if(EMSCRIPTEN)
        # Node's own file system, so `--check <directory>` reads the replay files where they are.
        target_link_options(heart_boom_maths_harness PRIVATE "SHELL:-s NODERAWFS=1" "SHELL:-s ENVIRONMENT=node")
    endif()
endif()
