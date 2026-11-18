#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Writes an index.html into every directory of the package repository tree that
# r2-repos.yml publishes, so that it can be browsed: each page lists the folders and
# files below it with their sizes, and the root page also shows how to add the apt, dnf
# and pacman repositories. A Cloudflare URL rewrite serves <dir>/index.html for <dir>/,
# since R2 has no directory listings (see the header of r2-repos.yml).
#
#   packaging/repo-index.sh --url URL DIR
set -euo pipefail

url=
dir=

while [ $# -gt 0 ]; do
    case "$1" in
    --url) url=${2%/}; shift 2 ;;
    -h | --help) sed -n '2,10p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "repo-index.sh: unknown option $1" >&2; exit 2 ;;
    *) dir=$1; shift ;;
    esac
done
[ -n "$url" ] || { echo "repo-index.sh: --url is required" >&2; exit 2; }
[ -d "$dir" ] || { echo "repo-index.sh: no directory given" >&2; exit 2; }
dir=$(cd "$dir" && pwd)

esc() { # esc <text>: the text with the HTML special characters escaped
    local s=${1//&/&amp;}
    s=${s//</&lt;}
    s=${s//>/&gt;}
    printf '%s' "${s//\"/&quot;}"
}

human() { # human <bytes>: the size in B, KiB or MiB
    awk -v b="$1" 'BEGIN {
        if (b < 1024) printf "%d B", b
        else if (b < 1048576) printf "%.1f KiB", b / 1024
        else printf "%.1f MiB", b / 1048576 }'
}

head_html() { # head_html <title>
    cat <<EOF
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>$(esc "$1")</title>
<style>
:root { color-scheme: light dark; --bg: #fbfaf7; --fg: #1d2421; --muted: #5f6b66; --line: #dfe3df;
        --code: #f0f2ef; --link: #1f6b4f; }
@media (prefers-color-scheme: dark) {
  :root { --bg: #141917; --fg: #e3e8e5; --muted: #98a59f; --line: #2a332f; --code: #1d2421; --link: #7fd1ad; }
}
body { margin: 0; background: var(--bg); color: var(--fg);
       font: 15px/1.55 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width: 860px; margin: 0 auto; padding: 32px 16px 64px; }
h1 { font-size: 22px; margin: 0 0 4px; overflow-wrap: anywhere; }
h2 { font-size: 17px; margin: 32px 0 8px; }
p { margin: 8px 0; color: var(--muted); }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
pre { background: var(--code); border: 1px solid var(--line); border-radius: 6px; padding: 12px;
      overflow-x: auto; font-size: 13px; }
code { font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; }
table { width: 100%; border-collapse: collapse; margin-top: 16px; font-size: 14px; }
td { padding: 6px 4px; border-bottom: 1px solid var(--line); }
td:first-child { overflow-wrap: anywhere; font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; }
td.size { text-align: right; white-space: nowrap; color: var(--muted); width: 1%; padding-left: 16px; }
</style>
</head>
<body>
<main>
EOF
}

listing() { # listing <directory> <path below the root, "" or ending in />
    local d=$1 rel=$2 entry name
    echo '<table>'
    [ -z "$rel" ] || echo '<tr><td><a href="../">../</a></td><td class="size"></td></tr>'
    while IFS= read -r entry; do
        name=$(esc "${entry##*/}")
        echo "<tr><td><a href=\"$name/\">$name/</a></td><td class=\"size\"></td></tr>"
    done < <(find "$d" -mindepth 1 -maxdepth 1 -type d | LC_ALL=C sort)
    while IFS= read -r entry; do
        name=$(esc "${entry##*/}")
        echo "<tr><td><a href=\"$name\">$name</a></td><td class=\"size\">$(human "$(stat -c %s "$entry")")</td></tr>"
    done < <(find "$d" -mindepth 1 -maxdepth 1 -type f ! -name index.html | LC_ALL=C sort)
    echo '</table>'
}

root_intro() {
    local u suites
    u=$(esc "$url")
    suites=$(find "$dir/dists" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | LC_ALL=C sort | tr '\n' ' ')
    cat <<HTML
<h1>Nt packages</h1>
<p>Signed package repositories with every final release of
<a href="https://open-nt.org/">Nt</a>, a clean-room implementation of the Qt 6 API.
Release candidates are on the <a href="https://github.com/open-nt/Nt6/releases">releases page</a>.</p>

<h2>Ubuntu (apt)</h2>
<p>Suites: $(esc "${suites% }").</p>
<pre><code>sudo install -d -m 0755 /etc/apt/keyrings
sudo curl -fsSLo /etc/apt/keyrings/nt6.gpg $u/nt6.gpg
sudo tee /etc/apt/sources.list.d/nt6.sources &gt; /dev/null &lt;&lt;EOF
Types: deb
URIs: $u
Suites: \$(. /etc/os-release &amp;&amp; echo "\$VERSION_CODENAME")
Components: main
Signed-By: /etc/apt/keyrings/nt6.gpg
EOF
sudo apt update
sudo apt install libnt6-dev nt6-tools</code></pre>

<h2>RHEL, Rocky Linux, AlmaLinux 9 and 10 (dnf)</h2>
<pre><code>sudo curl -fsSLo /etc/yum.repos.d/nt6.repo $u/nt6.repo
sudo dnf install nt6-devel nt6-tools</code></pre>

<h2>Arch Linux (pacman)</h2>
<pre><code>curl -fsSLo /tmp/nt6.asc $u/nt6.asc
sudo pacman-key --add /tmp/nt6.asc
sudo pacman-key --lsign-key "\$(gpg --show-keys --with-colons /tmp/nt6.asc | awk -F: '\$1 == "fpr" { print \$10; exit }')"
sudo tee -a /etc/pacman.conf &gt; /dev/null &lt;&lt;'EOF'

[nt6]
SigLevel = Required
Server = $u/arch/\$arch
EOF
sudo pacman -Syu nt6</code></pre>

<p>The signing key: <a href="nt6.asc">nt6.asc</a>. More in the
<a href="https://open-nt.org/install.html">install guide</a>.</p>

<h2>Files</h2>
HTML
}

count=0
while IFS= read -r d; do
    rel=${d#"$dir"}
    rel=${rel#/}
    [ -z "$rel" ] || rel=$rel/
    {
        if [ -z "$rel" ]; then
            head_html "Nt packages"
            root_intro
        else
            head_html "Index of /$rel"
            echo "<h1>Index of /$(esc "$rel")</h1>"
        fi
        listing "$d" "$rel"
        printf '</main>\n</body>\n</html>\n'
    } > "$d/index.html"
    count=$((count + 1))
done < <(find "$dir" -type d)
echo "==> repo-index.sh: wrote $count index.html files in $dir"
