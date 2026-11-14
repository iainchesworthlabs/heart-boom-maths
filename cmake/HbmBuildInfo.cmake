# SPDX-License-Identifier: GPL-3.0-or-later
#
#   hbm_generate_build_info(<header> TEMPLATE <file.in> APP_NAME <name> QT_VERSION <version>)
#
# Writes <header> at configure time from <file.in>: the version (from project()), the build number, the short commit,
# the branch or tag, the build date and the Qt version. The build number is the CI run number, read from
# ICL_BUILD_NUMBER and then GITHUB_RUN_NUMBER, and 0 when neither is a whole number. The commit comes from git, then
# from GITHUB_SHA, and is "dev" when neither exists. The branch (or tag) and the date are taken only on CI (a build
# number is set, or GITHUB_ACTIONS is true) and read "dev" on a local build, as About shows them.
#
# The header changes only when a value does, so a reconfigure that finds the same values rebuilds nothing. A commit
# made after the configure shows at the next configure.

function(hbm_generate_build_info output)
    cmake_parse_arguments(ARG "" "TEMPLATE;APP_NAME;QT_VERSION" "" ${ARGN})
    if(NOT ARG_TEMPLATE OR NOT ARG_APP_NAME OR NOT ARG_QT_VERSION)
        message(FATAL_ERROR "hbm_generate_build_info: TEMPLATE, APP_NAME and QT_VERSION are required")
    endif()

    set(build_number 0)
    set(on_ci OFF)
    foreach(variable IN ITEMS ICL_BUILD_NUMBER GITHUB_RUN_NUMBER)
        if(DEFINED ENV{${variable}} AND "$ENV{${variable}}" MATCHES "^[0-9]+$")
            set(build_number "$ENV{${variable}}")
            set(on_ci ON)
            break()
        endif()
    endforeach()
    if("$ENV{GITHUB_ACTIONS}" STREQUAL "true")
        set(on_ci ON)
    endif()

    set(commit "")
    set(branch "")
    find_program(git_program git)
    if(git_program)
        execute_process(COMMAND "${git_program}" -C "${PROJECT_SOURCE_DIR}" rev-parse --short=7 HEAD
            OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE status)
        if(NOT status EQUAL 0)
            set(commit "")
        endif()
        execute_process(COMMAND "${git_program}" -C "${PROJECT_SOURCE_DIR}" rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE status)
        if(NOT status EQUAL 0)
            set(branch "")
        endif()
    endif()
    if(NOT commit AND DEFINED ENV{GITHUB_SHA})
        string(SUBSTRING "$ENV{GITHUB_SHA}" 0 7 commit)
    endif()
    if(NOT commit)
        set(commit "dev")
    endif()

    if(on_ci)
        if(DEFINED ENV{GITHUB_REF_NAME} AND NOT "$ENV{GITHUB_REF_NAME}" STREQUAL "")
            set(branch "$ENV{GITHUB_REF_NAME}")
        endif()
        if(NOT branch)
            set(branch "dev")
        endif()
        string(TIMESTAMP build_date "%Y-%m-%d" UTC)
    else()
        set(branch "dev")
        set(build_date "dev")
    endif()

    # These end up inside C++ string literals: keep to characters a commit, a branch, a tag and a date can hold.
    foreach(variable IN ITEMS commit branch build_date)
        string(REGEX REPLACE "[^A-Za-z0-9._/+-]" "_" ${variable} "${${variable}}")
    endforeach()

    set(HBM_BUILD_APP_NAME "${ARG_APP_NAME}")
    set(HBM_BUILD_VERSION "${PROJECT_VERSION}")
    set(HBM_BUILD_NUMBER "${build_number}")
    set(HBM_BUILD_COMMIT "${commit}")
    set(HBM_BUILD_BRANCH "${branch}")
    set(HBM_BUILD_DATE "${build_date}")
    set(HBM_BUILD_QT_VERSION "${ARG_QT_VERSION}")
    configure_file("${ARG_TEMPLATE}" "${output}" @ONLY)
endfunction()
