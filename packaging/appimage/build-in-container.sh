#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs build-appimage.sh in an AlmaLinux 9 container with podman or docker, as CI does.
#
#   packaging/appimage/build-in-container.sh <qt-prefix> <output-dir>
#
# VERSION, BUILD_TYPE and SOURCE_DATE_EPOCH are passed on, and so is CACHE_DIR, mounted into the
# container.
# The container has no git, so the metainfo's releases are made here from the tags.
# PACKAGE_CACHE and CCACHE_DIR, if set, are folders for dnf's downloads and for ccache.

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
releases=$(mktemp)
trap 'rm -f "$releases"' EXIT
python3 "$src/packaging/metainfo-releases.py" "${VERSION:-0.0.0-dev}" >"$releases"

options=(
    -v "$src:/src:ro,z" -v "$qt:/qt:ro,z" -v "$out:/out:z" -v "$cache:/cache:z"
    -e VERSION="${VERSION:-0.0.0-dev}" -e BUILD_TYPE="${BUILD_TYPE:-Release}"
    -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git -C "$src" log -1 --format=%ct)}"
    -v "$releases:/releases.xml:ro,z" -e METAINFO_RELEASES=/releases.xml
    -e BUILD_DIR=/build -e CACHE_DIR=/cache
)
if [ -n "${PACKAGE_CACHE:-}" ]; then
    mkdir -p "$PACKAGE_CACHE"
    options+=(-v "$(realpath "$PACKAGE_CACHE"):/var/cache/dnf:z")
fi
if [ -n "${CCACHE_DIR:-}" ]; then
    mkdir -p "$CCACHE_DIR"
    # CMake reads the launcher from the environment.
    options+=(-v "$(realpath "$CCACHE_DIR"):/ccache:z" -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-500M}"
        -e CMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi

# The Qt private header check needs fontconfig-devel. patchelf and ccache are in EPEL. lld is the
# fast profile's linker, as GCC 11 can't use mold. Downloaded
# packages are kept for PACKAGE_CACHE, less the versions that are no longer installed.
# shellcheck disable=SC2016 # the script expands its variables in the container
"$engine" run --rm "${options[@]}" docker.io/library/almalinux:9 bash -c '
    set -e
    dnf install -y -q --setopt=install_weak_deps=False --setopt=keepcache=True dnf-plugins-core epel-release
    dnf config-manager --set-enabled crb
    dnf install -y -q --setopt=install_weak_deps=False --setopt=keepcache=True \
        gcc-c++ cmake ninja-build libglvnd-devel libxkbcommon-devel wayland-devel fontconfig-devel \
        patchelf file ccache lld
    rpm -qa --qf "%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm\n" > /tmp/installed
    find /var/cache/dnf -name "*.rpm" | while read -r f; do
        grep -qxF "${f##*/}" /tmp/installed || rm -- "$f"
    done
    if [ -n "${CCACHE_DIR:-}" ]; then ccache --zero-stats > /dev/null; fi
    /src/packaging/appimage/build-appimage.sh /qt /out
    if [ -n "${CCACHE_DIR:-}" ]; then ccache --show-stats; fi'
