# SPDX-License-Identifier: GPL-3.0-or-later
# Run by ctest (tracy.*): shows that the configure refuses Tracy for a WebAssembly, Android or iOS build, and accepts it
# for a desktop one.
#
#   cmake -DSOURCE_DIR=<repo> -DICL_ENABLE_TRACY=ON -DEMSCRIPTEN=1 -P tracy_platform_check.cmake
#
# The tests expect "Tracy opens a socket" in the output for the first and "accepted" for the second.
include("${SOURCE_DIR}/cmake/HbmProfiling.cmake")
message(STATUS "accepted")
