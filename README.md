# EACheck

A lightweight Qt 6 desktop app that checks whether a Linux machine meets the
**Easy Anti-Cheat Linux v2** system requirements. It reports pass or fail for each requirement.
It only reads system state and never changes it.

Status: early development.

## Build (native)

Debian 13 / Ubuntu packages:

```sh
sudo apt install cmake ninja-build extra-cmake-modules qt6-base-dev qt6-wayland qt6-svg-plugins \
  libkf6coreaddons-dev libkf6iconthemes-dev breeze-icon-theme
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/bin/eac-ready-checker            # window (Wayland)
./build/bin/eac-ready-checker --json     # headless report
```

## Build (Flatpak)

```sh
flatpak install --user flathub org.kde.Platform//6.11 org.kde.Sdk//6.11
flatpak-builder --user --install --force-clean build-flatpak packaging/flatpak/io.github.Mahir007.EacReadyChecker.yml
flatpak run io.github.Mahir007.EacReadyChecker
```

`flatpak-builder` needs `elfutils` installed on the host.

## License

GPL-3.0-or-later
