#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Renames the .deb files in DIR to the SemVer spelling used by release assets:
# libnt6-dev_6.0.0~rc.4-1~ubuntu24.04_amd64.deb -> libnt6-dev_6.0.0-rc.4-1~ubuntu24.04_amd64.deb
# The Version field inside the package keeps the tilde, which dpkg needs to sort
# a pre-release below the final release. The SHA256SUMS-*.txt files are updated.
#
#   packaging/semver-assets.sh DIR
set -euo pipefail

dir=${1:?usage: semver-assets.sh DIR}

shopt -s nullglob
for deb in "$dir"/*.deb; do
    old=$(basename "$deb")
    [[ $old =~ ^([^_]+_[0-9]+\.[0-9]+\.[0-9]+)~(.*)$ ]] || continue
    new=${BASH_REMATCH[1]}-${BASH_REMATCH[2]}
    mv "$deb" "$dir/$new"
    for sums in "$dir"/SHA256SUMS-*.txt; do
        content=$(<"$sums")
        printf '%s\n' "${content//"$old"/"$new"}" > "$sums"
    done
    echo "semver-assets.sh: $old -> $new"
done
