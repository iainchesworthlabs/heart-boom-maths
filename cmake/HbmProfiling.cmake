# SPDX-License-Identifier: GPL-3.0-or-later
# ---------------------------------------------------------------------------
# HbmProfiling.cmake
#
#   hbm_add_profiling(<target>)
#
# adds the application's profiling support to <target> (a library): the run-time switch (always, it is plain code and
# is tested everywhere) and one of two implementations of the zone macros, chosen here and not by the preprocessor:
#
#   src/profiling/tracy/  the marks over Tracy, built when ICL_ENABLE_TRACY is on (the vcpkg feature `profiling`)
#   src/profiling/null/   the marks as nothing: no Tracy code, no listener, no cost, and Tracy need not be installed
#
# Both directories hold a profiling.hpp with the same names and are put on the include path one at a time, so no source
# asks which one it got. The Tracy directory also holds what only a profiling build needs: the scene graph's zones, the
# GPU context (Direct3D 11 on Windows, none elsewhere) and the scripted run behind --profile-run.
#
# HEART_BOOM_MATHS_PROFILE_AUTOSTART says whether a build that is told nothing starts the profiler when the game starts.
# The profile presets turn it on, because they exist to take a trace; every other build leaves it off, and then only
# --profile or HBM_PROFILE=1 starts it (docs/profiling.md). WebAssembly and iOS cannot host Tracy's
# listener: configure them with ICL_ENABLE_TRACY off, as the presets do.
# ---------------------------------------------------------------------------

include_guard(GLOBAL)

option(HEART_BOOM_MATHS_PROFILE_AUTOSTART "Start the profiler when the game starts, with no flag (the profile presets)" OFF)

# Tracy opens a listening socket. The web, Android and iOS builds ship as pages and phone apps with no listener, so asking
# for Tracy on them is an error at configure time, not a build that has one. (A desktop build may carry it, dormant.)
function(hbm_assert_tracy_platform)
    if(ICL_ENABLE_TRACY AND (EMSCRIPTEN OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
        message(FATAL_ERROR "ICL_ENABLE_TRACY is on for a WebAssembly, Android or iOS build: Tracy opens a socket and "
            "those builds have none. Configure them with ICL_ENABLE_TRACY=OFF (the presets do).")
    endif()
endfunction()
hbm_assert_tracy_platform()

function(hbm_add_profiling target)
    set(profiling_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/profiling")
    if(ICL_ENABLE_TRACY)
        if(ICL_TRACY_IN_PROCESS)
            message(FATAL_ERROR "ICL_TRACY_IN_PROCESS is not supported: this project has no in-process recorder")
        endif()
        set(impl tracy)
    else()
        set(impl null)
    endif()

    if(impl STREQUAL "tracy")
        set(HBM_PROFILE_TRACY_VALUE "true")
    else()
        set(HBM_PROFILE_TRACY_VALUE "false")
    endif()
    if(HEART_BOOM_MATHS_PROFILE_AUTOSTART AND impl STREQUAL "tracy")
        set(HBM_PROFILE_AUTOSTART_VALUE "true")
    else()
        set(HBM_PROFILE_AUTOSTART_VALUE "false")
    endif()
    configure_file("${profiling_dir}/profile_build.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/profile_build.hpp" @ONLY)

    target_sources(${target} PRIVATE "${profiling_dir}/profile_switch.cpp")
    target_include_directories(${target} PUBLIC "${profiling_dir}/${impl}")

    if(impl STREQUAL "tracy")
        target_sources(${target} PRIVATE
            "${profiling_dir}/tracy/profile_run.cpp"
            "${profiling_dir}/tracy/profiling.cpp"
            "${profiling_dir}/tracy/render_profiler.cpp"
            "${profiling_dir}/tracy/scene_graph_log.cpp")
        # The graphics API has a context of its own on Windows only.
        if(WIN32)
            target_sources(${target} PRIVATE "${profiling_dir}/tracy/gpu_d3d11.cpp")
            target_link_libraries(${target} PRIVATE d3d11 dxgi)
            # <windows.h> must leave the standard library's min and max alone.
            set_source_files_properties("${profiling_dir}/tracy/gpu_d3d11.cpp" PROPERTIES COMPILE_DEFINITIONS NOMINMAX)
        else()
            target_sources(${target} PRIVATE "${profiling_dir}/tracy/gpu_none.cpp")
        endif()
        # icl::tracy brings Tracy's client and the definitions every file that includes profiling.hpp needs. The RHI's
        # timestamps (the GPU frame time plot) are read through Qt's semi-public RHI header, so only a profiling build
        # depends on Qt's private Gui interface.
        find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
        target_link_libraries(${target} PUBLIC icl::tracy PRIVATE Qt6::GuiPrivate)
    endif()
endfunction()
