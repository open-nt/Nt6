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
        ca-certificates cmake dpkg-dev file g++ libfontconfig-dev libfreetype-dev libjpeg-dev libsqlite3-dev libssl-dev libxcb1-dev libzstd-dev make pkg-config tar zlib1g-dev zstd
    ;;
*" rhel "* | *" fedora "* | *" centos "*)
    dnf -y install cmake gcc-c++ fontconfig-devel freetype-devel libjpeg-turbo-devel libxcb-devel libzstd-devel make openssl-devel pkgconf-pkg-config rpm-build sqlite-devel tar zlib-devel zstd findutils
    ;;
*" arch "*)
    pacman -Syu --noconfirm --needed cmake fontconfig freetype2 gcc libjpeg-turbo libxcb make openssl pkgconf sqlite tar zlib zstd
    ;;
*)
    echo "install-deps.sh: unsupported distribution: ${PRETTY_NAME:-$ID}" >&2
    exit 1
    ;;
esac
