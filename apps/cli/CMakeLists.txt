# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(heart_boom_maths_cli src/main.cpp)
set_target_properties(heart_boom_maths_cli PROPERTIES OUTPUT_NAME heart-boom-maths)
target_link_libraries(heart_boom_maths_cli
    PRIVATE heart_boom_maths::core icl::warnings icl::hardening icl::coverage)

install(TARGETS heart_boom_maths_cli RUNTIME DESTINATION bin)

if(HEART_BOOM_MATHS_BUILD_TESTS)
    # Integration tests run the real executable.
    add_test(NAME cli.version COMMAND heart_boom_maths_cli --version)
    set_tests_properties(cli.version PROPERTIES
        LABELS integration
        PASS_REGULAR_EXPRESSION "^heart-boom-maths ${PROJECT_VERSION}")
    add_test(NAME cli.greets COMMAND heart_boom_maths_cli Ada)
    set_tests_properties(cli.greets PROPERTIES
        LABELS integration
        PASS_REGULAR_EXPRESSION "Hello, Ada, from ")
    add_test(NAME cli.rejects_extra_arguments COMMAND heart_boom_maths_cli one two)
    set_tests_properties(cli.rejects_extra_arguments PROPERTIES LABELS integration WILL_FAIL TRUE)
endif()
