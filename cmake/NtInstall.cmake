# MIT License - see LICENSE
#
# Install rules and the CMake package (find_package(Nt6)).
#
# Headers go to <prefix>/include/nt6 so that Nt can sit next to a real Qt without
# clashing. Two components are installed:
#
#   Development  headers, static or shared libraries, CMake package
#   Tools        ntmoc, ntrcc, ndoc and, with Vulkan, ntgltfview

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(NT_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/nt6)
set(NT_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Nt6)

set(NT_MODULES Core Network Gpio SerialPort Sql DBus HttpServer WebSockets Gui Widgets Svg SvgWidgets Charts OpenGL OpenGLWidgets Qml FM FMQml Gamepad Gltf Barcode Positioning Sensors Protobuf SerialBus Mqtt Twin Comms TwinNetwork Multimedia MultimediaWidgets Concurrent Xml Test)
set(NT_INSTALL_TARGETS)
set(NT_INSTALL_INCLUDES)
foreach(module IN LISTS NT_MODULES)
    # A cross build for Android leaves out some modules (see doc/android.h).
    if(TARGET Nt${module})
        set_target_properties(Nt${module} PROPERTIES EXPORT_NAME ${module})
        list(APPEND NT_INSTALL_TARGETS Nt${module})
        list(APPEND NT_INSTALL_INCLUDES include/Qt${module})
    endif()
endforeach()
if(TARGET NtWaylandCompositor)
        install(TARGETS NtWaylandCompositor
        EXPORT Nt6Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
endif()
if(NOT CMAKE_CROSSCOMPILING)
    set_target_properties(ntmoc PROPERTIES EXPORT_NAME moc)
    set_target_properties(ndoc PROPERTIES EXPORT_NAME ndoc)
    set_target_properties(ntrcc PROPERTIES EXPORT_NAME rcc)
    if(TARGET ntprotobufgen)
        set_target_properties(ntprotobufgen PROPERTIES EXPORT_NAME protobufgen)
    endif()
endif()

install(TARGETS ${NT_INSTALL_TARGETS}
    EXPORT Nt6Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
if(NOT CMAKE_CROSSCOMPILING)
    install(TARGETS ntmoc ntrcc ndoc
        EXPORT Nt6Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools)
    if(TARGET ntprotobufgen)
        install(TARGETS ntprotobufgen
            EXPORT Nt6Targets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools)
    endif()
endif()
if(TARGET freetype)
    # Nt GUI for Android is built with a FreeType of its own, which the static library needs when it is linked.
    set_target_properties(freetype PROPERTIES EXPORT_NAME freetype)
    install(TARGETS freetype
        EXPORT Nt6Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
endif()
if(TARGET ntgltfview)
    install(TARGETS ntgltfview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools)
endif()

install(DIRECTORY ${NT_INSTALL_INCLUDES}
    DESTINATION ${NT_INSTALL_INCLUDEDIR}
    COMPONENT Development
    PATTERN "*_p.h" EXCLUDE)

# qconfig.h is generated; qassoc_p.h is private, but qmap.h and qhash.h include it.
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/QtCore/qconfig.h include/QtCore/qassoc_p.h
    DESTINATION ${NT_INSTALL_INCLUDEDIR}/QtCore
    COMPONENT Development)
install(EXPORT Nt6Targets
    NAMESPACE Nt6::
    DESTINATION ${NT_INSTALL_CMAKEDIR}
    COMPONENT Development)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Nt6Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/Nt6Config.cmake
    INSTALL_DESTINATION ${NT_INSTALL_CMAKEDIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/Nt6ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
if(NT_ANDROID)
    set(NT_HOST_TOOLS_DIR_FOR_TOOLCHAIN "${_nt_host_dir}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/nt-android.toolchain.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/nt-android.toolchain.cmake @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nt-android.toolchain.cmake
        DESTINATION ${NT_INSTALL_CMAKEDIR} COMPONENT Development)
    install(FILES cmake/android/NtActivity.java DESTINATION ${NT_INSTALL_CMAKEDIR}/android COMPONENT Development)
    # Qt 6 projects find Nt under Qt's own package name.
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Qt6Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/Qt6Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Qt6)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/Qt6ConfigVersion.cmake
        VERSION 6.8.0
        COMPATIBILITY AnyNewerVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Qt6Config.cmake ${CMAKE_CURRENT_BINARY_DIR}/Qt6ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Qt6
        COMPONENT Development)
endif()
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/Nt6Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/Nt6ConfigVersion.cmake
    cmake/NtMoc.cmake
    cmake/NtRcc.cmake
    cmake/NtProtobuf.cmake
    cmake/NtDoc.cmake
    cmake/NtAndroid.cmake
    cmake/NtAndroidApk.cmake
    DESTINATION ${NT_INSTALL_CMAKEDIR}
    COMPONENT Development)

# Debian wants /usr/share/doc/<package>/copyright in every package.
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/libnt6-dev
    RENAME copyright
    COMPONENT Development)
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/nt6-tools
    RENAME copyright
    COMPONENT Tools)

# "make uninstall" removes the files listed in install_manifest*.txt.
set(NT_UNINSTALL_KEEP_DIRS
    ${CMAKE_INSTALL_FULL_BINDIR}
    ${CMAKE_INSTALL_FULL_LIBDIR}
    ${CMAKE_INSTALL_FULL_INCLUDEDIR}
    ${CMAKE_INSTALL_FULL_DATAROOTDIR}
    ${CMAKE_INSTALL_FULL_DATAROOTDIR}/doc
    ${CMAKE_INSTALL_FULL_LIBDIR}/cmake)
if(NOT TARGET uninstall)
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND}
            -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DPREFIX=${CMAKE_INSTALL_PREFIX}
            "-DKEEP_DIRS=${NT_UNINSTALL_KEEP_DIRS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtUninstall.cmake
        COMMENT "Removing installed files"
        VERBATIM)
endif()
