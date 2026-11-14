# SPDX-License-Identifier: GPL-3.0-or-later
# Runs cmake/HbmBuildInfo.cmake the way a CI build does and reads the header it writes.
#   cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<scratch> -P build_info_check.cmake

list(APPEND CMAKE_MODULE_PATH "${SOURCE_DIR}/cmake")
include(HbmBuildInfo)

set(PROJECT_VERSION "1.2.3")
set(PROJECT_SOURCE_DIR "${SOURCE_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(generate output)
    hbm_generate_build_info("${output}"
        TEMPLATE "${SOURCE_DIR}/apps/game/src/services/build_info_data.hpp.in"
        APP_NAME "Heart Boom Maths"
        QT_VERSION "6.9.9")
endfunction()

function(expect file pattern)
    file(READ "${file}" text)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${file} does not match ${pattern}:\n${text}")
    endif()
endfunction()

# A local build: build 0, and "dev" for the branch and the date.
unset(ENV{ICL_BUILD_NUMBER})
unset(ENV{GITHUB_RUN_NUMBER})
unset(ENV{GITHUB_ACTIONS})
generate("${WORK_DIR}/local.hpp")
expect("${WORK_DIR}/local.hpp" "BUILD_VERSION = \"1\.2\.3\"")
expect("${WORK_DIR}/local.hpp" "BUILD_NUMBER = 0;")
expect("${WORK_DIR}/local.hpp" "BUILD_BRANCH = \"dev\"")
expect("${WORK_DIR}/local.hpp" "BUILD_DATE = \"dev\"")
expect("${WORK_DIR}/local.hpp" "BUILD_QT_VERSION = \"6\.9\.9\"")

# CI: the run number, a branch name and a date.
set(ENV{ICL_BUILD_NUMBER} "42")
set(ENV{GITHUB_REF_NAME} "v1.2.3")
generate("${WORK_DIR}/ci.hpp")
expect("${WORK_DIR}/ci.hpp" "BUILD_NUMBER = 42;")
expect("${WORK_DIR}/ci.hpp" "BUILD_BRANCH = \"v1\.2\.3\"")
expect("${WORK_DIR}/ci.hpp" "BUILD_DATE = \"[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\"")

# Not a whole number: ignored. And the commit is only characters a hash can hold, or "dev".
set(ENV{ICL_BUILD_NUMBER} "forty-two")
unset(ENV{GITHUB_RUN_NUMBER})
generate("${WORK_DIR}/bad.hpp")
expect("${WORK_DIR}/bad.hpp" "BUILD_NUMBER = 0;")
expect("${WORK_DIR}/bad.hpp" "BUILD_COMMIT = \"[0-9a-f]+\"|BUILD_COMMIT = \"dev\"")
