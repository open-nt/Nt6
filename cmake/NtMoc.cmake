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
