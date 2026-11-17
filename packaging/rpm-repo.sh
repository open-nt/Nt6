#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Builds a signed dnf/yum repository from the Enterprise Linux packages made by build.sh.
#
#   packaging/rpm-repo.sh --output DIR [--key KEY] [--url URL] [--releases-only] RPM...
#
# Each package goes to DIR/el<N>/<arch>/ after the .el<N> dist tag of its release
# and its architecture (el9/x86_64, el10/aarch64, ...), and createrepo_c writes the
# repodata/ of each. The metadata is signed (repodata/repomd.xml.asc) with KEY (a gpg
# key ID, default: gpg's default key); set NT_APT_GPG_PASSPHRASE if the key has a
# passphrase. The packages themselves stay as released, so that they match their SLSA
# provenance; the signed metadata carries the SHA-256 of each. With --releases-only,
# pre-releases (a ~ in the version, such as 6.1.0~rc1) are left out. With --url, the
# repository's public URL, DIR/nt6.repo is written for /etc/yum.repos.d.
#
# Needs rpm, createrepo_c and gpg. r2-repos.yml publishes the result to Cloudflare R2.
set -euo pipefail

output=
key=
url=
releases_only=0
rpms=()

while [ $# -gt 0 ]; do
    case "$1" in
    --output) output=$2; shift 2 ;;
    --key) key=$2; shift 2 ;;
    --url) url=${2%/}; shift 2 ;;
    --releases-only) releases_only=1; shift ;;
    -h | --help) sed -n '2,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "rpm-repo.sh: unknown option $1" >&2; exit 2 ;;
    *) rpms+=("$1"); shift ;;
    esac
done
[ -n "$output" ] || { echo "rpm-repo.sh: --output is required" >&2; exit 2; }
[ ${#rpms[@]} -gt 0 ] || { echo "rpm-repo.sh: no packages given" >&2; exit 2; }

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

declare -A repos=()
for rpm in "${rpms[@]}"; do
    read -r name version release arch < <(rpm -qp --nosignature \
        --queryformat '%{NAME} %{VERSION} %{RELEASE} %{ARCH}\n' "$rpm")
    if [[ ! $release =~ \.(el[0-9]+)$ ]]; then
        echo "rpm-repo.sh: $rpm: no .el<N> dist tag in release $release" >&2
        exit 1
    fi
    dist=${BASH_REMATCH[1]}
    if [ "$releases_only" = 1 ] && [[ $version == *"~"* ]]; then
        echo "rpm-repo.sh: skipping $rpm, $version is a pre-release" >&2
        continue
    fi
    file=$name-$version-$release.$arch.rpm
    if [ -e "$output/$dist/$arch/$file" ]; then
        echo "rpm-repo.sh: skipping $rpm, $file is already in $dist/$arch" >&2
        continue
    fi
    mkdir -p "$output/$dist/$arch"
    cp "$rpm" "$output/$dist/$arch/$file"
    repos[$dist/$arch]=1
done
[ ${#repos[@]} -gt 0 ] || { echo "rpm-repo.sh: no packages left to publish" >&2; exit 1; }

for repo in $(printf '%s\n' "${!repos[@]}" | sort); do
    # --revision follows SOURCE_DATE_EPOCH, as apt-repo.sh's Release date does.
    createrepo_c --quiet --revision "${SOURCE_DATE_EPOCH:-$(date +%s)}" "$output/$repo"
    sign --detach-sign --armor --output "$output/$repo/repodata/repomd.xml.asc" \
        "$output/$repo/repodata/repomd.xml"
done

gpg --batch --yes --export --armor --output "$output/nt6.asc" ${key:+"$key"}
[ -s "$output/nt6.asc" ] || { echo "rpm-repo.sh: gpg exported no public key" >&2; exit 1; }
if [ -n "$url" ]; then
    # dnf sets $releasever to the major version (9, 10) on Enterprise Linux.
    cat > "$output/nt6.repo" <<EOF
[nt6]
name=Nt
baseurl=$url/rpm/el\$releasever/\$basearch/
enabled=1
repo_gpgcheck=1
gpgcheck=0
gpgkey=$url/nt6.asc
EOF
fi
echo "==> rpm repository in $output: $(printf '%s\n' "${!repos[@]}" | sort | tr '\n' ' ')"
