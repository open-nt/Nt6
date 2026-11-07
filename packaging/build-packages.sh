#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Builds the packages for each supported distribution in a container, the same
# way CI does. Needs Docker (or set CONTAINER_ENGINE=podman).
#
#   packaging/build-packages.sh [--output DIR] [--native] [target...]
#
# Targets: ubuntu-22.04 ubuntu-24.04 ubuntu-26.04 el9 el10 arch (default: all).
# --native builds only for this machine, without a container: packaging/build.sh.
# Packages end up in DIR/<target>/ (default: dist/).
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
output=$source_dir/dist
engine=${CONTAINER_ENGINE:-docker}
targets=()

declare -A images=(
    [ubuntu-22.04]=ubuntu:22.04
    [ubuntu-24.04]=ubuntu:24.04
    [ubuntu-26.04]=ubuntu:26.04
    [el9]=quay.io/rockylinux/rockylinux:9
    [el10]=quay.io/rockylinux/rockylinux:10
    [arch]=archlinux:latest
)
all_targets=(ubuntu-22.04 ubuntu-24.04 ubuntu-26.04 el9 el10 arch)

while [ $# -gt 0 ]; do
    case "$1" in
    --output) output=$2; shift 2 ;;
    --native) exec "$source_dir/packaging/build.sh" --output "$output" ;;
    -h | --help) sed -n '2,11p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "build-packages.sh: unknown option $1" >&2; exit 2 ;;
    *)
        [ -n "${images[$1]:-}" ] || { echo "build-packages.sh: unknown target $1" >&2; exit 2; }
        targets+=("$1"); shift ;;
    esac
done
[ ${#targets[@]} -gt 0 ] || targets=("${all_targets[@]}")

mkdir -p "$output"
failed=()
for target in "${targets[@]}"; do
    echo "==> $target (${images[$target]})"
    mkdir -p "$output/$target"
    if ! "$engine" run --rm \
        -v "$source_dir":/src:ro -v "$(cd "$output/$target" && pwd)":/out \
        -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
        "${images[$target]}" bash -ec '
            mkdir /work
            tar -C /src --exclude=./build --exclude="./build-*" --exclude=./dist --exclude=./.git -cf - . | tar -C /work -xf -
            cd /work
            packaging/install-deps.sh
            status=0
            packaging/build.sh --output /out --verify-install || status=$?
            chown -R "$HOST_UID:$HOST_GID" /out
            exit $status'; then
        failed+=("$target")
    fi
done

if [ ${#failed[@]} -gt 0 ]; then
    echo "build-packages.sh: failed: ${failed[*]}" >&2
    exit 1
fi
echo "==> done; packages are in $output"
