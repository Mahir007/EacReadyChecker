#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the AppImage: the app with Qt from qt.io, which runs on glibc 2.34 or later. Build it on
# AlmaLinux 9, so the app needs no newer glibc or libstdc++ than that Qt does. build-in-container.sh
# runs it there, as CI does.
#
#   packaging/appimage/build-appimage.sh <qt-prefix> <output-dir>
#
# <qt-prefix> is a qt.io Qt with qtbase, qtwayland, qtsvg and icu, such as ~/qt/6.11.3/gcc_64.
# VERSION is the version to stamp (default 0.0.0-dev), and BUILD_TYPE the build profile: Release
# (the default) or Debug, which keeps the debug info. SOURCE_DATE_EPOCH defaults to the commit's
# time, so the same commit and Qt give the same file. BUILD_DIR defaults to build-appimage in the
# checkout, and CACHE_DIR, where the AppImage tools are downloaded, to ~/.cache/eacheck-appimage.
# METAINFO_RELEASES is a file from packaging/metainfo-releases.py, for a build without git.
# Needs cmake, ninja, a C++20 compiler, curl, objdump, patchelf and file.

set -euo pipefail

# The AppImage runtime and appimagetool, at fixed releases.
runtime_url=https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64
runtime_sha256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
appimagetool_url=https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage
appimagetool_sha256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0

# Qt plugins the app uses. Only Wayland: the app doesn't run on X11.
qt_plugins=(
    platforms/libqwayland.so
    wayland-shell-integration/libxdg-shell.so
    # Qt's title bars: the app falls back to them when it can't use its own.
    wayland-decoration-client/libadwaita.so
    wayland-decoration-client/libbradient.so
    # GNOME's font and light palette. It loads the host's GTK 3, and Qt uses its own theme without it.
    platformthemes/libqgtk3.so
    iconengines/libqsvgicon.so
    imageformats/libqsvg.so
    # Certificate subjects in the report.
    tls/libqcertonlybackend.so
)

if [ $# -ne 2 ]; then
    echo "usage: $0 <qt-prefix> <output-dir>" >&2
    exit 1
fi
qt=$(realpath "$1")
here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../.." && pwd)
mkdir -p "$2"
out=$(realpath "$2")
version=${VERSION:-0.0.0-dev}
build_type=${BUILD_TYPE:-Release}
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-$(git -C "$src" log -1 --format=%ct)}
export SOURCE_DATE_EPOCH
build=${BUILD_DIR:-$src/build-appimage}
cache=${CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/eacheck-appimage}
appdir=$build/AppDir
id=io.github.Mahir007.EacReadyChecker
name=eac-ready-checker-$version-x86_64.AppImage

# Downloads <url> to <file> unless it's there with the right SHA-256.
fetch() {
    local url=$1 sha256=$2 file=$3
    if [ -f "$file" ] && echo "$sha256  $file" | sha256sum -c --status; then
        return
    fi
    curl -fsSL -o "$file.part" "$url"
    if ! echo "$sha256  $file.part" | sha256sum -c --status; then
        echo "$url: SHA-256 doesn't match" >&2
        exit 1
    fi
    mv "$file.part" "$file"
}
mkdir -p "$cache"
fetch "$runtime_url" "$runtime_sha256" "$cache/runtime-x86_64"
fetch "$appimagetool_url" "$appimagetool_sha256" "$cache/appimagetool-x86_64.AppImage"
chmod +x "$cache/appimagetool-x86_64.AppImage"
# Its zsyncmake, for the .zsync file.
if [ ! -x "$cache/zsyncmake" ]; then
    (cd "$cache" && rm -rf squashfs-root && APPIMAGE_EXTRACT_AND_RUN=1 ./appimagetool-x86_64.AppImage --appimage-extract usr/bin/zsyncmake >/dev/null)
    mv "$cache/squashfs-root/usr/bin/zsyncmake" "$cache/zsyncmake"
    rm -rf "$cache/squashfs-root"
fi

# No RPATH from CMake: it pads the installed one to the length of the build tree's, which holds the
# Qt prefix, so the file would depend on where Qt is. patchelf sets it below.
# Qt's CMake files link OpenGL into the app, and CMake picks GLVND's libOpenGL.so.0 and libGLX.so.0,
# which aren't installed everywhere. qt.io's Qt links libGL.so.1, so use that.
cmake -S "$src" -B "$build/cmake" -G Ninja \
    -DCMAKE_PREFIX_PATH="$qt" \
    -DCMAKE_SKIP_RPATH=ON \
    -DOpenGL_GL_PREFERENCE=LEGACY \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_TESTING=OFF \
    -DEACHECK_DECORATIONS=ON \
    -DEACHECK_VERSION="$version" \
    -DEACHECK_METAINFO_RELEASES="${METAINFO_RELEASES:-}"
cmake --build "$build/cmake"
rm -rf "$appdir"
DESTDIR=$appdir cmake --install "$build/cmake"

# Qt in the layout of a qt.io install, whose libraries and plugins find each other through their
# RUNPATHs: libraries in usr/lib, plugins in usr/plugins/<type>.
copy_qt_libraries() {
    local file lib
    for file in "$@"; do
        for lib in $(objdump -p "$file" | awk '$1 == "NEEDED" { print $2 }'); do
            if [ -e "$qt/lib/$lib" ] && [ ! -e "$appdir/usr/lib/$lib" ]; then
                cp -L "$qt/lib/$lib" "$appdir/usr/lib/$lib"
                copy_qt_libraries "$appdir/usr/lib/$lib"
            fi
        done
    done
}
for plugin in "${qt_plugins[@]}"; do
    install -D -m 755 "$qt/plugins/$plugin" "$appdir/usr/plugins/$plugin"
done
mapfile -t elfs < <(find "$appdir/usr/bin" "$appdir/usr/lib" "$appdir/usr/plugins" -type f -name '*.so' -o -type f -path '*/bin/*')
copy_qt_libraries "${elfs[@]}"

# The app and its title bar plugin come from our build.
for file in "$appdir/usr/bin/eac-ready-checker" "$appdir"/usr/lib/eac-ready-checker/plugins/*/*.so; do
    patchelf --set-rpath "\$ORIGIN/$(realpath --relative-to="$(dirname "$file")" "$appdir/usr/lib")" "$file"
done
printf '[Paths]\nPrefix = ..\n' >"$appdir/usr/bin/qt.conf"

install -m 755 "$here/AppRun" "$appdir/AppRun"
install -D -m 644 "$here/kdeglobals" "$appdir/usr/share/eac-ready-checker/xdg/kdeglobals"
# Tools that add an AppImage to the menu rename its desktop file, and then the app doesn't set its
# ID as the Wayland app ID (main.cpp), so Qt uses the binary's name. StartupWMClass ties the window
# to the renamed file.
cp "$appdir/usr/share/applications/$id.desktop" "$appdir/"
echo 'StartupWMClass=eac-ready-checker' >>"$appdir/$id.desktop"
cp "$appdir/usr/share/icons/hicolor/scalable/apps/$id.svg" "$appdir/"
ln -sf "$id.svg" "$appdir/.DirIcon"

# A release lets AppImageUpdate and the tools that use it update to the latest release, downloading
# only the blocks that changed. Not a pre-release or a development build: "latest" is the newest
# release, which may be older than them.
update=()
if [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    update=(--updateinformation "gh-releases-zsync|Mahir007|EacReadyChecker|latest|eac-ready-checker-*-x86_64.AppImage.zsync")
fi

# The metainfo is checked by the tests on newer distributions. AlmaLinux 9's appstreamcli is older
# than some of what it uses.
rm -f "$out/$name" "$out/$name.zsync"
# appimagetool writes a .zsync file into the folder it runs in. It has the time it was made in it,
# so it's made again below.
(cd "$build" && ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$cache/appimagetool-x86_64.AppImage" --no-appstream \
    "${update[@]}" --runtime-file "$cache/runtime-x86_64" "$appdir" "$out/$name")
rm -f "$build/$name.zsync"
touch -d "@$SOURCE_DATE_EPOCH" "$out/$name"
if [ ${#update[@]} -gt 0 ]; then
    (cd "$out" && "$cache/zsyncmake" -u "$name" -o "$name.zsync" "$name" >/dev/null)
fi
(cd "$out" && sha256sum "$name" >"$name.sha256")
echo "$out/$name"
