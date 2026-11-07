# MIT License - see LICENSE
#
# Runs NtBadge.cmake and checks the generated badge against the VERSION file.

file(REMOVE_RECURSE "${OUTPUT_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -DVERSION_FILE=${VERSION_FILE} -DOUTPUT_DIR=${OUTPUT_DIR}
                -P "${BADGE_SCRIPT}" RESULT_VARIABLE result ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "NtBadge.cmake failed: ${err}")
endif()

file(STRINGS "${VERSION_FILE}" version LIMIT_COUNT 1)
if(NOT version MATCHES "^6\\.[0-9]+\\.[0-9]+$")
    message(SEND_ERROR "VERSION '${version}' must be 6.x.y")
endif()

file(READ "${OUTPUT_DIR}/version.svg" svg)
file(READ "${OUTPUT_DIR}/version.json" json)
foreach(text "<title>version: ${version}</title>" ">${version}</text>")
    string(FIND "${svg}" "${text}" at)
    if(at EQUAL -1)
        message(SEND_ERROR "version.svg: missing: ${text}")
    endif()
endforeach()
string(FIND "${json}" "\"message\":\"${version}\"" at)
if(at EQUAL -1)
    message(SEND_ERROR "version.json: missing the version: ${json}")
endif()

# A bad VERSION file must be rejected.
file(WRITE "${OUTPUT_DIR}/bad" "6.1.0 <script>\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -DVERSION_FILE=${OUTPUT_DIR}/bad -DOUTPUT_DIR=${OUTPUT_DIR}/bad.d
                -P "${BADGE_SCRIPT}" RESULT_VARIABLE bad OUTPUT_QUIET ERROR_QUIET)
if(bad EQUAL 0)
    message(SEND_ERROR "NtBadge.cmake accepted an invalid version")
endif()
