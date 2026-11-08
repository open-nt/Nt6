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
        ca-certificates cmake dpkg-dev file g++ libfontconfig-dev libfreetype-dev libssl-dev libxcb1-dev make pkg-config tar zstd
    ;;
*" rhel "* | *" fedora "* | *" centos "*)
    dnf -y install cmake gcc-c++ fontconfig-devel freetype-devel libxcb-devel make openssl-devel pkgconf-pkg-config rpm-build tar zstd findutils
    ;;
*" arch "*)
    pacman -Syu --noconfirm --needed cmake fontconfig freetype2 gcc libxcb make openssl pkgconf tar zstd
    ;;
*)
    echo "install-deps.sh: unsupported distribution: ${PRETTY_NAME:-$ID}" >&2
    exit 1
    ;;
esac
