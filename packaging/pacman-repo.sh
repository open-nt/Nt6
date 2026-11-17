#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Builds a signed pacman repository, named nt6, from the Arch Linux packages made by
# build.sh.
#
#   packaging/pacman-repo.sh --output DIR [--key KEY] [--releases-only] PKG...
#
# Each package goes to DIR/<arch>/ after the arch field of its .PKGINFO, next to a
# detached signature (<package>.sig, so the package itself stays as released), and
# repo-add writes the database nt6.db and the file list nt6.files, both signed. Signs
# with KEY (a gpg key ID, default: gpg's default key); set NT_APT_GPG_PASSPHRASE if the
# key has a passphrase. With --releases-only, pre-releases (an _ in pkgver, such as
# 6.1.0_rc1) are left out. The database names are real files, not the symlinks repo-add
# makes, since object stores such as R2 have no symlinks.
#
# Needs bsdtar, repo-add (both part of pacman) and gpg. r2-repos.yml publishes the
# result to Cloudflare R2.
set -euo pipefail

output=
key=
releases_only=0
pkgs=()

while [ $# -gt 0 ]; do
    case "$1" in
    --output) output=$2; shift 2 ;;
    --key) key=$2; shift 2 ;;
    --releases-only) releases_only=1; shift ;;
    -h | --help) sed -n '2,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "pacman-repo.sh: unknown option $1" >&2; exit 2 ;;
    *) pkgs+=("$1"); shift ;;
    esac
done
[ -n "$output" ] || { echo "pacman-repo.sh: --output is required" >&2; exit 2; }
[ ${#pkgs[@]} -gt 0 ] || { echo "pacman-repo.sh: no packages given" >&2; exit 2; }

gpg_sign=(gpg --batch --yes)
[ -z "$key" ] || gpg_sign+=(--local-user "$key")
if [ -n "${NT_APT_GPG_PASSPHRASE:-}" ]; then
    gpg_sign+=(--pinentry-mode loopback --passphrase-fd 0)
fi
sign() { # sign <gpg options...>: the passphrase, if any, goes on stdin
    printf '%s' "${NT_APT_GPG_PASSPHRASE:-}" | "${gpg_sign[@]}" "$@"
}

rm -rf "$output"
mkdir -p "$output"
output=$(cd "$output" && pwd)

field() { # field <name> <package>: one field of the package's .PKGINFO
    bsdtar -xOf "$2" .PKGINFO | sed -n "s/^$1 = //p" | head -n1
}

declare -A archs=()
for pkg in "${pkgs[@]}"; do
    name=$(field pkgname "$pkg")
    version=$(field pkgver "$pkg")
    arch=$(field arch "$pkg")
    if [ -z "$name" ] || [ -z "$version" ] || [ -z "$arch" ]; then
        echo "pacman-repo.sh: $pkg: no pkgname, pkgver or arch in .PKGINFO" >&2
        exit 1
    fi
    upstream=${version%-*}
    if [ "$releases_only" = 1 ] && [[ $upstream == *_* ]]; then
        echo "pacman-repo.sh: skipping $pkg, $version is a pre-release" >&2
        continue
    fi
    file=$name-$version-$arch.pkg.tar.zst
    if [ -e "$output/$arch/$file" ]; then
        echo "pacman-repo.sh: skipping $pkg, $file is already in $arch" >&2
        continue
    fi
    mkdir -p "$output/$arch"
    cp "$pkg" "$output/$arch/$file"
    sign --detach-sign --no-armor --output "$output/$arch/$file.sig" "$output/$arch/$file"
    archs[$arch]=1
done
[ ${#archs[@]} -gt 0 ] || { echo "pacman-repo.sh: no packages left to publish" >&2; exit 1; }

for arch in $(printf '%s\n' "${!archs[@]}" | sort); do
    (
        cd "$output/$arch"
        # Oldest first, so that repo-add keeps the newest version of each package.
        mapfile -t files < <(ls -1v -- *.pkg.tar.zst)
        repo-add --quiet nt6.db.tar.gz "${files[@]}"
        for db in nt6.db nt6.files; do
            rm -f "$db" "$db.tar.gz.old"
            cp "$db.tar.gz" "$db"
            sign --detach-sign --no-armor --output "$db.sig" "$db"
            cp "$db.sig" "$db.tar.gz.sig"
        done
    )
done

gpg --batch --yes --export --armor --output "$output/nt6.asc" ${key:+"$key"}
[ -s "$output/nt6.asc" ] || { echo "pacman-repo.sh: gpg exported no public key" >&2; exit 1; }
echo "==> pacman repository in $output: $(printf '%s\n' "${!archs[@]}" | sort | tr '\n' ' ')"
