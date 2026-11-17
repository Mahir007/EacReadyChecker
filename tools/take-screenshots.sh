#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Takes the screenshots in docs/screenshots, which the README and the metainfo use. Each one runs
# the app on a fixture in its own KWin session on Xvfb, with Plasma's default settings, and KWin
# saves the window with its title bar and shadow.
#
#   tools/take-screenshots.sh <build-dir> [<output-dir>]
#
# Needs Xvfb, KWin for X11, Breeze, Plasma's Qt integration, Noto fonts, xdotool, dbus-run-session,
# g++ and Qt's D-Bus development files. On Debian:
#   sudo apt install xvfb kwin-x11 breeze plasma-integration fonts-noto-core xdotool optipng
# KWin draws with Mesa's software OpenGL, so no GPU is needed. Plasma's default session is
# Wayland, but its title bars and shadows look the same on X11. optipng is used if installed.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")

# One screenshot, run by the main part below inside its own D-Bus session.
#   --shot <out.png> <kdeglobals or -> <expand-first-row: 0|1> <fixture> [app args...]
if [ "${1-}" = --shot ]; then
    out=$2 kdeglobals=$3 expand=$4 fixture=$5
    shift 5
    config=$(mktemp -d)
    export XDG_RUNTIME_DIR=$config/run XDG_CONFIG_HOME=$config
    mkdir -m 700 "$XDG_RUNTIME_DIR"
    [ "$kdeglobals" != - ] && cp "$kdeglobals" "$config/kdeglobals"
    export XDG_CURRENT_DESKTOP=KDE KDE_FULL_SESSION=true KDE_SESSION_VERSION=6 QT_QPA_PLATFORM=xcb
    export LIBGL_ALWAYS_SOFTWARE=1 KWIN_COMPOSE=O2 KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1
    unset WAYLAND_DISPLAY QT_QPA_PLATFORMTHEME QT_STYLE_OVERRIDE

    pids=()
    trap 'kill "${pids[@]}" 2>/dev/null; rm -rf "$config"' EXIT
    display=77
    while [ -e "/tmp/.X$display-lock" ]; do display=$((display + 1)); done
    export DISPLAY=:$display
    Xvfb "$DISPLAY" -screen 0 1600x1200x24 +extension GLX >/dev/null 2>&1 &
    pids+=($!)
    for _ in $(seq 50); do
        xdotool getdisplaygeometry >/dev/null 2>&1 && break
        sleep 0.1
    done

    kwin_x11 >/dev/null 2>&1 &
    pids+=($!)
    for _ in $(seq 50); do
        busctl --user status org.kde.KWin.ScreenShot2 >/dev/null 2>&1 && break
        sleep 0.2
    done

    xdotool mousemove 1599 1199 # keep the pointer from highlighting a row
    "$app" --sysroot "$repo/tests/fixtures/$fixture" "$@" >/dev/null 2>&1 &
    pids+=($!)
    window=$(xdotool search --sync --name '^EACheck$' | tail -n 1)
    sleep 2 # the checks run and the icons fade in
    if [ "$expand" = 1 ]; then
        # The first row's centre is 92 px below the top of the window's contents.
        xdotool mousemove --window "$window" 250 92 click 1 mousemove 1599 1199
        sleep 1
    fi
    "$grab" "$out"
    exit
fi

if [ $# -lt 1 ]; then
    echo "usage: $0 <build-dir> [<output-dir>]" >&2
    exit 1
fi
app=$(cd "$1" && pwd)/bin/eac-ready-checker
export app
outdir=${2:-$repo/docs/screenshots}
mkdir -p "$outdir"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export grab=$tmp/grab-window
# shellcheck disable=SC2046 # pkg-config prints several words
g++ -std=c++20 -fPIC -o "$grab" "$here/grab-window.cpp" $(pkg-config --cflags --libs Qt6Gui Qt6DBus)

dark=$tmp/kdeglobals-dark
{ cat /usr/share/color-schemes/BreezeDark.colors; printf '\n[General]\nColorScheme=BreezeDark\n'; } >"$dark"

shot() {
    echo "$1"
    dbus-run-session -- "$here/take-screenshots.sh" --shot "$outdir/$1" "${@:2}"
    if command -v optipng >/dev/null; then optipng -quiet -o5 -strip all "$outdir/$1"; fi
}
shot not-ready.png - 1 fedora44-vmware-akmods --elevate
shot ready.png - 0 fedora-ready --elevate
shot needs-admin.png - 0 fedora-ready
shot not-ready-dark.png "$dark" 1 fedora44-vmware-akmods --elevate
