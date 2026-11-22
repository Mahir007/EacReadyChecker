<div align="center">

<img src="packaging/icons/io.github.Mahir007.EacReadyChecker.svg" width="112" alt="">

# EACheck

Checks whether a Linux machine meets the
[Easy Anti-Cheat Linux v2 requirements](https://onlineservices.epicgames.com/news/introducing-easy-anti-cheat-linux-v2).

[![CI](https://github.com/Mahir007/EacReadyChecker/actions/workflows/ci.yml/badge.svg)](https://github.com/Mahir007/EacReadyChecker/actions/workflows/ci.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)
![Platform: Linux x86_64](https://img.shields.io/badge/platform-Linux%20x86__64-informational)

[![Download the AppImage](docs/assets/download-appimage.svg)](https://github.com/Mahir007/EacReadyChecker/releases/latest)
&nbsp;
[![Download the Flatpak](docs/assets/download-flatpak.svg)](https://github.com/Mahir007/EacReadyChecker/releases/latest)

<img src="docs/screenshots/needs-admin.png" width="390" alt="EACheck in light mode on Plasma: Incomplete, with three checks that need admin access">
<img src="docs/screenshots/ready-dark.png" width="390" alt="EACheck in dark mode on Plasma: Ready for EAC Linux v2">

</div>

EACheck only reads system state. It asks for an admin password once, to read the TPM event log.

## Requirements

- Linux on x86_64.
- A Wayland session for the window. The command-line reports work without one.
- For the AppImage, glibc 2.34 or later (Ubuntu 22.04, Debian 12, Fedora 35, RHEL 9 or newer).
  libfuse2 isn't needed.
- For the Flatpak, the Flathub remote.
- For the full check, a polkit authentication agent. See
  [Troubleshooting](#no-polkit-authentication-agent-is-running).

## Install

Get either file from the [latest release](https://github.com/Mahir007/EacReadyChecker/releases/latest).

AppImage:

```sh
chmod +x eac-ready-checker-*-x86_64.AppImage
./eac-ready-checker-*-x86_64.AppImage
```

AppImageUpdate, Gear Lever and AppImageLauncher can update it.

Flatpak:

```sh
flatpak install --user eac-ready-checker-*-x86_64.flatpak
flatpak run io.github.Mahir007.EacReadyChecker
```

To build from source, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Usage

The checks run when the window opens. Parts of Signed kernel & kernel modules, Secure Boot and
Measured boot need the TPM event log, which only root can read. Until you click "Run Full Check"
and enter the admin password, those parts show "Needs admin".

| Verdict | Meaning |
| --- | --- |
| Ready for EAC Linux v2 | Every requirement passes or doesn't apply. |
| Not ready | At least one requirement fails. |
| Incomplete | Nothing fails, but at least one requirement needs admin access or couldn't be checked. |

"Copy Report" copies the results as text or JSON. F5 runs the checks again.

### What it checks

| Requirement | Passes when |
| --- | --- |
| Signed kernel & kernel modules | The keys that verified the boot chain are well-known, no unsigned module has been loaded, and every enrolled Machine Owner Key (MOK) is well-known. |
| TPM 2.0 | The TPM is version 2.0. |
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

The results are what EACheck can see on the machine. EAC does its own verification when a game
starts.

## Command line

Run the AppImage file, `flatpak run io.github.Mahir007.EacReadyChecker`, or `eac-ready-checker` from
a source build, with these options:

```
--text      print the report
--json      print the report as JSON, with the values each check read
--elevate   also run the checks that need admin access; on its own, open the window and run them
```

The reports don't need a display. The exit status is 0 for ready, 2 for not ready, 3 for incomplete and 1 for a usage error.

## Troubleshooting

### "No polkit authentication agent is running"

Plasma, GNOME and most other desktops start a polkit agent. Standalone compositors such as Sway and
Hyprland don't, so start one yourself, for example `hyprpolkitagent`, `lxqt-policykit` or
`polkit-gnome`.

### A result looks wrong

Open an issue with the output of `--json --elevate`. It includes the kernel command line and the
subjects of the enrolled keys, which can name the machine, so look it over first. An unrecognised
key from one of Epic's vendors usually means EACheck's list is missing that certificate.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). The GNOME title bar is built from QAdwaitaDecorations in
`third_party/qadwaitadecorations`, which is LGPL-2.1-or-later, see its
[LICENSE](third_party/qadwaitadecorations/LICENSE).
