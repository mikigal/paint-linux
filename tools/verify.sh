#!/usr/bin/env bash
# Everything that must pass before a commit.
#
#   tools/verify.sh            build, unit + widget tests, formatting, visual regression
#   tools/verify.sh --tidy     additionally run clang-tidy on src/
#   tools/verify.sh --asan     additionally build and test under AddressSanitizer + UBSan
#   tools/verify.sh --fast     skip the visual regression scenarios
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

tidy=0 asan=0 visual=1
for arg in "$@"; do
    case "$arg" in
        --tidy) tidy=1 ;;
        --asan) asan=1 ;;
        --fast) visual=0 ;;
        *) echo "verify: unknown option $arg" >&2; exit 2 ;;
    esac
done

step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

step "Configure and build (dev)"
cmake --preset dev >/dev/null
cmake --build --preset dev

step "Unit and widget tests"
ctest --preset dev

step "Formatting (clang-format)"
mapfile -t sources < <(git ls-files --cached --others --exclude-standard -- '*.cpp' '*.h')
clang-format --dry-run --Werror "${sources[@]}"

step "Python tools"
python3 -m py_compile tools/paintctl.py tests/visual/*.py

if [[ $visual == 1 ]]; then
    step "Visual regression"
    tests/visual/run.py
fi

if [[ $tidy == 1 ]]; then
    step "clang-tidy"
    # Qt's CMake config adds a GCC-only flag that clang does not know; strip it for clang-tidy.
    mkdir -p build/dev/tidy
    sed 's/ -mno-direct-extern-access//g' build/dev/compile_commands.json >build/dev/tidy/compile_commands.json
    run-clang-tidy -quiet -p build/dev/tidy "$PWD/src/" 2>&1 | tee build/dev/tidy/report.txt
    if grep -qE '(warning|error):' build/dev/tidy/report.txt; then
        echo "verify: clang-tidy reported problems" >&2
        exit 1
    fi
fi

if [[ $asan == 1 ]]; then
    step "AddressSanitizer + UBSan"
    cmake --preset asan >/dev/null
    cmake --build --preset asan
    ctest --preset asan
fi

step "All checks passed"
