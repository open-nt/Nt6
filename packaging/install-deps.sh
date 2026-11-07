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
        ca-certificates cmake dpkg-dev file g++ libssl-dev make tar zstd
    ;;
*" rhel "* | *" fedora "* | *" centos "*)
    dnf -y install cmake gcc-c++ make openssl-devel rpm-build tar zstd findutils
    ;;
*" arch "*)
    pacman -Syu --noconfirm --needed cmake gcc make openssl tar zstd
    ;;
*)
    echo "install-deps.sh: unsupported distribution: ${PRETTY_NAME:-$ID}" >&2
    exit 1
    ;;
esac
