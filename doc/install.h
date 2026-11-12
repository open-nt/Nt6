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
    \c ntmoc and \c ntrcc, replacements for Qt's moc and rcc, and \c ndoc,
    the documentation generator. Install both to build a Qt-style project.

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
    \c packages-ubuntu-26.04, \c packages-el9, \c packages-el10,
    \c packages-arch, \c packages-snap and \c packages-snap-arm64.

    \note Install the packages built for your system. A package built for another
    release of the same system is not tested there and is not supported.

    \section1 Ubuntu 22.04, 24.04 and 26.04

    \section2 From the apt repository

    The Ubuntu packages of every release are also in a signed apt repository,
    so that \c{apt upgrade} picks up new releases. Add its key and the sources
    for your release (the suite is your release's code name: \c jammy,
    \c noble or \c resolute), then install:

    \code
    sudo install -d -m 0755 /etc/apt/keyrings
    sudo curl -fsSLo /etc/apt/keyrings/nt6.gpg https://open-nt.github.io/Nt6/apt/nt6.gpg
    sudo tee /etc/apt/sources.list.d/nt6.sources > /dev/null <<EOF
    Types: deb
    URIs: https://open-nt.github.io/Nt6/apt
    Suites: $(. /etc/os-release && echo "$VERSION_CODENAME")
    Components: main
    Signed-By: /etc/apt/keyrings/nt6.gpg
    EOF
    sudo apt update
    sudo apt install g++ cmake libnt6-dev nt6-tools
    \endcode

    The \c main component has final releases only. For pre-releases such as
    \c 6.1.0-rc1 as well, use \c{Components: main testing}. Until the first
    final release, \c testing is the only component with packages.

    \section2 From the downloaded packages

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

    \section1 Snap

    The snap carries the tools and the development files in one package. It is
    built for \c amd64 and \c arm64 (including NVIDIA Jetson) on \c core24 and is
    published to the Snap Store as \l{https://snapcraft.io/open-nt6}{open-nt6}.
    snapd picks the build for your architecture:

    \code
    sudo snap install open-nt6
    \endcode

    Releases of a pre-release version (\c 6.0.0-rc2, for example) are on the \c candidate
    channel until a final release reaches \c stable; until then install with
    \c{sudo snap install open-nt6 --channel=candidate}.

    To install the snap of a release instead, download \c{open-nt6_<version>_amd64.snap}
    or \c{open-nt6_<version>_arm64.snap} from the releases page and run
    \c{sudo snap install --dangerous open-nt6_<version>_<arch>.snap}. Both are
    covered by the release provenance described at the end of this page.

    The commands are \c open-nt6.ntmoc, \c open-nt6.ntrcc and \c open-nt6.ndoc, and
    \c ntmoc, \c ntrcc and \c ndoc once the Snap Store has granted the aliases, or after
    \c{sudo snap alias open-nt6.ntmoc ntmoc}, \c{sudo snap alias open-nt6.ntrcc ntrcc} and
    \c{sudo snap alias open-nt6.ndoc ndoc}.

    The snap is strictly confined: the tools read and write files in your home
    directory, not elsewhere. The headers, static libraries and CMake package are
    in \c{/snap/open-nt6/current/usr}; to build against them, pass
    \c{-DCMAKE_PREFIX_PATH=/snap/open-nt6/current/usr} to CMake. Remove the snap with
    \c{sudo snap remove open-nt6}.

    To build the snap yourself, configure Nt and build the \c snap target:

    \code
    cmake -S . -B build
    cmake --build build --target snap
    \endcode

    This runs \c snapcraft in LXD or Multipass and writes
    \c{open-nt6_<version>_<arch>.snap} to the build directory. CI builds each
    architecture natively, the arm64 snap on a hosted arm64 runner. On a machine
    running Ubuntu 24.04 that has no container runtime, such as a Jetson, pass
    \c{-DNT_SNAPCRAFT_ARGS=--destructive-mode}.

    \section1 Using Nt

    The headers are in \c{/usr/include/nt6}, away from the headers of a Qt
    installation, so the two do not clash. Find Nt from CMake:

    \code
    find_package(Nt6 6 REQUIRED)

    add_executable(app main.cpp counter.h)
    target_link_libraries(app PRIVATE Nt6::Network)   # Nt::Network works too
    nt_enable_automoc(app)                            # ntmoc, for Q_OBJECT classes
    nt_enable_autorcc(app)                            # ntrcc, for .qrc files
    \endcode

    The package provides \c{Nt6::Core}, \c{Nt6::Network}, \c{Nt6::SerialPort}, \c{Nt6::Sql}, \c{Nt6::DBus}, \c{Nt6::HttpServer},
    \c{Nt6::WebSockets}, \c{Nt6::Gui}, \c{Nt6::Widgets}, \c{Nt6::Svg}, \c{Nt6::SvgWidgets}, \c{Nt6::Charts}, \c{Nt6::Qml}, \c{Nt6::Concurrent}, \c{Nt6::Test},
    the tools \c{Nt6::moc}, \c{Nt6::rcc} and \c{Nt6::ndoc}, and the helper functions
    \c nt_enable_automoc(), \c nt_wrap_cpp(), \c nt_enable_autorcc(), \c nt_add_resources()
    (see \l{The Nt Resource System}) and \c nt_add_docs(). Check the installation with:

    \code
    ntmoc --version
    ntrcc --version
    ndoc --version
    \endcode

    The libraries are static, so a program that links \c{Nt6::Network} also needs the OpenSSL
    development package, one that links \c{Nt6::Sql} that of SQLite (\c{libsqlite3-dev}), and one that links \c{Nt6::Gui} or \c{Nt6::Widgets} needs those of
    FreeType, fontconfig, libjpeg (optional, for JPEG images) and XCB (\c{libssl-dev libfreetype-dev libfontconfig-dev libjpeg-dev libxcb1-dev} on
    Ubuntu). The \c{examples/hello} and \c{examples/widgets} directories are standalone projects to
    start from.

    Coming from Qt, read \l{The Nt Resource System} for how \c{.qrc} files are compiled with \c ntrcc, and
    \l{Using Nt Widgets} for user interfaces.

    For a device without an operating system of this kind, Nt Core and Nt Network cross build
    as a module of the Zephyr RTOS; see \l{Nt on Zephyr}.

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
    run \c{cmake --install build}. Remove those files again with
    \c{cmake --build build --target uninstall} (\c{make uninstall}), which uses the
    \c install_manifest.txt written by the install.

    \sa {Nt Brand Guide}
*/
