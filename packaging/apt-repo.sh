#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Builds a signed apt repository from the Ubuntu packages made by build.sh.
#
#   packaging/apt-repo.sh --output DIR [--key KEY] DEB...
#
# Each package goes to the suite of the Ubuntu release it was built for, which
# comes from the ~ubuntu<release> suffix of its version (jammy, noble, resolute).
# A suite has two components: main holds final releases only, testing holds
# pre-releases such as 6.1.0~rc1 as well. Signs with KEY (a gpg key ID, default:
# gpg's default key); set NT_APT_GPG_PASSPHRASE if the key has a passphrase.
# Writes DIR/dists, DIR/pool and the public key as DIR/nt6.gpg and DIR/nt6.asc.
#
# Needs only dpkg-deb, gzip, sha256sum and gpg. docs.yml publishes the result
# to GitHub Pages under /apt; see doc/install.h for the sources line.
set -euo pipefail

output=
key=
debs=()

while [ $# -gt 0 ]; do
    case "$1" in
    --output) output=$2; shift 2 ;;
    --key) key=$2; shift 2 ;;
    -h | --help) sed -n '2,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "apt-repo.sh: unknown option $1" >&2; exit 2 ;;
    *) debs+=("$1"); shift ;;
    esac
done
[ -n "$output" ] || { echo "apt-repo.sh: --output is required" >&2; exit 2; }
[ ${#debs[@]} -gt 0 ] || { echo "apt-repo.sh: no packages given" >&2; exit 2; }

codename_for() {
    case "$1" in
    22.04) echo jammy ;;
    24.04) echo noble ;;
    26.04) echo resolute ;;
    *) return 1 ;;
    esac
}

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
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Copy each package into pool/<suite>/ and record "<suite> <component> <arch> <pool path>".
declare -A seen=()
for deb in "${debs[@]}"; do
    package=$(dpkg-deb -f "$deb" Package)
    version=$(dpkg-deb -f "$deb" Version)
    arch=$(dpkg-deb -f "$deb" Architecture)
    if [[ ! $version =~ ~ubuntu([0-9]+\.[0-9]+)$ ]] || ! suite=$(codename_for "${BASH_REMATCH[1]}"); then
        echo "apt-repo.sh: $deb: no known Ubuntu release in version $version" >&2
        exit 1
    fi
    file=${package}_${version}_${arch}.deb
    if [ -n "${seen[$suite/$file]:-}" ]; then
        echo "apt-repo.sh: skipping $deb, $file is already in $suite" >&2
        continue
    fi
    seen[$suite/$file]=1
    mkdir -p "$output/pool/$suite"
    cp "$deb" "$output/pool/$suite/$file"
    upstream=${version%-*}
    printf '%s testing %s pool/%s/%s\n' "$suite" "$arch" "$suite" "$file" >> "$work/index"
    if [[ $upstream != *"~"* ]]; then
        printf '%s main %s pool/%s/%s\n' "$suite" "$arch" "$suite" "$file" >> "$work/index"
    fi
done

stanza() { # stanza <pool path>: the Packages entry of one package
    local path=$output/$1
    # dpkg-deb ends the fields with a blank line, which would split the entry.
    dpkg-deb -f "$path" | sed '/^$/d'
    printf 'Filename: %s\nSize: %s\nMD5sum: %s\nSHA256: %s\n\n' "$1" "$(stat -c %s "$path")" \
        "$(md5sum "$path" | cut -d' ' -f1)" "$(sha256sum "$path" | cut -d' ' -f1)"
}

checksums() { # checksums <command> <files...>: one Release checksum section
    local sum file
    for file in "${@:2}"; do
        sum=$("$1" "$file" | cut -d' ' -f1)
        printf ' %s %16s %s\n' "$sum" "$(stat -c %s "$file")" "$file"
    done
}

date=$(LC_ALL=C date -u -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}" '+%a, %d %b %Y %H:%M:%S UTC')
for suite in $(cut -d' ' -f1 "$work/index" | sort -u); do
    dist=$output/dists/$suite
    archs=$(awk -v s="$suite" '$1 == s { print $3 }' "$work/index" | sort -u | tr '\n' ' ')
    archs=${archs% }
    for component in main testing; do
        for arch in $archs; do
            dir=$dist/$component/binary-$arch
            mkdir -p "$dir"
            # An empty index is still written, so that apt finds every file Release lists.
            : > "$dir/Packages"
            awk -v s="$suite" -v c="$component" -v a="$arch" '$1 == s && $2 == c && $3 == a { print $4 }' \
                "$work/index" | sort | while read -r path; do stanza "$path"; done >> "$dir/Packages"
            gzip -9n < "$dir/Packages" > "$dir/Packages.gz"
        done
    done
    (
        cd "$dist"
        mapfile -t files < <(find main testing -type f | sort)
        {
            printf 'Origin: Nt\nLabel: Nt\nSuite: %s\nCodename: %s\nDate: %s\n' "$suite" "$suite" "$date"
            printf 'Architectures: %s\nComponents: main testing\n' "$archs"
            printf 'Description: Nt, a clean-room implementation of the Qt 6 API\n'
            printf 'MD5Sum:\n'
            checksums md5sum "${files[@]}"
            printf 'SHA256:\n'
            checksums sha256sum "${files[@]}"
        } > Release
    )
    sign --clearsign --output "$dist/InRelease" "$dist/Release"
    sign --detach-sign --armor --output "$dist/Release.gpg" "$dist/Release"
done

# The public key, in both forms: nt6.gpg for /etc/apt/keyrings, nt6.asc to read.
gpg --batch --yes --export --output "$output/nt6.gpg" ${key:+"$key"}
gpg --batch --yes --export --armor --output "$output/nt6.asc" ${key:+"$key"}
[ -s "$output/nt6.gpg" ] || { echo "apt-repo.sh: gpg exported no public key" >&2; exit 1; }
echo "==> apt repository in $output: $(cut -d' ' -f1 "$work/index" | sort -u | tr '\n' ' ')"
