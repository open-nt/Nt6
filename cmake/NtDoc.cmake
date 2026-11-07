# MIT License - see LICENSE
#
# Documentation support for Nt, using NDoc.
#
#   nt_add_docs(<target> CONFIG <file> [OUTPUT <dir>] [STRICT] [REQUIRE_COMPLETE])

if(NOT TARGET ndoc)
    message(FATAL_ERROR "NtDoc.cmake needs the ndoc target")
endif()

function(nt_add_docs target)
    cmake_parse_arguments(ARG "STRICT;REQUIRE_COMPLETE" "CONFIG;OUTPUT" "" ${ARGN})
    if(NOT ARG_CONFIG)
        message(FATAL_ERROR "nt_add_docs needs CONFIG")
    endif()
    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/doc/html")
    endif()
    set(flags)
    if(ARG_STRICT)
        list(APPEND flags --strict)
    endif()
    if(ARG_REQUIRE_COMPLETE)
        list(APPEND flags --require-complete)
    endif()
    add_custom_target(${target}
        COMMAND ndoc ${flags} -o "${ARG_OUTPUT}" "${ARG_CONFIG}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "ndoc ${ARG_CONFIG}"
        VERBATIM)
endfunction()
