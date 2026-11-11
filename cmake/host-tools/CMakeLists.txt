# MIT License - see LICENSE
#
# Builds ntmoc and ntrcc for the build machine. A cross build of Nt configures this project with
# the native compiler (see ../NtHostTools.cmake); it is not part of the main build.
cmake_minimum_required(VERSION 3.16)
project(NtHostTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

foreach(tool ntmoc ntrcc)
    add_executable(${tool} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/${tool}/main.cpp)
    target_compile_definitions(${tool} PRIVATE NT_VERSION_STRING="${NT_VERSION}")
endforeach()
