#!/usr/bin/env bash
# Registers the development build with the desktop for the current user, so the window gets its
# name and icon in the task manager and xdg-desktop-portal recognises the app ID
# (pl.mikigal.PaintForLinux). Writes only to ~/.local/share; --uninstall removes the files again.
#
#   tools/install-dev-desktop.sh [--build DIR]
#   tools/install-dev-desktop.sh --uninstall
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_ID="pl.mikigal.PaintForLinux"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
DESKTOP_FILE="$DATA/applications/$APP_ID.desktop"
ICON_FILE="$DATA/icons/hicolor/scalable/apps/$APP_ID.svg"
build="$ROOT/build/dev"

refresh_caches() {
    update-desktop-database "$DATA/applications" 2>/dev/null || true
    kbuildsycoca6 2>/dev/null || true
}

case "${1:-}" in
    --uninstall)
        rm -f "$DESKTOP_FILE" "$ICON_FILE"
        refresh_caches
        echo "Removed $DESKTOP_FILE and $ICON_FILE"
        exit 0
        ;;
    --build) build="$(cd "$2" && pwd)" ;;
    "") ;;
    *) echo "usage: $0 [--build DIR] | --uninstall" >&2; exit 2 ;;
esac

template="$build/packaging/$APP_ID.desktop"
binary="$build/src/app/paint-for-linux"
[[ -f "$template" && -x "$binary" ]] || { echo "Build first: cmake --build --preset dev" >&2; exit 1; }

mkdir -p "$(dirname "$DESKTOP_FILE")" "$(dirname "$ICON_FILE")"
sed -e "s|^Exec=.*|Exec=$binary %F|" -e "s|^Name=\(.*\)|Name=\1 (dev)|" "$template" >"$DESKTOP_FILE"
cp "$ROOT/resources/app/$APP_ID.svg" "$ICON_FILE"
refresh_caches
echo "Installed $DESKTOP_FILE (runs $binary)"
