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

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtCompilerOptions.cmake)

find_package(Threads REQUIRED)

# The version macros come from the VERSION file.
configure_file(cmake/qconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/QtCore/qconfig.h @ONLY)

add_library(NtCore
    src/dispatcher.cpp
    src/qbuffer.cpp
    src/qbytearray.cpp
    src/qchar.cpp
    src/qcommandlineparser.cpp
    src/qcoreapplication.cpp
    src/qcoreevent.cpp
    src/qcryptographichash.cpp
    src/qdatastream.cpp
    src/qdatetime.cpp
    src/qdebug.cpp
    src/qdir.cpp
    src/qeventloop.cpp
    src/qfile.cpp
    src/qfileinfo.cpp
    src/qiodevice.cpp
    src/qlibraryinfo.cpp
    src/qobject.cpp
    src/qrandom.cpp
    src/qsavefile.cpp
    src/qsocketnotifier.cpp
    src/qstandardpaths.cpp
    src/qstring.cpp
    src/qtemporaryfile.cpp
    src/qtextstream.cpp
    src/qtimer.cpp
    src/qurl.cpp
    src/quuid.cpp
    src/qversionnumber.cpp
)
add_library(Nt::Core ALIAS NtCore)
add_library(Qt6::Core ALIAS NtCore)

target_include_directories(NtCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/QtCore>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include/nt6>
        $<INSTALL_INTERFACE:include/nt6/QtCore>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(NtCore PUBLIC Threads::Threads)

nt_set_compile_options(NtCore)

add_library(NtNetwork
    src/network/qabstractsocket.cpp
    src/network/qhostaddress.cpp
    src/network/qhostinfo.cpp
    src/network/qlocalserver.cpp
    src/network/qlocalsocket.cpp
    src/network/qnetworkaccessmanager.cpp
    src/network/qnetworkinterface.cpp
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

# TLS uses the system's OpenSSL. Without it, QSslSocket and https: are absent.
option(NT_USE_OPENSSL "Build TLS support with the system OpenSSL" ON)
set(NT_HAVE_OPENSSL OFF)
if(NT_USE_OPENSSL)
    find_package(OpenSSL 1.1.1 QUIET)
    if(OPENSSL_FOUND)
        set(NT_HAVE_OPENSSL ON)
    else()
        message(WARNING "OpenSSL 1.1.1 or newer was not found: building Nt Network without TLS")
    endif()
endif()
if(NT_HAVE_OPENSSL)
    target_sources(NtNetwork PRIVATE
        src/network/qssl.cpp
        src/network/qsslserver.cpp
        src/network/qsslsocket.cpp)
    target_link_libraries(NtNetwork PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
    target_compile_definitions(NtNetwork PUBLIC QT_NO_SSL)
endif()
nt_set_compile_options(NtNetwork)

# Moc replacement and its CMake helpers.
add_executable(ntmoc tools/ntmoc/main.cpp)
add_executable(Nt::moc ALIAS ntmoc)
add_executable(Qt6::moc ALIAS ntmoc)
target_compile_definitions(ntmoc PRIVATE NT_VERSION_STRING="${PROJECT_VERSION}")
nt_set_compile_options(ntmoc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtMoc.cmake)

# Documentation generator; build the docs with the `docs` target.
add_executable(ndoc tools/ndoc/main.cpp)
add_executable(Nt::ndoc ALIAS ndoc)
target_compile_definitions(ndoc PRIVATE NT_VERSION_STRING="${PROJECT_VERSION}")
nt_set_compile_options(ndoc)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtDoc.cmake)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    nt_add_docs(docs CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/doc/ndoc.conf STRICT REQUIRE_COMPLETE)

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

    foreach(suite filesystem streams types)
        add_executable(nt_${suite}_tests tests/test_${suite}.cpp)
        target_link_libraries(nt_${suite}_tests PRIVATE Nt::Core)
        add_test(NAME nt_${suite}_tests COMMAND nt_${suite}_tests)
    endforeach()

    add_executable(nt_core_value_tests tests/test_core_values.cpp)
    target_link_libraries(nt_core_value_tests PRIVATE Nt::Core)
    target_compile_definitions(nt_core_value_tests PRIVATE NT_EXPECTED_VERSION="${PROJECT_VERSION}")
    add_test(NAME nt_core_value_tests COMMAND nt_core_value_tests)

    if(NT_HAVE_OPENSSL)
        add_executable(nt_ssl_tests tests/test_ssl.cpp)
        target_link_libraries(nt_ssl_tests PRIVATE Nt::Network OpenSSL::SSL OpenSSL::Crypto)
        add_test(NAME nt_ssl_tests COMMAND nt_ssl_tests)
    endif()

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

    nt_set_compile_options(nt_tests nt_core_tests nt_core_value_tests nt_network_tests nt_moc_tests)
    foreach(suite filesystem streams types)
        nt_set_compile_options(nt_${suite}_tests)
    endforeach()
endif()

if(NT_BUILD_EXAMPLES)
    add_executable(hello examples/hello.cpp)
    target_link_libraries(hello PRIVATE Nt::Core)

    # Same flow as a Qt project using CMAKE_AUTOMOC.
    add_executable(moc_example examples/moc/main.cpp examples/moc/shape.h)
    target_link_libraries(moc_example PRIVATE Nt::Core)
    nt_enable_automoc(moc_example)
    nt_set_compile_options(hello moc_example)
endif()
