#!/usr/bin/env bash
# MIT License - see LICENSE
#
# Pre-commit quality gate for Nt. Run it before every commit; CI runs it too.
#
#   ./check.sh                 Run every check.
#   ./check.sh --quick         Skip the sanitizer build (what the git hook runs).
#   ./check.sh --fix           Also reformat changed lines with clang-format.
#   ./check.sh --install-hook  Install a git pre-commit hook that runs --quick.
#   ./check.sh -j N            Use N parallel jobs (default: all cores).
#
# Checks: file hygiene, tech-debt markers, CMake source lists, header
# self-containment, a -Werror build, the tests, complete ndoc comments,
# clang-format on changed lines, clang-tidy, cppcheck, and the tests again
# under AddressSanitizer and UBSan, and the thread tests under ThreadSanitizer.
#
# Environment:
#   NT_CHECK_BUILD_DIR       Build directory root (default: build/check).
#   NT_CHECK_ALLOW_MISSING   Set to 1 to skip, rather than fail on, missing tools.
#   NT_CHECK_BASE            Commit to compare against for clang-format (default: HEAD).

set -u
set -o pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$root" || exit 2

quick=0
fix=0
jobs=$(nproc 2>/dev/null || echo 4)
while [ $# -gt 0 ]; do
    case $1 in
        --quick) quick=1 ;;
        --fix) fix=1 ;;
        -j) shift; jobs=${1:?-j needs a number} ;;
        -j*) jobs=${1#-j} ;;
        --install-hook)
            hook=$(git rev-parse --git-path hooks/pre-commit)
            mkdir -p "$(dirname "$hook")"
            printf '#!/usr/bin/env bash\nexec "$(git rev-parse --show-toplevel)/check.sh" --quick\n' > "$hook"
            chmod +x "$hook"
            echo "Installed $hook"
            exit 0
            ;;
        -h|--help) sed -n '3,21p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "check.sh: unknown option $1" >&2; exit 2 ;;
    esac
    shift
done

build_root=${NT_CHECK_BUILD_DIR:-build/check}
log_dir="$build_root/logs"
mkdir -p "$log_dir"

if [ -t 1 ]; then
    red=$'\033[31m'; green=$'\033[32m'; yellow=$'\033[33m'; bold=$'\033[1m'; reset=$'\033[0m'
else
    red=; green=; yellow=; bold=; reset=
fi

failed=()
skipped=()

# step <name> <function>: runs the function with its output logged, reports the result.
step() {
    local name=$1 fn=$2 log="$log_dir/${1//[^A-Za-z0-9]/_}.log" start=$SECONDS
    printf '%s==>%s %s\n' "$bold" "$reset" "$name"
    if "$fn" > "$log" 2>&1; then
        printf '    %sPASS%s (%ds)\n' "$green" "$reset" $((SECONDS - start))
    else
        printf '    %sFAIL%s (%ds)\n' "$red" "$reset" $((SECONDS - start))
        tail -n 40 "$log" | sed 's/^/    | /'
        printf '    | full log: %s\n' "$log"
        failed+=("$name")
    fi
}

# need <tool>: succeeds if the tool exists. A missing tool fails the step unless allowed.
need() {
    command -v "$1" > /dev/null 2>&1 && return 0
    if [ "${NT_CHECK_ALLOW_MISSING:-0}" = 1 ]; then
        echo "SKIPPED: $1 is not installed"
        skipped+=("$1")
        return 1
    fi
    echo "$1 is not installed (set NT_CHECK_ALLOW_MISSING=1 to skip it)"
    return 2
}

# Tracked plus untracked, non-ignored files that still exist.
repo_files() {
    git ls-files -co --exclude-standard -- "$@" | while IFS= read -r f; do [ -f "$f" ] && printf '%s\n' "$f"; done
}
cpp_files() { repo_files 'include/*' 'src/*' 'tests/*' 'tools/*' | grep -E '\.(h|cpp|inc)$' || true; }
text_files() {
    repo_files | grep -vE '^assets/' \
        | grep -E '\.(h|cpp|inc|cmake|txt|sh|yml|conf|in)$|(^|/)(VERSION|\.clang-format|\.clang-tidy|\.editorconfig)$' || true
}

check_hygiene() {
    local bad=0 f
    while IFS= read -r f; do
        if grep -qP '\t' "$f"; then echo "$f: contains tabs"; bad=1; fi
        if grep -qE '[[:space:]]+$' "$f"; then echo "$f: trailing whitespace ($(grep -nE '[[:space:]]+$' "$f" | head -1 | cut -d: -f1))"; bad=1; fi
        if grep -qU $'\r' "$f"; then echo "$f: CRLF line endings"; bad=1; fi
        if [ -s "$f" ] && [ -n "$(tail -c1 "$f")" ]; then echo "$f: no newline at end of file"; bad=1; fi
    done < <(text_files)
    # Every source file carries the license header; one or two line forwarding headers are exempt.
    while IFS= read -r f; do
        case $f in tests/ndoc/*|*.inc) continue ;; esac
        [ "$(wc -l < "$f")" -lt 3 ] && continue
        if ! head -n 1 "$f" | grep -q '^// MIT License - see LICENSE$'; then
            echo "$f: first line must be '// MIT License - see LICENSE'"; bad=1
        fi
    done < <(cpp_files)
    # Public headers use #pragma once.
    while IFS= read -r f; do
        [ "$(wc -l < "$f")" -lt 3 ] && continue
        if ! grep -q '^#pragma once' "$f"; then echo "$f: missing #pragma once"; bad=1; fi
    done < <(repo_files 'include/*.h' | grep -E '^include/Qt[A-Za-z]+/q[a-z0-9_]+\.h$' || true)
    return $bad
}

check_debt_markers() {
    local hits
    # TODO is allowed only with an issue reference, e.g. TODO(#123).
    hits=$( (cpp_files; repo_files 'CMakeLists.txt' 'cmake/*' 'packaging/*') | sort -u \
        | xargs -r grep -nP '\b(TODO|FIXME|HACK)\b(?!\(#[0-9]+\))|\bXXX\b' 2>/dev/null || true)
    if [ -n "$hits" ]; then
        echo "$hits"
        echo
        echo "Fix it now, or file an issue and write TODO(#<number>)."
        return 1
    fi
}

check_cmake_sources() {
    local listed bad=0 f
    listed=$(grep -oE '(src|tools)/[A-Za-z0-9_/]+\.cpp' CMakeLists.txt | sort)
    if [ -n "$(printf '%s\n' "$listed" | uniq -d)" ]; then
        echo "listed more than once in CMakeLists.txt:"; printf '%s\n' "$listed" | uniq -d; bad=1
    fi
    while IFS= read -r f; do
        printf '%s\n' "$listed" | grep -qx "$f" || { echo "$f: not built by CMakeLists.txt"; bad=1; }
    done < <(repo_files 'src/*.cpp')
    while IFS= read -r f; do
        [ -f "$f" ] || { echo "$f: listed in CMakeLists.txt but missing"; bad=1; }
    done < <(printf '%s\n' "$listed" | uniq)
    return $bad
}

check_headers() {
    local cxx=${CXX:-c++} dirs="-Iinclude -Iinclude/QtCore -Iinclude/QtNetwork -Iinclude/QtSerialPort -Iinclude/QtDBus -Iinclude/QtGui -Iinclude/QtWidgets -Iinclude/QtTest -I$build_root/release/generated" out
    need "$cxx" || return $?
    export cxx dirs
    # Each public header must compile when it is the only thing included.
    out=$(repo_files 'include/*.h' | grep -E '^include/Qt[A-Za-z]+/q[a-z0-9_]+\.h$' \
        | xargs -r -P "$jobs" -I{} bash -c '
            h=$(basename "$1")
            if ! printf "#include \"%s\"\n" "$h" | $cxx -std=c++17 -fsyntax-only -Wall -Wextra -Werror $dirs -x c++ - 2> /tmp/nt-hdr-$$-$h.log; then
                echo "$1 is not self-contained:"; head -n 5 /tmp/nt-hdr-$$-$h.log
            fi
            rm -f /tmp/nt-hdr-$$-$h.log' _ {})
    [ -z "$out" ] || { printf '%s\n' "$out"; return 1; }
}

configure() { # <dir> [cmake args...]
    local dir=$1; shift
    local gen=()
    command -v ninja > /dev/null 2>&1 && gen=(-G Ninja)
    cmake -S . -B "$dir" "${gen[@]}" -DNT_WARNINGS_AS_ERRORS=ON -DNT_BUILD_EXAMPLES=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
}

check_build() {
    need cmake || return $?
    configure "$build_root/release" -DCMAKE_BUILD_TYPE=Release && \
        cmake --build "$build_root/release" -j "$jobs"
}

check_tests() {
    need ctest || return $?
    ctest --test-dir "$build_root/release" -j "$jobs" --output-on-failure
}

check_docs() {
    # Every public class and member needs an ndoc comment, and the docs must build without warnings.
    "$build_root/release/ndoc" --strict --require-complete -o "$build_root/doc" doc/ndoc.conf
}

check_format() {
    need git-clang-format || return $?
    local files out base=${NT_CHECK_BASE:-HEAD}
    git rev-parse --verify -q "$base^{commit}" > /dev/null || base=HEAD
    files=$(cpp_files | grep -vE '\.inc$|^tests/ndoc/' || true)
    [ -n "$files" ] || return 0
    if [ "$fix" = 1 ]; then
        # git clang-format refuses to edit a tree with unstaged changes, so apply its diff instead.
        # shellcheck disable=SC2086
        git clang-format --diff "$base" -- $files | git apply --whitespace=nowarn 2> /dev/null || true
        return 0
    fi
    # shellcheck disable=SC2086
    out=$(git clang-format --diff "$base" -- $files 2>&1) || true
    case $out in
        *"did not modify any files"*|*"no modified files to format"*|"") ;;
        *) printf '%s\n\nRun ./check.sh --fix to apply it.\n' "$out"; return 1 ;;
    esac
}

check_tidy() {
    # Distributions ship the runner unversioned or as run-clang-tidy-<N>.
    local runner
    runner=$(command -v run-clang-tidy || compgen -c run-clang-tidy- | sort -V | tail -n 1)
    if [ -z "$runner" ]; then need run-clang-tidy; return $?; fi
    # The libraries are held to clang-tidy; the tools are covered by -Werror and cppcheck.
    "$runner" -p "$build_root/release" -quiet -j "$jobs" "$root/src/.*\\.cpp\$"
}

check_cppcheck() {
    need cppcheck || return $?
    cppcheck --enable=warning,performance,portability --std=c++17 --inline-suppr \
        --suppressions-list=.cppcheck-suppressions --error-exitcode=1 --quiet \
        -Iinclude -Iinclude/QtCore -Iinclude/QtNetwork -Iinclude/QtSerialPort -Iinclude/QtDBus -Iinclude/QtGui -Iinclude/QtWidgets -Iinclude/QtTest -I"$build_root/release/generated" -Isrc \
        -j "$jobs" src tools
}

check_shell() {
    local f
    while IFS= read -r f; do bash -n "$f" || return 1; done < <(repo_files '*.sh')
    need shellcheck || return 0
    # Warnings and errors fail; the info-level style notes in the packaging scripts do not.
    repo_files '*.sh' | xargs -r shellcheck --severity=warning
}

# The thread tests under ThreadSanitizer, in their own build. A toolchain without the TSan runtime
# counts as a missing tool. A runtime that cannot start here is skipped: TSan refuses to run when
# the kernel's address space randomisation is too high and the process may not lower it, which is
# the case in unprivileged containers.
check_tsan() {
    need cmake || return $?
    local cxx=${CXX:-c++} probe="$build_root/tsan-probe"
    need "$cxx" || return $?
    mkdir -p "$probe"
    printf 'int main() { return 0; }\n' > "$probe/probe.cpp"
    if ! "$cxx" -fsanitize=thread "$probe/probe.cpp" -o "$probe/probe" 2>&1; then
        if [ "${NT_CHECK_ALLOW_MISSING:-0}" = 1 ]; then
            echo "SKIPPED: $cxx cannot build with -fsanitize=thread"
            skipped+=("tsan")
            return 0
        fi
        echo "$cxx cannot build with -fsanitize=thread (set NT_CHECK_ALLOW_MISSING=1 to skip it)"
        return 2
    fi
    if ! "$probe/probe" 2>&1; then
        echo "SKIPPED: ThreadSanitizer programs cannot run in this environment"
        skipped+=("tsan")
        return 0
    fi
    configure "$build_root/tsan" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNT_HARDENING=OFF \
        -DNT_BUILD_EXAMPLES=OFF -DNT_SANITIZE=thread && \
        cmake --build "$build_root/tsan" -j "$jobs" --target nt_thread_tests && \
        TSAN_OPTIONS=halt_on_error=1 ctest --test-dir "$build_root/tsan" -R '^nt_thread_tests$' --output-on-failure
}

check_sanitizers() {
    need cmake || return $?
    configure "$build_root/asan" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNT_HARDENING=OFF \
        -DNT_BUILD_EXAMPLES=OFF "-DNT_SANITIZE=address;undefined" && \
        cmake --build "$build_root/asan" -j "$jobs" && \
        ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1 \
        LSAN_OPTIONS="suppressions=$root/.lsan-suppressions" \
        ctest --test-dir "$build_root/asan" -j "$jobs" --output-on-failure
}

start=$SECONDS
step "File hygiene and licenses" check_hygiene
step "Tech debt markers" check_debt_markers
step "CMake source lists" check_cmake_sources
step "Shell scripts" check_shell
step "Build with -Werror (Release)" check_build
step "Public headers are self-contained" check_headers
# Everything below needs the build above (tests, ndoc, compile_commands.json).
if [ -x "$build_root/release/ndoc" ]; then
    step "Tests" check_tests
    step "Documentation is complete" check_docs
    step "clang-tidy" check_tidy
else
    printf '%s==>%s Tests, docs and clang-tidy\n    %sFAIL%s (build did not produce ndoc)\n' "$bold" "$reset" "$red" "$reset"
    failed+=("Tests, docs and clang-tidy")
fi
step "clang-format on changed lines" check_format
step "cppcheck" check_cppcheck
if [ "$quick" = 1 ]; then
    skipped+=("sanitizers (--quick)")
else
    step "Tests under ASan and UBSan" check_sanitizers
    step "Thread tests under TSan" check_tsan
fi

echo
if [ ${#skipped[@]} -gt 0 ]; then
    printf '%sSkipped:%s %s\n' "$yellow" "$reset" "${skipped[*]}"
fi
if [ ${#failed[@]} -gt 0 ]; then
    printf '%scheck.sh FAILED%s in %ds:\n' "$red" "$reset" $((SECONDS - start))
    printf '  - %s\n' "${failed[@]}"
    exit 1
fi
printf '%scheck.sh passed%s in %ds\n' "$green" "$reset" $((SECONDS - start))
