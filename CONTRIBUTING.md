# Contributing to EACheck

## Layout

| Path | What it is |
| --- | --- |
| `src/core` | Host access, the check runner and reports. Part of `eacheck_core`, which has no widgets, so the CLI and tests run headless. |
| `src/checks` | One class per requirement. A check declares the files it needs and evaluates a snapshot of them. |
| `src/parsers` | EFI signature lists, the TCG2 event log and the kernel command line. |
| `src/data` | `well-known-keys.json`, compiled in as a Qt resource. |
| `src/app` | The Qt Widgets window (`eacheck_app`). |
| `src/cli` | `--json` and `--text` output. |
| `src/decoration` | The GNOME title bar plugin's entry point. It subclasses QAdwaitaDecorations' decoration to add a shadow to unfocused windows. |
| `tests` | Qt Test binaries. `tests/fixtures` has sysroots captured from real machines or written by hand. |
| `tools` | `capture-fixture.sh`, `shim-vendor-certs.py`, `compare-qt-releases.py` and `take-screenshots.sh`. |
| `packaging` | Flatpak manifest, AppImage build scripts, desktop file, metainfo and icon. |
| `docs` | Where each well-known key came from, screenshots, and the README's download buttons. |
| `third_party` | QAdwaitaDecorations, which draws the window's title bar on GNOME with a border and shadow that Qt's own lacks. |

## Requirements

- CMake 3.22 or later, Ninja, and a C++20 compiler such as GCC 11 or later.
- Qt 6.5 or later with its Wayland and SVG plugins.
- For the title bar on GNOME: Qt's private headers and Qt SVG's development files. Without them
  the window gets Qt's own title bar, which has no shadow.
- Breeze icons. `tst_mainwindow` fails without them. The app runs without them and uses its bundled
  status icons.
- `pkexec`, for "Run Full Check" and `--elevate`.

## Linux setup

apt (Debian 13 or later, Ubuntu 26.04 or later):

```sh
sudo apt install g++ git cmake ninja-build qt6-base-dev qt6-wayland qt6-svg-plugins breeze-icon-theme appstream desktop-file-utils \
  qt6-base-private-dev qt6-wayland-private-dev qt6-svg-dev
```

dnf (Fedora 43 or later):

```sh
sudo dnf install gcc-c++ git cmake ninja-build qt6-qtbase-devel qt6-qtsvg breeze-icon-theme appstream desktop-file-utils \
  qt6-qtbase-private-devel qt6-qtsvg-devel
```

pacman (Arch):

```sh
sudo pacman -S --needed gcc git cmake ninja qt6-base qt6-svg breeze-icons appstream desktop-file-utils
```

### Homebrew

Use Homebrew if your distribution ships a Qt older than 6.5. Homebrew's Qt needs glibc 2.38, and
a compiler on an older glibc can't link it, so this installs Homebrew's compiler and glibc too.

```sh
brew install glibc gcc cmake ninja qtbase qtsvg
```

Homebrew has no Breeze icons, so `ctest` needs the distribution's `breeze-icon-theme`.

Configure with this command instead of the one in the next section. `brew list --versions gcc` shows
the version for `g++-16`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)" -DCMAKE_CXX_COMPILER=g++-16
```

## Building and running

```sh
git clone https://github.com/Mahir007/EacReadyChecker.git
cd EacReadyChecker
cmake -S . -B build -G Ninja
cmake --build build
./build/bin/eac-ready-checker          # window
./build/bin/eac-ready-checker --text   # report, no display needed
```

The default build type is `RelWithDebInfo`. Add `-DCMAKE_BUILD_TYPE=Debug` for a debug build, or
`-DBUILD_TESTING=OFF` to skip the tests. The app reports version `0.0.0-dev` unless
`-DEACHECK_VERSION=1.2.3` sets one.

CMake says whether it builds the GNOME title bar plugin. It's built against the same Qt as the app.
The app uses it on that Qt release and on the patch releases checked to share its build (see
[Adding a Qt release](#adding-a-qt-release)). If a Qt update turns it off, the app says so when it
starts, and rebuilding turns it back on.
`-DEACHECK_DECORATIONS=ON` makes missing dependencies an error, and `OFF` leaves the plugin out.

## Flatpak

Needs `flatpak`, `flatpak-builder` and `git` (Debian/Ubuntu also need `elfutils`). From a checkout:

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --user --install-deps-from=flathub --install --force-clean \
  build-flatpak packaging/flatpak/io.github.Mahir007.EacReadyChecker.yml
flatpak run io.github.Mahir007.EacReadyChecker
```

`--install-deps-from=flathub` installs the KDE runtime and SDK (about 1 GB) on the first build.

The sandbox can't see `/sys/firmware`, `/sys/kernel/security` or `/sys/kernel/iommu_groups`, so
reads go through `flatpak-spawn --host`. Test host access changes in the Flatpak as well as
natively.

## AppImage

The AppImage runs on distributions with glibc 2.34 or later. It's built in an AlmaLinux 9 container
with Qt from qt.io, and needs podman or docker, and `aqtinstall` from pip:

```sh
aqt install-qt linux desktop 6.11.3 linux_gcc_64 --archives qtbase qtwayland qtsvg icu --outputdir ~/qt
packaging/appimage/build-in-container.sh ~/qt/6.11.3/gcc_64 dist
./dist/eac-ready-checker-0.0.0-dev-x86_64.AppImage
```

The same commit gives the same file. `VERSION` sets the version it reports. With a release version
such as `1.2.0`, the build also writes a `.zsync` file and embeds update information, so
AppImageUpdate, Gear Lever and AppImageLauncher can update it from the latest GitHub release.

## Tests

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -R tst_fixtures           # one binary
./build/bin/tst_checks combineStatus             # one function
./build/bin/tst_fixtures statuses:fedora-ready   # one data row
./build/bin/tst_checks -functions                # list functions; -datatags lists rows
```

Run `tst_mainwindow` and `tst_iconwidget` through `ctest`, which sets the offscreen platform they
need. The `appstream` and `desktop-file` tests only exist if CMake found `appstreamcli` and
`desktop-file-validate`.

`tst_fixtures` runs every check against every directory in `tests/fixtures`, without and with admin
access, and compares the statuses to its `expected.json`.

| Variable | Effect |
| --- | --- |
| `EACHECK_SCREENSHOT_DIR` | `tst_mainwindow` saves screenshots of the window there. |
| `EACHECK_UUTILS_BIN` | A directory of plain-named uutils links for `tst_hostaccess`. It finds Homebrew's `uutils-coreutils` without this, and skips that test if there's neither. |

## Debugging

`--sysroot` reads host files below a directory instead of `/`, so the CLI or the window can run
against a fixture. Admin reads skip `pkexec` there, so `--elevate` doesn't ask for a password.

```sh
./build/bin/eac-ready-checker --sysroot tests/fixtures/fedora-ready --elevate --text
./build/bin/eac-ready-checker --sysroot tests/fixtures/no-tpm
```

`--json` shows what a check saw: `raw` under each check has the values it read, and `host` has the
transport (`direct` or `flatpak-spawn`) and how the admin read went.

To turn a machine's state into a fixture:

```sh
tools/capture-fixture.sh ~/eacheck-capture --with-event-log
```

The event log needs `sudo`. `proc/cmdline` in the capture shows the root device, so look it over
before sharing.

Without a Wayland session, the window aborts with `no Qt platform plugin could be initialized`.
`QT_QPA_PLATFORM` overrides the platform (`xcb`, or `offscreen` for no display), and
`QT_DEBUG_PLUGINS=1` shows why a plugin didn't load. On GNOME, `QT_WAYLAND_DECORATION=adwaita`
gives Qt's own title bar instead of the bundled one.

## Adding a check

1. Add the class to `src/checks/Checks.h`, its `.cpp` to `src/CMakeLists.txt`, and an entry to
   `createAllChecks()`. `requests()` lists the paths it reads and `privilegedRequests()` the ones
   that need root. `evaluate()` only looks at the snapshot.
2. Add its paths to `tools/capture-fixture.sh`.
3. Add its status to `checks` and `checksElevated` in every `tests/fixtures/*/expected.json`.
4. Update the check counts in `tst_checkrunner`, `tst_cli` and `tst_mainwindow`.

## Adding a well-known key

`src/data/well-known-keys.json` lists keys by SHA-256 fingerprint. Each one needs two independent
sources, recorded in [docs/well-known-keys.md](docs/well-known-keys.md).
`tools/shim-vendor-certs.py SHIMX64.EFI` prints the fingerprint and subject of every certificate a
shim trusts. It needs `objcopy` and `openssl`.

## Adding a Qt release

The GNOME title bar plugin uses Qt's private API. The app uses a build of it on the Qt release it
was built with and on the patch releases in `samePrivateApi` (`src/app/WindowDecoration.cpp`). When
Qt publishes a patch release, compare it with the others of its minor version:

```sh
tools/compare-qt-releases.py ~/qt-compare 6.12.1 6.12.2
```

It downloads each release from qt.io, builds the plugin against it and compares what the plugin
uses of the private API. Releases it reports as the same go in one entry, and a release that
differs starts a new one. The "not checked" rows in `tst_windowdecoration` use the first release
after an entry, so change them to the release after the new one. The script needs `aqtinstall` and
`pyelftools` from pip, and the build tools above.

## Updating the screenshots

The README and the metainfo use the screenshots in `docs/screenshots`. To take them again after a UI
change:

```sh
tools/take-screenshots.sh build
```

Each one runs the window on a fixture in a KWin session on Xvfb, with Plasma's default settings, so
it needs no desktop or GPU. The packages it needs are listed at the top of the script. The metainfo
links to the images at a commit, so after committing new ones, update that commit in its screenshot
URLs.

## CI and releases

`ci.yml` runs on every push. Compiler warnings are errors in the test jobs, except in the vendored
title bar plugin.

| Job | What it does |
| --- | --- |
| `lint` | actionlint, shellcheck and ruff. |
| `test (Debian)` | The tests on the latest Debian, with the apt packages above. |
| `test (Homebrew)` | The tests on Ubuntu with Homebrew's Qt and compiler, installed with the brew command above. |
| `test (minimum)` | The tests with the oldest versions the build accepts: Qt 6.5.0 from qt.io and CMake 3.22, on AlmaLinux 9 with GCC 11 and glibc 2.34. |
| `build`, `check` | Builds the Flatpak bundle in Flathub's KDE container, installs it on Ubuntu and checks its version. |
| `build-appimage`, `check-appimage` | Builds the AppImage on AlmaLinux 9, runs it on Ubuntu and opens its window in Weston with GNOME's settings. |

To release, add the version to `<releases>` in the metainfo and push a tag such as `v1.2.0`.
`release.yml` builds both packages through `ci.yml` with that version and publishes them with notes
from `.github/scripts/release-notes.py`. A tag with a hyphen, such as `v1.2.0-rc.1`, makes a
pre-release, and its AppImage has no update information.

## Upkeep

| What | Where | When |
| --- | --- | --- |
| Qt patch releases | `samePrivateApi`, see [Adding a Qt release](#adding-a-qt-release). | When Qt publishes one. aqtinstall can install a release before qt.io's download pages list it. |
| The KDE runtime | `runtime-version` in the Flatpak manifest, and the `kde-6.x` container image and cache keys of the `build` job. | When Flathub publishes a new branch and Flathub's `flatpak-github-actions` has an image for it. |
| The AppImage's Qt | `QT_VERSION` in the `build-appimage` job, and the [AppImage](#appimage) commands. | With the KDE runtime. The AppImage bundles the Qt the Flatpak runs. |
| QAdwaitaDecorations | `third_party/qadwaitadecorations`, at the commit named in `third_party/CMakeLists.txt`. | When upstream changes. Copy it unchanged, since our own changes go in `src/decoration`. Then run `compare-qt-releases.py` again on every release in `samePrivateApi`, since the private API it uses can change. |
| Pinned tools | actionlint, shellcheck, ruff and aqtinstall in `ci.yml`, the AppImage runtime and appimagetool in `build-appimage.sh`, and the actions' major versions. | Before a release. |
| Epic's requirements | The checks in `src/checks`, the README's table of checks, and the date in `requirementsAsOf()` (`src/core/Report.cpp`). | When Epic changes the article, or publishes its key list. |
| Well-known keys | See [Adding a well-known key](#adding-a-well-known-key). | When a vendor adds a certificate, or a report shows an unrecognised key from one of Epic's vendors. |
| Screenshots | See [Updating the screenshots](#updating-the-screenshots). | After a UI change. |

## Conventions

Commits follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/), with the area
as the scope (`app`, `checks`, `parsers`, `keys`, `host`, `report`, `tools` or `packaging`).

The build turns on Qt's strict string and keyword macros, so use `u"text"_s`, `Q_SLOTS` and
`Q_EMIT`.

Changes to the checks are tested on real machines, and the captures become fixtures.
