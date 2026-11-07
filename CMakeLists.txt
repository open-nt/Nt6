cmake_minimum_required(VERSION 3.16)
# The version lives in the VERSION file at the root of the repository.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION NT_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
project(Nt VERSION ${NT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NT_BUILD_TESTS "Build tests" ON)
option(NT_BUILD_EXAMPLES "Build examples" ON)

find_package(Threads REQUIRED)

add_library(NtCore
    src/dispatcher.cpp
    src/qbuffer.cpp
    src/qbytearray.cpp
    src/qchar.cpp
    src/qcoreapplication.cpp
    src/qcoreevent.cpp
    src/qdebug.cpp
    src/qeventloop.cpp
    src/qfile.cpp
    src/qiodevice.cpp
    src/qobject.cpp
    src/qsocketnotifier.cpp
    src/qstring.cpp
    src/qtimer.cpp
)
add_library(Nt::Core ALIAS NtCore)
add_library(Qt6::Core ALIAS NtCore)

target_include_directories(NtCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtCore>
        $<INSTALL_INTERFACE:include/nt6>
        $<INSTALL_INTERFACE:include/nt6/QtCore>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtCore PUBLIC Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(NtCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(NtNetwork
    src/network/qabstractsocket.cpp
    src/network/qhostaddress.cpp
    src/network/qtcpserver.cpp
    src/network/qudpsocket.cpp
)
add_library(Nt::Network ALIAS NtNetwork)
add_library(Qt6::Network ALIAS NtNetwork)
target_include_directories(NtNetwork
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtNetwork>
        $<INSTALL_INTERFACE:include/nt6/QtNetwork>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtNetwork PUBLIC NtCore)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(NtNetwork PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Moc replacement and its CMake helpers.
add_executable(ntmoc tools/ntmoc/main.cpp)
add_executable(Nt::moc ALIAS ntmoc)
add_executable(Qt6::moc ALIAS ntmoc)
target_compile_definitions(ntmoc PRIVATE NT_VERSION_STRING="${PROJECT_VERSION}")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtMoc.cmake)

# Documentation generator; build the docs with the `docs` target.
add_executable(ndoc tools/ndoc/main.cpp)
add_executable(Nt::ndoc ALIAS ndoc)
target_compile_definitions(ndoc PRIVATE NT_VERSION_STRING="${PROJECT_VERSION}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ndoc PRIVATE -Wall -Wextra -Wpedantic)
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtDoc.cmake)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    nt_add_docs(docs CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/doc/ndoc.conf STRICT)

    # The README badge is published next to the documentation.
    add_custom_target(badge
        COMMAND ${CMAKE_COMMAND}
            -DVERSION_FILE=${CMAKE_CURRENT_SOURCE_DIR}/VERSION
            -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/doc/html/badges
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtBadge.cmake
        COMMENT "Writing version badge"
        VERBATIM)
    add_dependencies(docs badge)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtInstall.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtPackaging.cmake)
endif()

if(NT_BUILD_TESTS)
    enable_testing()

    add_executable(nt_tests tests/test_main.cpp)
    target_link_libraries(nt_tests PRIVATE Nt::Core)
    add_test(NAME nt_tests COMMAND nt_tests)

    add_executable(nt_core_tests tests/test_core.cpp)
    target_link_libraries(nt_core_tests PRIVATE Nt::Core)
    add_test(NAME nt_core_tests COMMAND nt_core_tests)

    add_executable(nt_network_tests tests/test_network.cpp)
    target_link_libraries(nt_network_tests PRIVATE Nt::Network)
    add_test(NAME nt_network_tests COMMAND nt_network_tests)

    add_test(NAME ndoc_tests
             COMMAND ${CMAKE_COMMAND}
                 -DNDOC=$<TARGET_FILE:ndoc>
                 -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/ndoc
                 -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/ndoc_tests
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ndoc/check.cmake)

    add_test(NAME badge_tests
             COMMAND ${CMAKE_COMMAND}
                 -DBADGE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtBadge.cmake
                 -DVERSION_FILE=${CMAKE_CURRENT_SOURCE_DIR}/VERSION
                 -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/badge_tests
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/badge/check.cmake)

    nt_wrap_cpp(moc_test_sources tests/moc_classes.h)
    add_executable(nt_moc_tests tests/test_moc.cpp ${moc_test_sources})
    target_include_directories(nt_moc_tests PRIVATE tests)
    target_link_libraries(nt_moc_tests PRIVATE Nt::Core)
    add_test(NAME nt_moc_tests COMMAND nt_moc_tests)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(nt_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(nt_moc_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(NT_BUILD_EXAMPLES)
    add_executable(hello examples/hello.cpp)
    target_link_libraries(hello PRIVATE Nt::Core)

    # Same flow as a Qt project using CMAKE_AUTOMOC.
    add_executable(moc_example examples/moc/main.cpp examples/moc/shape.h)
    target_link_libraries(moc_example PRIVATE Nt::Core)
    nt_enable_automoc(moc_example)
endif()
