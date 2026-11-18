#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Installs what is needed to build, test and package Nt on this distribution.
# Needs root. Used by CI and by build-packages.sh.
set -euo pipefail

. /etc/os-release
like=" ${ID} ${ID_LIKE:-} "

case "$like" in
*" debian "* | *" ubuntu "*)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        ca-certificates cmake dpkg-dev file g++ libfontconfig-dev libfreetype-dev libjpeg-dev libsqlite3-dev libssl-dev libvulkan-dev libxcb1-dev libzstd-dev make mesa-vulkan-drivers pkg-config tar zlib1g-dev zstd
    ;;
*" rhel "* | *" fedora "* | *" centos "*)
    dnf -y install cmake gcc-c++ fontconfig-devel freetype-devel libjpeg-turbo-devel libxcb-devel libzstd-devel make openssl-devel pkgconf-pkg-config rpm-build sqlite-devel tar vulkan-headers zlib-devel zstd findutils
    # The software Vulkan driver only lets the Vulkan tests run; they skip without it. Enterprise Linux 9 has no
    # aarch64 build of it.
    dnf -y install mesa-vulkan-drivers || echo "install-deps.sh: mesa-vulkan-drivers is not available; the Vulkan tests will skip"
    ;;
*" arch "*)
    # Arch's fontconfig, unlike Debian's and Fedora's, brings no font; the text tests need one.
    pacman -Syu --noconfirm --needed cmake fontconfig freetype2 gcc libjpeg-turbo libxcb make openssl pkgconf sqlite tar ttf-dejavu vulkan-headers vulkan-swrast zlib zstd
    ;;
*)
    echo "install-deps.sh: unsupported distribution: ${PRETTY_NAME:-$ID}" >&2
    exit 1
    ;;
esac
