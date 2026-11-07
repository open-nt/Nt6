// MIT License - see LICENSE
//
// Install guide page for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page install.html
    \title Installing Nt
    \brief Installing Nt from the .deb, .rpm and pacman packages for Ubuntu, Enterprise Linux and Arch Linux.

    Nt is packaged for these systems. Each package is built on the system it is
    for, so that its dependencies and C++ runtime match.

    \table
    \header
        \li System
        \li Format
        \li Packages
    \row
        \li Ubuntu 22.04, 24.04 and 26.04
        \li \c .deb
        \li \c libnt6-dev and \c nt6-tools
    \row
        \li Enterprise Linux 9 and 10 (RHEL, Rocky Linux, AlmaLinux)
        \li \c .rpm
        \li \c nt6-devel and \c nt6-tools
    \row
        \li Arch Linux
        \li \c .pkg.tar.zst (pacman)
        \li \c nt6, with everything in one package
    \endtable

    The development package (\c libnt6-dev or \c nt6-devel) holds the headers,
    the static libraries and the CMake package. The tools package holds
    \c ntmoc, a replacement for Qt's moc, and \c ndoc, the documentation
    generator. Install both to build a Qt-style project.

    The packages need a C++17 compiler and CMake 3.18 or newer to be useful, which
    the sections below install too. Nt is statically linked, so a program built with
    it needs no Nt package to run.

    \section1 Getting the Packages

    Packages are attached to each release on the
    \l{https://github.com/open-nt/Nt6/releases}{GitHub releases page}. Download
    the ones for your system and release. The file names carry the version, which
    is the one in the \c VERSION file of the source tree; the examples below use
    6.1.0, so replace it with the version you download.

    \table
    \header
        \li System
        \li Files
    \row
        \li Ubuntu 22.04
        \li \c{libnt6-dev_6.1.0-1~ubuntu22.04_amd64.deb}, \c{nt6-tools_6.1.0-1~ubuntu22.04_amd64.deb}
    \row
        \li Ubuntu 24.04
        \li \c{libnt6-dev_6.1.0-1~ubuntu24.04_amd64.deb}, \c{nt6-tools_6.1.0-1~ubuntu24.04_amd64.deb}
    \row
        \li Ubuntu 26.04
        \li \c{libnt6-dev_6.1.0-1~ubuntu26.04_amd64.deb}, \c{nt6-tools_6.1.0-1~ubuntu26.04_amd64.deb}
    \row
        \li Enterprise Linux 9
        \li \c{nt6-devel-6.1.0-1.el9.x86_64.rpm}, \c{nt6-tools-6.1.0-1.el9.x86_64.rpm}
    \row
        \li Enterprise Linux 10
        \li \c{nt6-devel-6.1.0-1.el10.x86_64.rpm}, \c{nt6-tools-6.1.0-1.el10.x86_64.rpm}
    \row
        \li Arch Linux
        \li \c{nt6-6.1.0-1-x86_64.pkg.tar.zst}
    \endtable

    With the \c gh command line tool, for example on Ubuntu 24.04:

    \code
    gh release download v6.1.0 --repo open-nt/Nt6 --pattern '*ubuntu24.04*'
    \endcode

    Or with \c curl:

    \code
    curl -LO https://github.com/open-nt/Nt6/releases/download/v6.1.0/libnt6-dev_6.1.0-1~ubuntu24.04_amd64.deb
    curl -LO https://github.com/open-nt/Nt6/releases/download/v6.1.0/nt6-tools_6.1.0-1~ubuntu24.04_amd64.deb
    \endcode

    Every build of the main branch also uploads its packages as artifacts of the
    workflow run, named \c packages-ubuntu-22.04, \c packages-ubuntu-24.04,
    \c packages-ubuntu-26.04, \c packages-el9, \c packages-el10 and
    \c packages-arch.

    \note Install the packages built for your system. A package built for another
    release of the same system is not tested there and is not supported.

    \section1 Ubuntu 22.04, 24.04 and 26.04

    Install the compiler and CMake, then the two packages. \c apt installs a local
    file when its name starts with \c{./} or a path:

    \code
    sudo apt update
    sudo apt install g++ cmake
    sudo apt install ./libnt6-dev_6.1.0-1~ubuntu24.04_amd64.deb ./nt6-tools_6.1.0-1~ubuntu24.04_amd64.deb
    \endcode

    Use the file names for your release. To remove them again:

    \code
    sudo apt remove libnt6-dev nt6-tools
    \endcode

    On Ubuntu 22.04 the packaged CMake is 3.22, which is new enough. The libraries
    are installed in \c{/usr/lib/x86_64-linux-gnu}.

    \section1 Enterprise Linux 9 and 10

    This covers RHEL, Rocky Linux and AlmaLinux. The packages are built on Rocky
    Linux and are binary compatible with the others. Install the compiler and
    CMake, then the two packages. \c dnf installs a local file when given its
    path, and resolves the dependencies from the repositories:

    \code
    sudo dnf install gcc-c++ cmake
    sudo dnf install ./nt6-devel-6.1.0-1.el9.x86_64.rpm ./nt6-tools-6.1.0-1.el9.x86_64.rpm
    \endcode

    On Enterprise Linux 10 use the \c el10 files. To remove them again:

    \code
    sudo dnf remove nt6-devel nt6-tools
    \endcode

    The libraries are installed in \c{/usr/lib64}.

    \section1 Arch Linux

    Install the compiler and CMake, then the package. \c{pacman -U} installs a
    local file:

    \code
    sudo pacman -S --needed gcc cmake
    sudo pacman -U nt6-6.1.0-1-x86_64.pkg.tar.zst
    \endcode

    To remove it again:

    \code
    sudo pacman -R nt6
    \endcode

    The package is built against the Arch Linux packages of the day, so install
    it on an up-to-date system (\c{sudo pacman -Syu}).

    \section1 Using Nt

    The headers are in \c{/usr/include/nt6}, away from the headers of a Qt
    installation, so the two do not clash. Find Nt from CMake:

    \code
    find_package(Nt6 6 REQUIRED)

    add_executable(app main.cpp counter.h)
    target_link_libraries(app PRIVATE Nt6::Network)   # Nt::Network works too
    nt_enable_automoc(app)                            # ntmoc, for Q_OBJECT classes
    \endcode

    The package provides \c{Nt6::Core}, \c{Nt6::Network}, \c{Nt6::SerialPort},
    the tools \c{Nt6::moc} and \c{Nt6::ndoc}, and the helper functions \c nt_enable_automoc(),
    \c nt_wrap_cpp() and \c nt_add_docs(). Check the installation with:

    \code
    ntmoc --version
    ndoc --version
    \endcode

    See \l{Nt Brand Guide} for the logo and the way to write Nt's name.

    \section1 Verifying a Download

    Each release has SHA-256 checksum files, \c SHA256SUMS-<system>.txt, next to
    the packages. Download the one for your system into the same directory and run:

    \code
    sha256sum --check --ignore-missing SHA256SUMS-*.txt
    \endcode

    Releases also carry SLSA level 3 build provenance, \c multiple.intoto.jsonl,
    which records that the packages were built from this repository by its GitHub
    workflows. Check a package with
    \l{https://github.com/slsa-framework/slsa-verifier}{slsa-verifier}:

    \code
    slsa-verifier verify-artifact libnt6-dev_6.1.0-1~ubuntu24.04_amd64.deb \
        --provenance-path multiple.intoto.jsonl \
        --source-uri github.com/open-nt/Nt6 --source-tag v6.1.0
    \endcode

    \section1 Building the Packages Yourself

    Build the packages for your own system, or for all the systems above in Docker
    containers, from a checkout of the source:

    \code
    packaging/build.sh                    # this machine; the packages are written to dist/
    packaging/build-packages.sh           # every system above, in containers
    packaging/build-packages.sh el9 arch  # some of them
    \endcode

    Or use CPack directly. It picks \c .deb, \c .rpm or a pacman package from
    \c{/etc/os-release}:

    \code
    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
    cpack --config build/CPackConfig.cmake
    \endcode

    Pass \c{-G DEB}, \c{-G RPM} or \c{-G External} to \c cpack to choose the
    format; \c External writes the pacman package. To install without a package,
    run \c{cmake --install build}.

    \sa {Nt Brand Guide}
*/
