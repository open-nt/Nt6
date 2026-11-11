# MIT License - see LICENSE
#
# CPack configuration: .deb for Debian and Ubuntu, .rpm for Enterprise Linux and
# Fedora, and .pkg.tar.zst for Arch Linux.
#
#   cmake -S . -B build
#   cmake --build build
#   cpack --config build/CPackConfig.cmake            # format chosen from /etc/os-release
#   cpack --config build/CPackConfig.cmake -G DEB     # or RPM, or External (pacman)
#
# Packages are built on the distribution they are for, so that dependencies and
# the C++ runtime match. packaging/build-packages.sh does that in containers.

# What this machine is: Ubuntu 24.04 gives NT_OS_ID=ubuntu, NT_OS_VERSION_ID=24.04.
set(NT_OS_ID "")
set(NT_OS_ID_LIKE "")
set(NT_OS_VERSION_ID "")
if(EXISTS /etc/os-release)
    file(STRINGS /etc/os-release os_release)
    foreach(line IN LISTS os_release)
        if(line MATCHES "^ID=\"?([^\"]*)\"?$")
            set(NT_OS_ID "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^ID_LIKE=\"?([^\"]*)\"?$")
            set(NT_OS_ID_LIKE "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^VERSION_ID=\"?([^\"]*)\"?$")
            set(NT_OS_VERSION_ID "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()
set(distro_names "${NT_OS_ID} ${NT_OS_ID_LIKE}")

if(NOT CPACK_GENERATOR)
    if(distro_names MATCHES "debian|ubuntu")
        set(CPACK_GENERATOR DEB)
    elseif(distro_names MATCHES "rhel|fedora|centos|rocky|almalinux")
        set(CPACK_GENERATOR RPM)
    elseif(distro_names MATCHES "arch")
        set(CPACK_GENERATOR External)
    else()
        set(CPACK_GENERATOR TGZ)
    endif()
endif()

# Packages install under /usr, whatever prefix a plain "cmake --install" would use.
if(NOT CPACK_PACKAGING_INSTALL_PREFIX)
    if(CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
        set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
    else()
        set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
    endif()
endif()

set(CPACK_PACKAGE_NAME nt6)
# The VERSION file may carry a pre-release suffix (6.0.0-rc1). Each package format
# spells it its own way, so the release sorts before 6.0.0: dpkg and rpm use a tilde,
# and pacman, which forbids hyphens in pkgver, an underscore.
set(CPACK_PACKAGE_VERSION ${NT_VERSION})
string(REPLACE "-" "~" CPACK_DEBIAN_PACKAGE_VERSION "${NT_VERSION}")
string(REPLACE "-" "~" CPACK_RPM_PACKAGE_VERSION "${NT_VERSION}")
string(REPLACE "-" "_" CPACK_NT_PACMAN_VERSION "${NT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Nt contributors")
set(CPACK_PACKAGE_CONTACT "Nt contributors <noreply@github.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/open-nt/Nt6")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MIT-licensed clean-room implementation of the Qt 6 API")
set(CPACK_PACKAGE_DESCRIPTION
    "Nt is a small, MIT-licensed, clean-room implementation of the Qt 6 API, built module by module. It is not affiliated with The Qt Company.")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_STRIP_FILES ON)
set(CPACK_COMPONENTS_ALL Development Tools)

# Debian and Ubuntu: libnt6-dev and nt6-tools.
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
if(NT_OS_ID STREQUAL "ubuntu")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1~ubuntu${NT_OS_VERSION_ID}")
elseif(NT_OS_ID STREQUAL "debian")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1~deb${NT_OS_VERSION_ID}")
endif()
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME libnt6-dev)
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_SECTION libdevel)
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS "libc6-dev, libssl-dev, libxcb1-dev, libfreetype-dev, libfontconfig-dev, zlib1g-dev, libzstd-dev, libsqlite3-dev")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_RECOMMENDS "cmake, g++ | clang")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY} (development files)")
set(CPACK_DEBIAN_TOOLS_PACKAGE_NAME nt6-tools)
set(CPACK_DEBIAN_TOOLS_PACKAGE_SECTION devel)
set(CPACK_DEBIAN_TOOLS_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_TOOLS_PACKAGE_DESCRIPTION "ntmoc and ntrcc, moc and rcc replacements, and ndoc, a documentation generator, for Nt")

# Enterprise Linux and Fedora: nt6-devel and nt6-tools.
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME nt6-devel)
set(CPACK_RPM_DEVELOPMENT_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_DEVELOPMENT_PACKAGE_REQUIRES "glibc-devel, openssl-devel, libxcb-devel, freetype-devel, fontconfig-devel, zlib-devel, libzstd-devel, sqlite-devel")
set(CPACK_RPM_DEVELOPMENT_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY} (development files)")
set(CPACK_RPM_TOOLS_PACKAGE_NAME nt6-tools)
set(CPACK_RPM_TOOLS_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_TOOLS_PACKAGE_SUMMARY "ntmoc, ntrcc and ndoc for Nt")
# Directories that belong to the system or to other packages.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    ${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake
    ${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/doc)

# Arch Linux: CPack has no pacman generator, so the External generator calls
# NtPacman.cmake, which writes nt6-<version>-<release>-<arch>.pkg.tar.zst.
set(CPACK_EXTERNAL_ENABLE_STAGING ON)
set(CPACK_EXTERNAL_PACKAGE_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NtPacman.cmake)
set(CPACK_EXTERNAL_REQUESTED_VERSIONS 1.0)
set(CPACK_NT_PACMAN_RELEASE 1)
set(CPACK_NT_PACMAN_DEPENDS "gcc-libs;glibc;openssl;libxcb;freetype2;fontconfig;zlib;zstd")
set(CPACK_NT_PACMAN_OPTDEPENDS "cmake: build projects that use Nt")
set(CPACK_NT_PACMAN_ARCH ${CMAKE_SYSTEM_PROCESSOR})

include(CPack)
