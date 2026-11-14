# SPDX-License-Identifier: GPL-3.0-or-later
#
# The generated headers carry their own licence line. It is written in two pieces below so that REUSE does not read it as
# this file's.
#
# Turns the two data files of the game into C++ headers while CMake configures:
#
#   hbm_generate_tuning_header(<tuning.json> <out.hpp>)
#       The release values of the game, as Tuning::defaults() returns them.
#   hbm_generate_prd_tables_header(<prd-tables.json> <out.hpp>)
#       The numbers the PRD states, for the test that keeps the two files equal.
#
# The values are plain integers in the headers, so the core needs no JSON reader and no extra dependency, and a change
# to a data file changes the build. An unknown name or a value that is not an integer stops the configure.

function(hbm_json_int out json)
    string(JSON type ERROR_VARIABLE error TYPE "${json}" ${ARGN})
    if(error OR NOT type STREQUAL "NUMBER")
        message(FATAL_ERROR "Tuning data: '${ARGN}' is missing or is not a number (${error})")
    endif()
    string(JSON value GET "${json}" ${ARGN})
    if(NOT value MATCHES "^-?[0-9]+$")
        message(FATAL_ERROR "Tuning data: '${ARGN}' is ${value}, not an integer")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(hbm_json_bool out json)
    string(JSON type ERROR_VARIABLE error TYPE "${json}" ${ARGN})
    if(error OR NOT type STREQUAL "BOOLEAN")
        message(FATAL_ERROR "Tuning data: '${ARGN}' is missing or is not true or false (${error})")
    endif()
    string(JSON value GET "${json}" ${ARGN})
    if(value)
        set(${out} "true" PARENT_SCOPE)
    else()
        set(${out} "false" PARENT_SCOPE)
    endif()
endfunction()

function(hbm_json_string out json)
    string(JSON type ERROR_VARIABLE error TYPE "${json}" ${ARGN})
    if(error OR NOT type STREQUAL "STRING")
        message(FATAL_ERROR "Tuning data: '${ARGN}' is missing or is not a string (${error})")
    endif()
    string(JSON value GET "${json}" ${ARGN})
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(hbm_json_length out json)
    string(JSON type ERROR_VARIABLE error TYPE "${json}" ${ARGN})
    if(error OR NOT type STREQUAL "ARRAY")
        message(FATAL_ERROR "Tuning data: '${ARGN}' is missing or is not a list (${error})")
    endif()
    string(JSON value LENGTH "${json}" ${ARGN})
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# The enumerator names of tuning.hpp, from the names the data files use.
function(hbm_shape_enumerator out name)
    if(name STREQUAL "two_term")
        set(result TWO_TERM)
    elseif(name STREQUAL "missing_addend")
        set(result MISSING_ADDEND)
    elseif(name STREQUAL "three_term")
        set(result THREE_TERM)
    elseif(name STREQUAL "four_term")
        set(result FOUR_TERM)
    else()
        message(FATAL_ERROR "Tuning data: unknown question shape '${name}'")
    endif()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

function(hbm_operations_enumerator out name)
    if(name STREQUAL "add")
        set(result ADD)
    elseif(name STREQUAL "take_away")
        set(result TAKE_AWAY)
    elseif(name STREQUAL "mixed")
        set(result MIXED)
    else()
        message(FATAL_ERROR "Tuning data: unknown operations '${name}'")
    endif()
    set(${out} "${result}" PARENT_SCOPE)
endfunction()

function(hbm_generate_tuning_header json_file out_file)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${json_file}")
    file(READ "${json_file}" json)

    # The members appear in the order of struct Tuning, which C++ requires of designated initialisers.
    set(body "")
    foreach(key IN ITEMS
            ticks_per_second questions_per_level answer_tiles campaign_levels level_seconds alert_seconds
            right_answer_seconds wrong_answer_seconds)
        hbm_json_int(value "${json}" ${key})
        string(APPEND body "    .${key} = ${value},\n")
    endforeach()

    hbm_json_bool(capped "${json}" right_answer_time_capped_at_level_time)
    string(APPEND body "    .right_answer_time_capped_at_level_time = ${capped},\n")

    hbm_json_int(value "${json}" base_points)
    string(APPEND body "    .base_points = ${value},\n")

    hbm_json_length(step_count "${json}" multiplier_steps)
    set(steps "")
    math(EXPR last "${step_count} - 1")
    foreach(i RANGE ${last})
        hbm_json_int(run "${json}" multiplier_steps ${i} run)
        hbm_json_int(step "${json}" multiplier_steps ${i} step)
        string(APPEND steps "        {${run}, ${step}},\n")
    endforeach()
    string(APPEND body "    .multiplier_steps = {{\n${steps}    }},\n")

    foreach(key IN ITEMS
            streak_bonus_coin_every time_bonus_points_per_second perfect_level_bonus_points perfect_level_coins
            win_bonus_coins hint_cost_coins hint_tiles_removed top10_size name_max_length operand_minimum)
        hbm_json_int(value "${json}" ${key})
        string(APPEND body "    .${key} = ${value},\n")
    endforeach()

    hbm_json_length(level_count "${json}" levels)
    set(levels "")
    math(EXPR last "${level_count} - 1")
    foreach(i RANGE ${last})
        hbm_json_int(level "${json}" levels ${i} level)
        hbm_json_string(shape_name "${json}" levels ${i} shape)
        hbm_json_string(operations_name "${json}" levels ${i} operations)
        hbm_json_int(largest "${json}" levels ${i} largest_number)
        hbm_shape_enumerator(shape "${shape_name}")
        hbm_operations_enumerator(operations "${operations_name}")
        string(APPEND levels
            "        {${level}, QuestionShape::${shape}, Operations::${operations}, ${largest}},\n")
    endforeach()
    string(APPEND body "    .levels = {{\n${levels}    }},\n")

    set(endless "")
    foreach(key IN ITEMS
            first_level largest_number level_seconds_start level_seconds_per_k level_seconds_floor
            bonus_seconds_start bonus_seconds_drop_every_k bonus_seconds_floor
            multi_term_percent_start multi_term_percent_end multi_term_percent_span_k
            wrong_answer_seconds alert_seconds)
        hbm_json_int(value "${json}" endless ${key})
        string(APPEND endless "        .${key} = ${value},\n")
    endforeach()
    string(APPEND body "    .endless = {\n${endless}    },\n")

    # string(CONCAT) and not a list: the text is full of semicolons, which a list would split.
    string(CONCAT content_text
        "// Generated by cmake/HbmTuningData.cmake from libs/core/data/tuning.json. Do not edit.\n"
        "// SPDX-License-" "Identifier: GPL-3.0-or-later\n"
        "#pragma once\n\n"
        "#include \"heart_boom_maths/tuning.hpp\"\n\n"
        "namespace heart_boom_maths::generated\n{\n\n"
        "inline constexpr Tuning TUNING_DEFAULTS{\n${body}};\n\n"
        "static_assert(TUNING_DEFAULTS.campaign_levels == static_cast<int>(TUNING_DEFAULTS.levels.size()),\n"
        "              \"campaign_levels must match the number of rows in levels\");\n\n"
        "}  // namespace heart_boom_maths::generated\n")
    file(CONFIGURE OUTPUT "${out_file}" CONTENT "${content_text}" @ONLY NEWLINE_STYLE UNIX)
endfunction()

# Emits `inline constexpr int <prefix>_<KEY> = <value>;` for every number in an object of the PRD tables.
function(hbm_emit_prd_numbers out json prefix)
    string(JSON count LENGTH "${json}" ${ARGN})
    set(text "")
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(i RANGE ${last})
            string(JSON name MEMBER "${json}" ${ARGN} ${i})
            string(JSON type TYPE "${json}" ${ARGN} ${name})
            if(type STREQUAL "NUMBER")
                hbm_json_int(value "${json}" ${ARGN} ${name})
                string(TOUPPER "${name}" upper)
                string(APPEND text "inline constexpr int ${prefix}_${upper} = ${value};\n")
            endif()
        endforeach()
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(hbm_generate_prd_tables_header json_file out_file)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${json_file}")
    file(READ "${json_file}" json)

    hbm_json_length(level_count "${json}" levels)
    set(levels "")
    math(EXPR last "${level_count} - 1")
    foreach(i RANGE ${last})
        hbm_json_int(level "${json}" levels ${i} level)
        hbm_json_string(prd_shape "${json}" levels ${i} prd_shape)
        hbm_json_string(prd_operations "${json}" levels ${i} prd_operations)
        hbm_json_int(largest "${json}" levels ${i} largest_number)

        # The PRD writes shapes as sentences such as "a + b - c". The shape follows from how many of the letters a to d
        # appear, and from a question mark for the missing addend. Only ASCII is compared, because the PRD also uses
        # the minus and plus-or-minus signs.
        string(REGEX MATCHALL "[a-d]" letters "${prd_shape}")
        list(LENGTH letters letter_count)
        if(prd_shape MATCHES "[?]")
            set(shape MISSING_ADDEND)
        elseif(letter_count EQUAL 2)
            set(shape TWO_TERM)
        elseif(letter_count EQUAL 3)
            set(shape THREE_TERM)
        elseif(letter_count EQUAL 4)
            set(shape FOUR_TERM)
        else()
            message(FATAL_ERROR "PRD tables: cannot read the shape of level ${level}")
        endif()

        if(prd_operations STREQUAL "Add")
            set(operations ADD)
        elseif(prd_operations STREQUAL "Take away")
            set(operations TAKE_AWAY)
        elseif(prd_operations STREQUAL "Mixed")
            set(operations MIXED)
        else()
            message(FATAL_ERROR "PRD tables: unknown operations '${prd_operations}' at level ${level}")
        endif()
        string(APPEND levels
            "    PrdLevel{${level}, QuestionShape::${shape}, Operations::${operations}, ${largest}},\n")
    endforeach()

    hbm_json_length(row_count "${json}" endless_ramp_rows)
    set(rows "")
    math(EXPR last "${row_count} - 1")
    foreach(i RANGE ${last})
        set(fields "")
        foreach(key IN ITEMS level level_seconds bonus_seconds multi_term_percent alert_share_percent)
            hbm_json_int(value "${json}" endless_ramp_rows ${i} ${key})
            list(APPEND fields "${value}")
        endforeach()
        string(JOIN ", " field_text ${fields})
        string(APPEND rows "    PrdEndlessRow{${field_text}},\n")
    endforeach()

    hbm_emit_prd_numbers(defaults "${json}" PRD_DEFAULTS tuning_defaults_table)
    hbm_emit_prd_numbers(scoring "${json}" PRD_SCORING scoring)
    hbm_emit_prd_numbers(streak "${json}" PRD_STREAK streak)

    string(CONCAT content_text
        "// Generated by cmake/HbmTuningData.cmake from libs/core/tests/data/prd-tables.json. Do not edit.\n"
        "// SPDX-License-" "Identifier: GPL-3.0-or-later\n"
        "#pragma once\n\n"
        "#include <array>\n\n"
        "#include \"heart_boom_maths/tuning.hpp\"\n\n"
        "namespace heart_boom_maths::prd\n{\n\n"
        "struct PrdLevel\n{\n    int level;\n    QuestionShape shape;\n    Operations operations;\n    int largest_number;\n};\n\n"
        "struct PrdEndlessRow\n{\n    int level;\n    int level_seconds;\n    int bonus_seconds;\n    int multi_term_percent;\n    int alert_share_percent;\n};\n\n"
        "inline constexpr std::array<PrdLevel, ${level_count}> PRD_LEVELS{{\n${levels}}};\n\n"
        "inline constexpr std::array<PrdEndlessRow, ${row_count}> PRD_ENDLESS_ROWS{{\n${rows}}};\n\n"
        "${defaults}\n${scoring}\n${streak}\n"
        "}  // namespace heart_boom_maths::prd\n")
    file(CONFIGURE OUTPUT "${out_file}" CONTENT "${content_text}" @ONLY NEWLINE_STYLE UNIX)
endfunction()
