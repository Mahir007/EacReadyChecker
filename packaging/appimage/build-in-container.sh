#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs build-appimage.sh in an AlmaLinux 9 container with podman or docker, as CI does.
#
#   packaging/appimage/build-in-container.sh <qt-prefix> <output-dir>
#
# VERSION and SOURCE_DATE_EPOCH are passed on, and so is CACHE_DIR, mounted into the container.

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <qt-prefix> <output-dir>" >&2
    exit 1
fi
qt=$(realpath "$1")
mkdir -p "$2"
out=$(realpath "$2")
src=$(cd "$(dirname "$0")/../.." && pwd)
cache=${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/eacheck-appimage}
mkdir -p "$cache"
engine=$(command -v podman || command -v docker)

# The Qt private header check needs fontconfig-devel. patchelf is in EPEL.
"$engine" run --rm \
    -v "$src:/src:ro,z" -v "$qt:/qt:ro,z" -v "$out:/out:z" -v "$cache:/cache:z" \
    -e VERSION="${VERSION:-0.0.0-dev}" \
    -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git -C "$src" log -1 --format=%ct)}" \
    -e BUILD_DIR=/build -e CACHE_DIR=/cache \
    docker.io/library/almalinux:9 bash -c '
        set -e
        dnf install -y -q --setopt=install_weak_deps=False dnf-plugins-core epel-release
        dnf config-manager --set-enabled crb
        dnf install -y -q --setopt=install_weak_deps=False \
            gcc-c++ cmake ninja-build libglvnd-devel libxkbcommon-devel wayland-devel fontconfig-devel \
            patchelf file
        /src/packaging/appimage/build-appimage.sh /qt /out'
