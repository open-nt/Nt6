#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Builds the documentation site with one copy of the docs per release.
#
#   packaging/docs-site.sh --ndoc NDOC --output DIR [--url URL] [TAG...]
#
# Writes the docs of the working tree (strict, as the docs target does) to
# DIR/dev, the docs of each TAG to DIR/<version>, where <version> is the tag's
# VERSION file, and the newest TAG (the first one given) again to DIR itself, so
# links to the root keep working and show the latest release. With no TAG, DIR
# redirects to dev/. DIR/versions.json lists the versions for the switcher in
# each page header, and DIR/badges holds the README badge of the working tree.
#
# Each tag is built from a temporary git worktree with the given NDOC, so old
# releases get the current look; a tag whose docs fail to build is skipped with
# a warning. URL is where DIR is published (default: the url in doc/ndoc.conf);
# it gives every page its absolute address and the location of versions.json.
# docs.yml publishes the result to GitHub Pages, next to the apt repository.
set -euo pipefail

ndoc=
output=
url=
tags=()

while [ $# -gt 0 ]; do
    case "$1" in
    --ndoc) ndoc=$2; shift 2 ;;
    --output) output=$2; shift 2 ;;
    --url) url=$2; shift 2 ;;
    -h | --help) sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "docs-site.sh: unknown option $1" >&2; exit 2 ;;
    *) tags+=("$1"); shift ;;
    esac
done
if [ -z "$ndoc" ] || [ -z "$output" ]; then
    echo "docs-site.sh: --ndoc and --output are required" >&2
    exit 2
fi

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ndoc=$(realpath "$ndoc")
mkdir -p "$output"
output=$(realpath "$output")
if [ -z "$url" ]; then
    url=$(sed -n 's/^url *= *//p' "$root/doc/ndoc.conf")
fi
url=${url%/}/

work=$(mktemp -d)
cleanup() {
    for tree in "$work"/tree-*; do
        [ -d "$tree" ] && git -C "$root" worktree remove --force "$tree"
    done
    rm -rf "$work"
}
trap cleanup EXIT

# json_string TEXT: TEXT as a JSON string; versions and tags need no escaping beyond quotes.
json_string() { printf '"%s"' "${1//\"/\\\"}"; }

# Releases made before ndoc had a favicon key get the icons too; every release has them in assets/.
favicon='favicon=../assets/nt-logo.svg ../assets/nt-logo.png'

entries=$(printf '{"name": "dev", "path": "dev/"}')
latest=

rm -rf "$output/dev"
"$ndoc" --strict --require-complete -o "$output/dev" \
    --doc-version dev --set "url=${url}dev/" --set "versions=${url}versions.json" \
    "$root/doc/ndoc.conf"

for tag in "${tags[@]}"; do
    tree=$work/tree-${tag//\//-}
    git -C "$root" worktree add --quiet --detach "$tree" "$tag"
    version=$(head -n 1 "$tree/VERSION" | tr -d '[:space:]')
    case "$version" in
    '' | dev | apt | badges | */* | .*)
        echo "::warning::docs-site.sh: skipping $tag, whose VERSION '$version' is not a usable directory name"
        continue
        ;;
    esac
    rm -rf "${output:?}/$version"
    if ! "$ndoc" -o "$output/$version" --doc-version "$version" --set "url=${url}${version}/" --set "versions=${url}versions.json" \
        --set "$favicon" "$tree/doc/ndoc.conf"; then
        echo "::warning::docs-site.sh: the docs of $tag failed to build; leaving them out"
        rm -rf "${output:?}/$version"
        continue
    fi
    entries+=", {\"name\": $(json_string "$version"), \"path\": $(json_string "$version/")}"
    if [ -z "$latest" ]; then
        latest=$version
        "$ndoc" -o "$work/root" --doc-version "$version" --set "url=$url" --set "versions=${url}versions.json" \
            --set "$favicon" "$tree/doc/ndoc.conf"
        cp -R "$work/root/." "$output/"
    fi
done

if [ -z "$latest" ]; then
    cat > "$output/index.html" <<'EOF'
<!doctype html>
<meta charset="utf-8">
<meta http-equiv="refresh" content="0; url=dev/">
<link rel="canonical" href="dev/">
<a href="dev/">Documentation</a>
EOF
fi

printf '{"latest": %s, "versions": [%s]}\n' "$(json_string "${latest:-dev}")" "$entries" > "$output/versions.json"

cmake -DVERSION_FILE="$root/VERSION" -DOUTPUT_DIR="$output/badges" -P "$root/cmake/NtBadge.cmake"
echo "docs-site.sh: wrote ${output} (latest: ${latest:-none})"
