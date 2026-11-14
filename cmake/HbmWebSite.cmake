# SPDX-License-Identifier: GPL-3.0-or-later
#
#   hbm_add_web_site(<target> WEB_DIR <dir> TITLE <title> LICENCE_FILE <file> NOTICES_FILE <file>)
#
# Makes the browser build of <target> a site that can be served as it is: a folder <build>/site with the page, its
# script and style, Qt's loader, the module (.js and .wasm), the licence and the notices, and nothing else. Qt's own
# page (wasm_shell.html) is not used: it starts the game from a script inside the page, shows Qt's logo and has no
# content security policy. The page is apps/game/web/index.html.in, and the folder is checked by
# tools/ci/check_web_site.py.
#
# Only for the Emscripten target; the caller guards it.

function(hbm_add_web_site target)
    cmake_parse_arguments(ARG "" "WEB_DIR;TITLE;LICENCE_FILE;NOTICES_FILE" "" ${ARGN})
    foreach(required WEB_DIR TITLE LICENCE_FILE NOTICES_FILE)
        if(NOT ARG_${required})
            message(FATAL_ERROR "hbm_add_web_site: ${required} is required")
        endif()
    endforeach()

    # Qt writes its own page and loader next to the module unless told not to; it still sets the link options.
    set_target_properties(${target} PROPERTIES
        NO_WASM_DEFAULT_FILES TRUE
        # The name start.js calls; Qt's default is derived from the target name.
        QT_WASM_EXPORT_NAME heart_boom_maths_entry)

    # Embind (which Qt's browser platform uses to talk to the page) builds its call stubs with `new Function` unless told
    # not to, and the page's content security policy forbids that. The stubs are then made without eval.
    target_link_options(${target} PRIVATE "SHELL:-s DYNAMIC_EXECUTION=0")

    get_target_property(module_name ${target} OUTPUT_NAME)
    if(NOT module_name)
        set(module_name "${target}")
    endif()
    set(HBM_WEB_MODULE "${module_name}")
    set(HBM_WEB_TITLE "${ARG_TITLE}")

    set(site_dir "${CMAKE_BINARY_DIR}/site")
    configure_file("${ARG_WEB_DIR}/index.html.in" "${CMAKE_CURRENT_BINARY_DIR}/web/index.html" @ONLY)

    set(qt_loader "${QT6_INSTALL_PREFIX}/plugins/platforms/qtloader.js")
    if(NOT EXISTS "${qt_loader}")
        message(FATAL_ERROR "hbm_add_web_site: Qt's loader was not found at ${qt_loader}")
    endif()

    # One step after each link, so `cmake --build` leaves the finished site in <build>/site.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${site_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE_DIR:${target}>/${module_name}.js"
            "$<TARGET_FILE_DIR:${target}>/${module_name}.wasm"
            "${CMAKE_CURRENT_BINARY_DIR}/web/index.html"
            "${ARG_WEB_DIR}/page.css"
            "${ARG_WEB_DIR}/start.js"
            "${qt_loader}"
            "${site_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ARG_LICENCE_FILE}" "${site_dir}/LICENSE.txt"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ARG_NOTICES_FILE}" "${site_dir}/notices.txt"
        COMMENT "Assembling the web site in ${site_dir}"
        VERBATIM)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${ARG_WEB_DIR}/index.html.in" "${ARG_WEB_DIR}/page.css" "${ARG_WEB_DIR}/start.js")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        "${ARG_WEB_DIR}/page.css" "${ARG_WEB_DIR}/start.js")
endfunction()
