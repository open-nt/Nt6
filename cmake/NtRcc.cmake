# MIT License - see LICENSE
#
# Resource support for Nt: replaces Qt's rcc with ntrcc.
#
#   nt_add_resources(<outvar> <file.qrc>... [OPTIONS <ntrcc option>...])
#       Compiles each .qrc file to qrc_<name>.cpp and appends the sources to <outvar>, like
#       qt_add_resources(<outvar> ...). Add them to a target's sources.
#
#   nt_add_resources(<target> <name> [PREFIX <prefix>] [BASE <dir>] [LANG <lang>]
#                    FILES <file>... [OPTIONS <ntrcc option>...])
#       Compiles the files into <target> under :<prefix>/, like the Qt 6 form of
#       qt_add_resources(). A file is named by its path relative to BASE (the current source
#       directory by default) or by its QT_RESOURCE_ALIAS source file property.
#
#   nt_add_binary_resources(<target> <file.qrc>... [DESTINATION <file.rcc>] [OPTIONS ...])
#       Adds a custom target that writes a binary resource file for
#       QResource::registerResource(), like qt_add_binary_resources().
#
#   nt_enable_autorcc(<target>...)
#       CMake AUTORCC driven by ntrcc: the .qrc files among the target's sources are compiled.
#
# qt_add_resources(), qt6_add_resources(), qt_add_binary_resources() and
# qt6_add_binary_resources() are defined as the same functions unless Qt's own are present.

if(NOT TARGET ntrcc)
    message(FATAL_ERROR "NtRcc.cmake needs the ntrcc target")
endif()

# Lists the files that the .qrc file <qrc> refers to, so that the generated source is rebuilt when
# one of them changes. Directories are expanded. The .qrc file itself is a configure dependency, so
# that adding a file to it reconfigures the project.
function(_nt_qrc_dependencies outvar qrc)
    get_filename_component(base "${qrc}" DIRECTORY)
    set(deps "${qrc}")
    if(EXISTS "${qrc}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${qrc}")
        file(READ "${qrc}" content)
        string(REGEX MATCHALL "<file[^>]*>[^<]*</file>" entries "${content}")
        foreach(entry IN LISTS entries)
            string(REGEX REPLACE "^<file[^>]*>([^<]*)</file>$" "\\1" path "${entry}")
            string(STRIP "${path}" path)
            string(REPLACE "&amp;" "&" path "${path}")
            if(NOT IS_ABSOLUTE "${path}")
                set(path "${base}/${path}")
            endif()
            if(IS_DIRECTORY "${path}")
                file(GLOB_RECURSE inside CONFIGURE_DEPENDS LIST_DIRECTORIES false "${path}/*")
                list(APPEND deps ${inside})
            else()
                list(APPEND deps "${path}")
            endif()
        endforeach()
    endif()
    set(${outvar} "${deps}" PARENT_SCOPE)
endfunction()

# Adds the command that compiles <qrc> to <out> with the initialization functions of <name>.
function(_nt_rcc_command qrc out name options)
    _nt_qrc_dependencies(deps "${qrc}")
    add_custom_command(
        OUTPUT "${out}"
        COMMAND ntrcc --name "${name}" ${options} -o "${out}" "${qrc}"
        DEPENDS ${deps} ntrcc
        COMMENT "ntrcc ${name}"
        VERBATIM)
    set_source_files_properties("${out}" PROPERTIES SKIP_AUTOMOC ON SKIP_AUTOUIC ON SKIP_AUTORCC ON GENERATED ON)
endfunction()

function(_nt_xml_escape outvar text)
    string(REPLACE "&" "&amp;" text "${text}")
    string(REPLACE "<" "&lt;" text "${text}")
    string(REPLACE ">" "&gt;" text "${text}")
    string(REPLACE "\"" "&quot;" text "${text}")
    set(${outvar} "${text}" PARENT_SCOPE)
endfunction()

function(nt_add_resources first)
    if(NOT "FILES" IN_LIST ARGN)
        # nt_add_resources(<outvar> <qrc>... [OPTIONS ...])
        cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "OPTIONS")
        set(generated)
        foreach(qrc IN LISTS arg_UNPARSED_ARGUMENTS)
            get_filename_component(abs "${qrc}" ABSOLUTE)
            get_filename_component(name "${qrc}" NAME_WE)
            string(MAKE_C_IDENTIFIER "${name}" name)
            set(out "${CMAKE_CURRENT_BINARY_DIR}/qrc_${name}.cpp")
            _nt_rcc_command("${abs}" "${out}" "${name}" "${arg_OPTIONS}")
            list(APPEND generated "${out}")
        endforeach()
        set(${first} ${${first}} ${generated} PARENT_SCOPE)
        return()
    endif()

    # nt_add_resources(<target> <name> FILES ...)
    set(target "${first}")
    cmake_parse_arguments(PARSE_ARGV 2 arg "BIG_RESOURCES" "PREFIX;BASE;LANG;OUTPUT_TARGETS" "FILES;OPTIONS")
    list(GET ARGN 0 name)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "nt_add_resources: '${target}' is not a target")
    endif()
    if(NOT arg_PREFIX)
        set(arg_PREFIX "/")
    endif()
    if(NOT arg_BASE)
        set(arg_BASE "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    get_filename_component(base "${arg_BASE}" ABSOLUTE)
    string(MAKE_C_IDENTIFIER "${name}" id)

    _nt_xml_escape(prefix "${arg_PREFIX}")
    set(qrc_content "<RCC>\n  <qresource prefix=\"${prefix}\"")
    if(arg_LANG)
        _nt_xml_escape(lang "${arg_LANG}")
        string(APPEND qrc_content " lang=\"${lang}\"")
    endif()
    string(APPEND qrc_content ">\n")
    foreach(file IN LISTS arg_FILES)
        get_filename_component(abs "${file}" ABSOLUTE)
        get_source_file_property(alias "${file}" QT_RESOURCE_ALIAS)
        if(NOT alias)
            file(RELATIVE_PATH alias "${base}" "${abs}")
        endif()
        _nt_xml_escape(alias "${alias}")
        _nt_xml_escape(path "${abs}")
        string(APPEND qrc_content "    <file alias=\"${alias}\">${path}</file>\n")
    endforeach()
    string(APPEND qrc_content "  </qresource>\n</RCC>\n")

    # The .qrc file is rewritten only when it changes, so that it does not trigger rebuilds.
    set(dir "${CMAKE_CURRENT_BINARY_DIR}/.nt_rcc")
    set(qrc "${dir}/${id}.qrc")
    file(WRITE "${qrc}.in" "${qrc_content}")
    configure_file("${qrc}.in" "${qrc}" COPYONLY)
    set(out "${dir}/qrc_${id}.cpp")
    _nt_rcc_command("${qrc}" "${out}" "${id}" "${arg_OPTIONS}")
    target_sources(${target} PRIVATE "${out}")
    if(arg_OUTPUT_TARGETS)
        set(${arg_OUTPUT_TARGETS} "" PARENT_SCOPE)
    endif()
endfunction()

function(nt_add_binary_resources target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "DESTINATION" "OPTIONS")
    set(inputs)
    set(deps)
    foreach(qrc IN LISTS arg_UNPARSED_ARGUMENTS)
        get_filename_component(abs "${qrc}" ABSOLUTE)
        _nt_qrc_dependencies(more "${abs}")
        list(APPEND inputs "${abs}")
        list(APPEND deps ${more})
    endforeach()
    if(NOT arg_DESTINATION)
        set(arg_DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${target}.rcc")
    endif()
    get_filename_component(dest "${arg_DESTINATION}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    add_custom_command(
        OUTPUT "${dest}"
        COMMAND ntrcc --binary ${arg_OPTIONS} -o "${dest}" ${inputs}
        DEPENDS ${deps} ntrcc
        COMMENT "ntrcc --binary ${target}"
        VERBATIM)
    add_custom_target(${target} ALL DEPENDS "${dest}")
endfunction()

function(nt_enable_autorcc)
    foreach(target IN LISTS ARGN)
        set_target_properties(${target} PROPERTIES
            AUTORCC ON
            AUTORCC_EXECUTABLE "$<TARGET_FILE:ntrcc>"
            AUTOGEN_TARGET_DEPENDS ntrcc)
    endforeach()
endfunction()

if(NOT COMMAND qt_add_resources)
    function(qt_add_resources)
        nt_add_resources(${ARGV})
        if(NOT "FILES" IN_LIST ARGN)
            set(${ARGV0} ${${ARGV0}} PARENT_SCOPE)
        endif()
    endfunction()
endif()
if(NOT COMMAND qt6_add_resources)
    function(qt6_add_resources)
        nt_add_resources(${ARGV})
        if(NOT "FILES" IN_LIST ARGN)
            set(${ARGV0} ${${ARGV0}} PARENT_SCOPE)
        endif()
    endfunction()
endif()
if(NOT COMMAND qt_add_binary_resources)
    function(qt_add_binary_resources)
        nt_add_binary_resources(${ARGV})
    endfunction()
endif()
if(NOT COMMAND qt6_add_binary_resources)
    function(qt6_add_binary_resources)
        nt_add_binary_resources(${ARGV})
    endfunction()
endif()
