#!/usr/bin/env bash
# Prints the version in CMakeLists.txt, which every package takes its version from.
set -euo pipefail
sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$(dirname "${BASH_SOURCE[0]}")/../../CMakeLists.txt" | head -n 1
