# MIT License - see LICENSE
#
# Install rules and the CMake package (find_package(Nt6)).
#
# Headers go to <prefix>/include/nt6 so that Nt can sit next to a real Qt without
# clashing. Two components are installed:
#
#   Development  headers, static or shared libraries, CMake package
#   Tools        ntmoc and ndoc

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(NT_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/nt6)
set(NT_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Nt6)

set_target_properties(NtCore PROPERTIES EXPORT_NAME Core)
set_target_properties(NtNetwork PROPERTIES EXPORT_NAME Network)
set_target_properties(ntmoc PROPERTIES EXPORT_NAME moc)
set_target_properties(ndoc PROPERTIES EXPORT_NAME ndoc)

install(TARGETS NtCore NtNetwork
    EXPORT Nt6Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development)
install(TARGETS ntmoc ndoc
    EXPORT Nt6Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools)

install(DIRECTORY include/QtCore include/QtNetwork
    DESTINATION ${NT_INSTALL_INCLUDEDIR}
    COMPONENT Development
    PATTERN "*_p.h" EXCLUDE)

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
