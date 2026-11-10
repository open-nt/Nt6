#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Builds Nt, runs its tests, makes the native package with CPack, and checks the
# package: the files are staged with a DESTDIR install and a small project is
# built against them. With --verify-install (needs root) the packages are
# installed with the system package manager and the same project is built
# against the real installation.
#
#   packaging/build.sh [--output DIR] [--build-dir DIR] [--verify-install] [--skip-tests]
#
# The format follows /etc/os-release: .deb on Debian and Ubuntu, .rpm on Enterprise
# Linux and Fedora, .pkg.tar.zst on Arch Linux.
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
output=$source_dir/dist
build_dir=$source_dir/build-package
verify_install=0
run_tests=1

while [ $# -gt 0 ]; do
    case "$1" in
    --output) output=$2; shift 2 ;;
    --build-dir) build_dir=$2; shift 2 ;;
    --verify-install) verify_install=1; shift ;;
    --skip-tests) run_tests=0; shift ;;
    -h | --help) sed -n '2,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "build.sh: unknown option $1" >&2; exit 2 ;;
    esac
done

. /etc/os-release
like=" ${ID} ${ID_LIKE:-} "
case "$like" in
*" debian "* | *" ubuntu "*) generator=DEB; pattern='*.deb' ;;
*" rhel "* | *" fedora "* | *" centos "*) generator=RPM; pattern='*.rpm' ;;
*" arch "*) generator=External; pattern='*.pkg.tar.zst' ;;
*) echo "build.sh: unsupported distribution: ${PRETTY_NAME:-$ID}" >&2; exit 1 ;;
esac
echo "==> ${PRETTY_NAME:-$ID}: building $generator packages"

output=$(realpath -m "$output")
build_dir=$(realpath -m "$build_dir")

rm -rf "$build_dir"
cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DNT_BUILD_EXAMPLES=OFF
cmake --build "$build_dir" -j"$(nproc)"
if [ "$run_tests" = 1 ]; then
    ctest --test-dir "$build_dir" --output-on-failure
fi

(cd "$build_dir" && cpack --config CPackConfig.cmake -G "$generator")

mkdir -p "$output"
rm -f "$output"/$pattern
find "$build_dir" -maxdepth 1 -name "$pattern" -exec cp {} "$output"/ \;
packages=$(find "$output" -maxdepth 1 -name "$pattern" | sort)
if [ -z "$packages" ]; then
    echo "build.sh: CPack produced no $pattern" >&2
    exit 1
fi

# Staged check: no root needed.
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
DESTDIR="$stage" cmake --install "$build_dir" > /dev/null
consumer() {
    local prefix_args=("$@")
    cmake -S "$source_dir/packaging/consumer" -B "$stage/consumer" "${prefix_args[@]}" > /dev/null
    cmake --build "$stage/consumer"
    "$stage/consumer/consumer"
    rm -rf "$stage/consumer"
}
echo "==> checking the staged installation"
consumer -DCMAKE_PREFIX_PATH="$stage/usr"

if [ "$verify_install" = 1 ]; then
    if [ "$(id -u)" != 0 ]; then
        echo "build.sh: --verify-install needs root" >&2
        exit 1
    fi
    echo "==> installing the packages"
    case "$generator" in
    DEB) apt-get install -y $packages ;;
    RPM) dnf -y install $packages ;;
    External) pacman -U --noconfirm $packages ;;
    esac
    consumer
    ntmoc --version
    ntrcc --version
    ndoc --version
fi

(cd "$output" && printf '%s\n' "$packages" | xargs -r -n1 basename | xargs -r sha256sum > "SHA256SUMS-${ID}-${VERSION_ID}.txt")
echo "==> packages in $output:"
ls -l $packages
