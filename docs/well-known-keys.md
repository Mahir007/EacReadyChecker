# Well-known keys

Epic names the vendors whose keys count as well-known, but it hasn't published the certificates.
`src/data/well-known-keys.json` is EACheck's own list of them. Each certificate was taken from two
places that don't depend on each other, and was only added when both copies had the same SHA-256.
They were last checked on 2026-10-26.

For a distro, the first copy is the certificate built into its signed shim, in the `.vendor_cert`
section, which `tools/shim-vendor-certs.py` prints. The second is the one the distro sent to
`rhboot/shim-review`, where Microsoft's reviewers check it.

| Vendor, as Epic names it | Certificate | SHA-256 | First copy | Second copy |
|---|---|---|---|---|
| Canonical Ltd. Master Certificate Authority | `CN=Canonical Ltd. Master Certificate Authority` | `ED:1F:E7:2C:...:67:99:8C` | Ubuntu `shim-signed` 1.59 and 1.37 | shim-review #436 (`canonical/shim-review`, `canonical-uefi-ca.der`) |
| Fedora Secure Boot CA | `CN=fedoraca, OU=Fedora Secure Boot CA 20200709` | `A3:B3:84:81:...:EF:33:2E` | Fedora 44 `shim-x64` 16.1-7 | shim-review #556 (`vathpela/shim-review`, `fedora-ca-20200709.cer`) |
| Fedora Secure Boot CA | `CN=Fedora Secure Boot CA` (2012, expired 2022) | `14:7D:D2:DC:...:C3:02:DB` | Fedora 30 `shim-x64` 15-8 | Fedora dist-git, `shim-unsigned-x64` branch `f30`, `fedora-ca.cer` |
| Universal Blue | `CN=ublue kernel, OU=kernel signing` | `4E:5C:68:47:...:9E:70:D1` | `ublue-os/akmods` `certs/public_key.der` | `ublue-os/bazzite` `secure_boot.der` and `build_files/ubmok102.cer` |
| Universal Blue | `CN=ublue akmods, OU=akmods` | `C0:1E:F5:E7:...:63:75:7D` | `ublue-os/akmods` `certs/public_key_2.der` | `ublue-os/bazzite` `build_files/ubmok101.cer` |
| Debian Secure Boot CA | `CN=Debian Secure Boot CA` | `07:96:46:97:...:CB:F1:A2` | Debian 13 and 11 `shim-signed` | shim-review #536 (`steve-mcintyre/shim-review`, `debian-uefi-ca.der`) |
| openSUSE Secure Boot CA | `CN=openSUSE Secure Boot CA` | `20:7C:5D:A8:...:EF:BB:7D` | Tumbleweed `shim` 16.1 | shim-review #489 (`rfrohl/shim-review`, `shim/openSUSE-UEFI-CA-Certificate.crt`) |
| SUSE Linux Enterprise Secure Boot CA | `CN=SUSE Linux Enterprise Secure Boot CA` | `C3:15:E3:76:...:1D:A5:B4` | Leap 15.6 and 16.0 `shim` (signed by SUSE) | shim-review #489 (`shim/SLES-UEFI-CA-Certificate.crt`) |
| Rocky Linux Secure Boot Root CA | `CN=Rocky Linux Secure Boot Root CA` | `BB:EB:A8:B3:...:46:42:27` | Rocky 8, 9 and 10 `shim-x64` | shim-review #274 (`rocky-linux/shim-review`, `rocky-root-ca.der`) |
| Microsoft UEFI CA | `CN=Microsoft Corporation UEFI CA 2011` | `48:E9:9B:99:...:24:85:07` | Microsoft's download (`go.microsoft.com/fwlink/p/?linkid=321194`) | `microsoft/secureboot_objects`, `PreSignedObjects/DB/Certificates/` |
| Microsoft UEFI CA | `CN=Microsoft UEFI CA 2023` | `F6:12:4E:34:...:DA:D9:01` | Microsoft's download (`go.microsoft.com/fwlink/?linkid=2239872`) | `microsoft/secureboot_objects`. It's also in the PCR 7 log of the Fedora 44 VM |
| AlmaLinux Secure Boot CA | `CN=AlmaLinux Secure Boot CA` | `3D:5C:19:8D:...:7E:6D:EA` | AlmaLinux 8, 9 and 10 `shim-x64` | shim-review #560 (`AlmaLinux/shim-review`, `almalinux-sb-cert-3.der`) |
| AlmaLinux Secure Boot CA | `CN=AlmaLinux OS Foundation` (EV, ST=Florida) | `CE:56:97:3E:...:78:0F:38` | the same shims | shim-review #560 (`almalinux-sb-cert-1.der`) |
| AlmaLinux Secure Boot CA | `CN=AlmaLinux OS Foundation` (EV, L=Fort Myers) | `81:9E:C8:4D:...:B0:03:D9` | the same shims | shim-review #560 (`almalinux-sb-cert-2.der`) |

The JSON file has the full fingerprints and subjects.

## Notes

AlmaLinux's shim trusts three certificates. Its shim-review submission says the two EV ones signed
shim's helpers (cert 1) and GRUB and the kernels until January 2025 (cert 2). They've expired, but
the shim keeps them so older signed binaries still boot. shim copies every certificate it's built
with into MokListRT, so every AlmaLinux machine lists all three, and all three are filed under
Epic's AlmaLinux name.

Fedora's 2012 CA is the only certificate whose CN is exactly "Fedora Secure Boot CA". Fedora
replaced it with `fedoraca` in 2020 and current shims don't have it, but older installs can still
have it in MokListRT.

Universal Blue doesn't ship a shim. Its images boot Fedora's and enroll two keys of their own as
MOKs: "ublue kernel" signs the kernels and "ublue akmods" the kernel modules. The repositories also
have `*.der.test` keys for CI, which aren't on the list.

Epic doesn't name these, so they aren't on the list either: Microsoft Windows Production PCA 2011
and Windows UEFI CA 2023, which sign Windows; Microsoft Option ROM UEFI CA 2023, which signs
firmware drivers such as a GPU's option ROM; and Red Hat's CAs for RHEL and CentOS.

## Adding or updating a key

1. Download the distro's signed shim package and unpack it.
2. Run `tools/shim-vendor-certs.py` on the shim binary: `shimx64.efi`, or `shimx64.efi.signed` on
   Debian and Ubuntu.
3. Compare the fingerprints with the vendor's latest submission to `rhboot/shim-review`.
4. Add the certificate to `src/data/well-known-keys.json` and a row to the table above.
