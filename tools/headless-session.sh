#!/usr/bin/env bash
# Runs Paint for Linux in an isolated session for automated UI checks without touching the
# user's desktop. Two platforms:
#   offscreen  Qt's offscreen platform: fastest, no compositor; enough for most screenshots.
#   kwin       a headless KWin (`kwin_wayland --virtual`) on a private D-Bus session: a real
#              Wayland compositor for window states and cross-application clipboard (wl-copy).
#              Synthetic input carries no Wayland serial there, so menus and other popups do not
#              open from paintctl clicks: drive popups on the offscreen platform instead.
# Both use the checked-in deterministic KDE settings (tests/env) and scratch XDG directories.
#
#   tools/headless-session.sh start [--platform offscreen|kwin] [--theme light|dark]
#                                   [--size WIDTHxHEIGHT] [--build DIR] [-- APP_ARGS...]
#   tools/headless-session.sh stop
#   tools/headless-session.sh status
#   tools/headless-session.sh env     # exports for other clients, e.g. eval "$(... env)"; wl-copy < a.png
#   tools/headless-session.sh logs
#
# Drive the running app with tools/paintctl.py (it finds the socket automatically).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATE_DIR="${XDG_RUNTIME_DIR:-/tmp}/paint-for-linux-test"
SOCKET="$STATE_DIR/automation.sock"
WAYLAND_NAME="wayland-paint-for-linux-test"

die() {
    echo "headless-session: $*" >&2
    exit 1
}

is_running() {
    [[ -f "$STATE_DIR/pid" ]] && kill -0 "$(cat "$STATE_DIR/pid")" 2>/dev/null
}

cmd_stop() {
    if is_running; then
        local pid
        pid=$(cat "$STATE_DIR/pid")
        python3 "$ROOT/tools/paintctl.py" --socket "$SOCKET" quit >/dev/null 2>&1 || true
        for _ in $(seq 30); do
            kill -0 "$pid" 2>/dev/null || break
            sleep 0.1
        done
        # Everything was started in its own process group (app, or dbus + kwin + app).
        kill -TERM -- "-$pid" 2>/dev/null || true
    fi
    rm -f "$STATE_DIR/pid" "$SOCKET"
}

cmd_start() {
    local platform=offscreen theme=light size=1600x1000 build="$ROOT/build/dev"
    local -a app_args=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --platform) platform="$2"; shift 2 ;;
            --theme) theme="$2"; shift 2 ;;
            --size) size="$2"; shift 2 ;;
            --build) build="$2"; shift 2 ;;
            --) shift; app_args=("$@"); break ;;
            *) die "unknown option $1" ;;
        esac
    done
    [[ "$platform" == offscreen || "$platform" == kwin ]] || die "platform must be offscreen or kwin"
    [[ "$theme" == light || "$theme" == dark ]] || die "theme must be light or dark"
    [[ "$size" =~ ^([0-9]+)x([0-9]+)$ ]] || die "size must look like 1600x1000"
    local width=${BASH_REMATCH[1]} height=${BASH_REMATCH[2]}
    local app="$build/src/app/paint-for-linux"
    [[ -x "$app" ]] || die "$app is not built (cmake --build --preset dev)"

    cmd_stop
    rm -rf "$STATE_DIR"
    mkdir -p "$STATE_DIR"/{config,data,cache}
    chmod 700 "$STATE_DIR"
    cp "$ROOT/tests/env/breeze-$theme/kdeglobals" "$STATE_DIR/config/kdeglobals"
    cat >"$STATE_DIR/offscreen.json" <<EOF
{"synchronousWindowSystemEvents": true, "windowFrameMargins": false,
 "screens": [{"name": "OffscreenScreen", "x": 0, "y": 0, "width": $width, "height": $height,
              "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}]}
EOF

    export XDG_CONFIG_HOME="$STATE_DIR/config" XDG_DATA_HOME="$STATE_DIR/data" XDG_CACHE_HOME="$STATE_DIR/cache"
    export QT_QPA_PLATFORMTHEME=kde LANG=C.UTF-8 LC_ALL=C.UTF-8 PAINT_AUTOMATION_SOCKET="$SOCKET"
    unset QT_SCALE_FACTOR QT_SCREEN_SCALE_FACTORS QT_FONT_DPI SESSION_MANAGER

    if [[ "$platform" == offscreen ]]; then
        QT_QPA_PLATFORM="offscreen:configfile=$STATE_DIR/offscreen.json" \
            setsid "$app" "${app_args[@]}" >"$STATE_DIR/app.log" 2>&1 &
        echo $! >"$STATE_DIR/pid"
    else
        command -v kwin_wayland >/dev/null || die "kwin_wayland not found"
        {
            echo '#!/bin/sh'
            echo 'unset DISPLAY'
            echo 'export QT_QPA_PLATFORM=wayland'
            printf 'exec %q' "$app"
            [[ ${#app_args[@]} -gt 0 ]] && printf ' %q' "${app_args[@]}"
            printf ' >%q 2>&1\n' "$STATE_DIR/app.log"
        } >"$STATE_DIR/run-app.sh"
        chmod +x "$STATE_DIR/run-app.sh"
        # A private D-Bus session keeps the nested KWin away from the real desktop's services;
        # GVFS must not try to mount over the real session's FUSE mount point.
        export GIO_USE_VFS=local GVFS_DISABLE_FUSE=1
        setsid dbus-run-session -- kwin_wayland --virtual --socket "$WAYLAND_NAME" \
            --width "$width" --height "$height" --no-lockscreen --no-global-shortcuts --no-kactivities \
            --exit-with-session "$STATE_DIR/run-app.sh" >"$STATE_DIR/kwin.log" 2>&1 &
        echo $! >"$STATE_DIR/pid"
    fi
    echo "$platform $theme ${width}x${height}" >"$STATE_DIR/session"

    for _ in $(seq 100); do
        if python3 "$ROOT/tools/paintctl.py" --socket "$SOCKET" --compact ping >/dev/null 2>&1; then
            echo "Paint for Linux is running ($platform, $theme, ${width}x${height}); socket: $SOCKET"
            return 0
        fi
        is_running || break
        sleep 0.2
    done
    echo "--- app.log" >&2
    cat "$STATE_DIR/app.log" >&2 2>/dev/null || true
    [[ -f "$STATE_DIR/kwin.log" ]] && { echo "--- kwin.log" >&2; tail -20 "$STATE_DIR/kwin.log" >&2; }
    cmd_stop
    die "the application did not come up"
}

cmd_status() {
    if is_running; then
        echo "running: $(cat "$STATE_DIR/session") (pid $(cat "$STATE_DIR/pid"), socket $SOCKET)"
    else
        echo "not running"
        return 1
    fi
}

cmd_env() {
    echo "export PAINT_AUTOMATION_SOCKET=$(printf %q "$SOCKET")"
    if [[ -f "$STATE_DIR/session" && "$(cut -d' ' -f1 "$STATE_DIR/session")" == kwin ]]; then
        echo "export WAYLAND_DISPLAY=$WAYLAND_NAME"
        echo "unset DISPLAY"
    fi
}

cmd_logs() {
    for log in app.log kwin.log; do
        [[ -f "$STATE_DIR/$log" ]] && { echo "--- $log"; cat "$STATE_DIR/$log"; }
    done
}

case "${1:-}" in
    start) shift; cmd_start "$@" ;;
    stop) cmd_stop ;;
    status) cmd_status ;;
    env) cmd_env ;;
    logs) cmd_logs ;;
    *) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
