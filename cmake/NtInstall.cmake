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

set_target_properties(NtCore PROPERTIES EXPORT_NAME Core)
set_target_properties(NtNetwork PROPERTIES EXPORT_NAME Network)
set_target_properties(NtTest PROPERTIES EXPORT_NAME Test)
set_target_properties(NtConcurrent PROPERTIES EXPORT_NAME Concurrent)
set_target_properties(NtSerialPort PROPERTIES EXPORT_NAME SerialPort)
set_target_properties(NtGpio PROPERTIES EXPORT_NAME Gpio)
set_target_properties(NtSql PROPERTIES EXPORT_NAME Sql)
set_target_properties(NtDBus PROPERTIES EXPORT_NAME DBus)
set_target_properties(NtHttpServer PROPERTIES EXPORT_NAME HttpServer)
set_target_properties(NtWebSockets PROPERTIES EXPORT_NAME WebSockets)
set_target_properties(NtGui PROPERTIES EXPORT_NAME Gui)
set_target_properties(NtWidgets PROPERTIES EXPORT_NAME Widgets)
set_target_properties(NtSvg PROPERTIES EXPORT_NAME Svg)
set_target_properties(NtSvgWidgets PROPERTIES EXPORT_NAME SvgWidgets)
set_target_properties(NtCharts PROPERTIES EXPORT_NAME Charts)
set_target_properties(NtQml PROPERTIES EXPORT_NAME Qml)
set_target_properties(NtFM PROPERTIES EXPORT_NAME FM)
set_target_properties(NtFMQml PROPERTIES EXPORT_NAME FMQml)
set_target_properties(NtGamepad PROPERTIES EXPORT_NAME Gamepad)
set_target_properties(NtGltf PROPERTIES EXPORT_NAME Gltf)
set_target_properties(NtMultimedia PROPERTIES EXPORT_NAME Multimedia)
set_target_properties(NtMultimediaWidgets PROPERTIES EXPORT_NAME MultimediaWidgets)
set_target_properties(NtOpenGL PROPERTIES EXPORT_NAME OpenGL)
set_target_properties(ntmoc PROPERTIES EXPORT_NAME moc)
set_target_properties(ndoc PROPERTIES EXPORT_NAME ndoc)
set_target_properties(ntrcc PROPERTIES EXPORT_NAME rcc)

install(TARGETS NtCore NtNetwork NtGpio NtSerialPort NtSql NtDBus NtWebSockets NtHttpServer NtGui NtWidgets NtSvg NtSvgWidgets NtCharts NtOpenGL NtQml NtFM NtFMQml NtGamepad NtGltf NtMultimedia NtMultimediaWidgets NtConcurrent NtTest
    EXPORT Nt6Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
install(TARGETS ntmoc ntrcc ndoc
    EXPORT Nt6Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools)
if(TARGET ntgltfview)
    install(TARGETS ntgltfview RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools)
endif()

install(DIRECTORY include/QtCore include/QtNetwork include/QtGpio include/QtSerialPort include/QtSql include/QtDBus include/QtHttpServer include/QtWebSockets include/QtGui include/QtWidgets include/QtSvg include/QtSvgWidgets include/QtCharts include/QtOpenGL include/QtQml include/QtFM include/QtFMQml include/QtGamepad include/QtGltf include/QtMultimedia include/QtMultimediaWidgets include/QtConcurrent include/QtTest
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
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/Nt6Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/Nt6ConfigVersion.cmake
    cmake/NtMoc.cmake
    cmake/NtRcc.cmake
    cmake/NtDoc.cmake
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
