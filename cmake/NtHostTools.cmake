# MIT License - see LICENSE
#
# Host tools for a cross build. ntmoc and ntrcc run while the libraries are built, so they have to
# be programs for the build machine, not for the target. This file defines the imported targets
# ntmoc and ntrcc, in one of two ways:
#
#   NT_HOST_TOOLS_DIR  A directory that holds ntmoc and ntrcc, for example the bin directory of an
#                      installed Nt, or the build directory of a native build. When it is empty,
#                      the tools are built with the build machine's compiler, in nt-host-tools
#                      under the build directory, while CMake configures the project.
#   NT_HOST_CXX_COMPILER  The compiler for that build. The default is the one CMake finds once
#                      CC, CXX and the flag variables of the cross build are out of the environment.
#
# The tools are built without zlib and Zstandard: a target that is cross built for seldom has them,
# and resources that ntrcc compressed could not be read there.

set(NT_HOST_TOOLS_DIR "" CACHE PATH "Directory with ntmoc and ntrcc built for the build machine")
set(NT_HOST_CXX_COMPILER "" CACHE FILEPATH "C++ compiler of the build machine, used to build ntmoc and ntrcc")

set(_nt_host_dir "${NT_HOST_TOOLS_DIR}")
if(NOT _nt_host_dir)
    set(_nt_host_dir "${CMAKE_BINARY_DIR}/nt-host-tools")
    set(_nt_host_args)
    if(NT_HOST_CXX_COMPILER)
        list(APPEND _nt_host_args "-DCMAKE_CXX_COMPILER=${NT_HOST_CXX_COMPILER}")
    endif()
    # The environment of a cross build may name the cross compiler and its flags.
    set(_nt_host_env ${CMAKE_COMMAND} -E env
        --unset=CC --unset=CXX --unset=CFLAGS --unset=CXXFLAGS --unset=CPPFLAGS --unset=LDFLAGS)
    execute_process(
        COMMAND ${_nt_host_env} ${CMAKE_COMMAND}
            -S "${CMAKE_CURRENT_LIST_DIR}/host-tools" -B "${_nt_host_dir}"
            -DCMAKE_BUILD_TYPE=Release "-DNT_VERSION=${NT_VERSION}" ${_nt_host_args}
        RESULT_VARIABLE _nt_host_result
        OUTPUT_VARIABLE _nt_host_output
        ERROR_VARIABLE _nt_host_output)
    if(_nt_host_result EQUAL 0)
        execute_process(
            COMMAND ${_nt_host_env} ${CMAKE_COMMAND} --build "${_nt_host_dir}"
            RESULT_VARIABLE _nt_host_result
            OUTPUT_VARIABLE _nt_host_output
            ERROR_VARIABLE _nt_host_output)
    endif()
    if(NOT _nt_host_result EQUAL 0)
        message(FATAL_ERROR "Could not build ntmoc and ntrcc for the build machine:\n${_nt_host_output}\n"
            "Set NT_HOST_CXX_COMPILER to a native C++17 compiler, or NT_HOST_TOOLS_DIR to a directory "
            "that holds the two tools.")
    endif()
endif()

foreach(_nt_tool ntmoc ntrcc)
    find_program(NT_HOST_${_nt_tool} ${_nt_tool} PATHS "${_nt_host_dir}" "${_nt_host_dir}/bin"
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    if(NOT NT_HOST_${_nt_tool})
        message(FATAL_ERROR "${_nt_tool} was not found in ${_nt_host_dir} (NT_HOST_TOOLS_DIR)")
    endif()
    add_executable(${_nt_tool} IMPORTED GLOBAL)
    set_target_properties(${_nt_tool} PROPERTIES IMPORTED_LOCATION "${NT_HOST_${_nt_tool}}")
endforeach()
