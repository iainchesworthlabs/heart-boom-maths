# SPDX-License-Identifier: GPL-3.0-or-later
#
#   hbm_generate_notices_text(<file> NAME <name> QT_VERSION <version>
#       [EXCLUDE <vcpkg package>...] [LICENCE_FILES <file>...])
#
# Writes the one notices text the About screen reads, at configure time: the third-party notices of
# icl_generate_notices (Qt, and the licence text of every runtime dependency in vcpkg.json), then each file of
# LICENCE_FILES under its own heading (the font licences). icl_generate_notices stops the configure when a dependency
# has no licence file, which is the CI check that the notices cover every dependency. To add another source, add its
# files to LICENCE_FILES.
#
# The file changes only when its text does.

include(IclNotices)

function(hbm_generate_notices_text output)
    cmake_parse_arguments(ARG "" "NAME;QT_VERSION" "EXCLUDE;LICENCE_FILES" ${ARGN})
    if(NOT ARG_NAME)
        message(FATAL_ERROR "hbm_generate_notices_text: NAME is required")
    endif()

    set(third_party "${CMAKE_CURRENT_BINARY_DIR}/notices-third-party.txt")
    icl_generate_notices(
        OUTPUT "${third_party}"
        NAME "${ARG_NAME}"
        QT_VERSION "${ARG_QT_VERSION}"
        EXCLUDE ${ARG_EXCLUDE})
    file(READ "${third_party}" text)

    foreach(licence IN LISTS ARG_LICENCE_FILES)
        if(NOT EXISTS "${licence}")
            message(FATAL_ERROR "hbm_generate_notices_text: no such licence file: ${licence}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${licence}")
        get_filename_component(licence_name "${licence}" NAME_WE)
        file(READ "${licence}" licence_text)
        string(APPEND text "\n================================================================================\n")
        string(APPEND text "${licence_name}\n")
        string(APPEND text "================================================================================\n")
        string(APPEND text "${licence_text}")
        string(REGEX MATCH "\n$" ends_with_newline "${licence_text}")
        if(NOT ends_with_newline)
            string(APPEND text "\n")
        endif()
    endforeach()

    set(previous "")
    if(EXISTS "${output}")
        file(READ "${output}" previous)
    endif()
    if(NOT previous STREQUAL text)
        file(WRITE "${output}" "${text}")
    endif()
endfunction()
