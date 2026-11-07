# MIT License - see LICENSE
#
# Moc support for Nt: replaces Qt's moc with ntmoc.
#
#   nt_wrap_cpp(<outvar> <header>...)   explicit form, like qt_wrap_cpp()
#   nt_enable_automoc(<target>...)      CMake AUTOMOC driven by ntmoc

if(NOT TARGET ntmoc)
    message(FATAL_ERROR "NtMoc.cmake needs the ntmoc target")
endif()

function(nt_wrap_cpp outvar)
    set(generated)
    foreach(header IN LISTS ARGN)
        get_filename_component(abs "${header}" ABSOLUTE)
        get_filename_component(name "${header}" NAME_WE)
        set(out "${CMAKE_CURRENT_BINARY_DIR}/moc_${name}.cpp")
        add_custom_command(
            OUTPUT "${out}"
            COMMAND ntmoc -o "${out}" "${abs}"
            DEPENDS "${abs}" ntmoc
            COMMENT "ntmoc ${header}"
            VERBATIM)
        set_source_files_properties("${out}" PROPERTIES SKIP_AUTOMOC ON GENERATED ON)
        list(APPEND generated "${out}")
    endforeach()
    set(${outvar} "${generated}" PARENT_SCOPE)
endfunction()

function(nt_enable_automoc)
    foreach(target IN LISTS ARGN)
        set_target_properties(${target} PROPERTIES
            AUTOMOC ON
            AUTOMOC_EXECUTABLE "$<TARGET_FILE:ntmoc>"
            AUTOGEN_TARGET_DEPENDS ntmoc)
    endforeach()
endfunction()

# Runs ntmoc over the headers of a library that declare Q_OBJECT or Q_GADGET, so that its classes
# have meta-objects. Each source file that implements such a class should end with
# #include "moc_<header>.cpp", which keeps the meta-object in the same object file as the class, so
# that it is linked whenever the class is used from a static library. A generated file that no
# source includes is compiled as a source of its own.
#
#   nt_moc_library(<target> <include directory>...)
function(nt_moc_library target)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    set(included)
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" lines REGEX "#include \"moc_[A-Za-z0-9_]+\\.cpp\"")
        foreach(line IN LISTS lines)
            string(REGEX MATCH "moc_[A-Za-z0-9_]+\\.cpp" name "${line}")
            list(APPEND included "${name}")
        endforeach()
    endforeach()
    set(generated)
    foreach(dir IN LISTS ARGN)
        file(GLOB headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/q*.h")
        foreach(header IN LISTS headers)
            file(STRINGS "${header}" hit LIMIT_COUNT 1 REGEX "^[ ]*Q_(OBJECT|GADGET)[ ]*$")
            if(hit)
                nt_wrap_cpp(out "${header}")
                list(APPEND generated ${out})
                get_filename_component(file "${out}" NAME)
                if(NOT file IN_LIST included)
                    target_sources(${target} PRIVATE ${out})
                endif()
            endif()
        endforeach()
    endforeach()
    add_custom_target(${target}_moc DEPENDS ${generated})
    add_dependencies(${target} ${target}_moc)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()
