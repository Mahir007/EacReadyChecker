<div align="center">

<img src="packaging/icons/io.github.Mahir007.EacReadyChecker.svg" width="112" alt="">

# EACheck

Checks whether a Linux machine meets the
[Easy Anti-Cheat Linux v2 requirements](https://onlineservices.epicgames.com/news/introducing-easy-anti-cheat-linux-v2).

[![CI](https://github.com/Mahir007/EacReadyChecker/actions/workflows/ci.yml/badge.svg)](https://github.com/Mahir007/EacReadyChecker/actions/workflows/ci.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)
![Platform: Linux x86_64](https://img.shields.io/badge/platform-Linux%20x86__64-informational)

[![Get it as an AppImage](docs/assets/appimage-badge.svg)](https://github.com/Mahir007/EacReadyChecker/releases/latest)
&nbsp;
[![Get it as a Flatpak](docs/assets/flatpak-badge.svg)](https://github.com/Mahir007/EacReadyChecker/releases/latest)

<img src="docs/screenshots/needs-admin.png" width="390" alt="EACheck in light mode on Plasma: Incomplete, with three checks that need admin access">
<img src="docs/screenshots/ready-dark.png" width="390" alt="EACheck in dark mode on Plasma: Ready for EAC Linux v2">

</div>

EACheck only reads system state. It runs on x86_64, and the window needs a Wayland session.

## Install

Get either file from the [latest release](https://github.com/Mahir007/EacReadyChecker/releases/latest).

### AppImage

Needs glibc 2.34 or later (Ubuntu 22.04, Debian 12, Fedora 35, RHEL 9 or newer), but not libfuse2.

```sh
chmod +x eac-ready-checker-*-x86_64.AppImage
./eac-ready-checker-*-x86_64.AppImage
```

AppImageUpdate, Gear Lever and AppImageLauncher can update it.

### Flatpak

Needs the Flathub remote, for the KDE runtime.

```sh
flatpak install --user eac-ready-checker-*-x86_64.flatpak
flatpak run io.github.Mahir007.EacReadyChecker
```

To build from source, see [CONTRIBUTING.md](CONTRIBUTING.md).

## What it checks

| Requirement | Passes when |
| --- | --- |
| Signed kernel & kernel modules | The keys that verified the boot chain are well-known, no unsigned module has been loaded, and every enrolled Machine Owner Key (MOK) is well-known. |
| TPM 2.0 | There is a TPM and it's version 2.0. |
| Secure Boot | Secure Boot is on, the firmware isn't in setup mode, and only well-known keys were used to boot. |
| Measured boot | Replaying the TPM event log gives the TPM's PCR values, and the boot manager measured the kernel. |
| IOMMU | The IOMMU is on and passthrough is off. Machines whose firmware reports a handheld or tablet chassis don't need it. |
| Kernel lockdown (integrity) | Lockdown is in integrity mode. Confidentiality mode fails. |
| Kexec disabled | `kexec_load_disabled` is 1, or the kernel is built without kexec. |
| BPF LSM | BPF is one of the active LSMs. |

Epic names the vendors whose keys count as well-known but hasn't published the certificates.
EACheck matches them by fingerprint against its own list, taken from the vendors' signed shims and
repositories. [docs/well-known-keys.md](docs/well-known-keys.md) lists each certificate and its
source.

| Verdict | Meaning | Exit status |
| --- | --- | --- |
| Ready for EAC Linux v2 | Every requirement passes or doesn't apply. | 0 |
| Not ready | At least one requirement fails. | 2 |
| Incomplete | Nothing fails, but at least one requirement needs admin access or couldn't be checked. | 3 |

## Usage

The checks run when the window opens. Parts of Signed kernel & kernel modules, Secure Boot and
Measured boot read the TPM event log, which only root can read, and show "Needs admin" until you
click "Run Full Check". It asks for the admin password once, through polkit.

"Copy Report" copies the results as text or JSON. F5 runs the checks again.

## Command line

```sh
./eac-ready-checker-*-x86_64.AppImage --text
flatpak run io.github.Mahir007.EacReadyChecker --text
```

| Option | |
| --- | --- |
| `--text` | Print the report. |
| `--json` | Print the report as JSON, with the values each check read. |
| `--elevate` | Also run the checks that need admin access. Without `--text` or `--json`, open the window and run them. |

The exit status is the verdict's, or 1 for a usage error.

## Troubleshooting

### "No polkit authentication agent is running"

Plasma, GNOME and most other desktops start a polkit agent. Standalone compositors such as Sway and
Hyprland don't. Start one yourself, such as `hyprpolkitagent`, `lxqt-policykit` or `polkit-gnome`.

### A result looks wrong

Open an issue with the output of `--json --elevate`. It includes the kernel command line and the
subjects of the enrolled keys, which can name the machine, so look it over first. An unrecognised
key from one of Epic's vendors usually means EACheck's list is missing that certificate.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). The GNOME title bar is built from QAdwaitaDecorations in
`third_party/qadwaitadecorations`, which is LGPL-2.1-or-later, see its
[LICENSE](third_party/qadwaitadecorations/LICENSE).
