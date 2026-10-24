// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Host paths the checks read.
namespace HostPaths
{
inline const QString EfiGlobalGuid = QStringLiteral("8be4df61-93ca-11d2-aa0d-00e098032b8c");
inline const QString ShimLockGuid = QStringLiteral("605dab50-e046-4300-abb6-3dd810dd8b23");

inline const QString EfiDir = QStringLiteral("/sys/firmware/efi");
inline const QString SecureBoot = QStringLiteral("/sys/firmware/efi/efivars/SecureBoot-") + EfiGlobalGuid;
inline const QString SetupMode = QStringLiteral("/sys/firmware/efi/efivars/SetupMode-") + EfiGlobalGuid;
// shim splits a long list into MokListRT, MokListRT1, MokListRT2, … The kernel's copy in
// /sys/firmware/efi/mok-variables/ is root-only (0400), so it isn't read.
inline const QString MokListRt = QStringLiteral("/sys/firmware/efi/efivars/MokListRT*-") + ShimLockGuid;

inline const QString SecurityFs = QStringLiteral("/sys/kernel/security");
inline const QString Lsm = QStringLiteral("/sys/kernel/security/lsm");
inline const QString Lockdown = QStringLiteral("/sys/kernel/security/lockdown");
inline const QString EventLog = QStringLiteral("/sys/kernel/security/tpm0/binary_bios_measurements");

inline const QString TpmDevice = QStringLiteral("/sys/class/tpm/tpm0");
inline const QString TpmVersionMajor = QStringLiteral("/sys/class/tpm/tpm0/tpm_version_major");
inline const QString TpmPcrsSha256 = QStringLiteral("/sys/class/tpm/tpm0/pcr-sha256/*");

inline const QString ChassisType = QStringLiteral("/sys/class/dmi/id/chassis_type");
inline const QString IommuDevices = QStringLiteral("/sys/class/iommu/*");
inline const QString IommuGroups = QStringLiteral("/sys/kernel/iommu_groups/*");
inline const QString IommuGroupTypes = QStringLiteral("/sys/kernel/iommu_groups/*/type");

inline const QString Cmdline = QStringLiteral("/proc/cmdline");
inline const QString Tainted = QStringLiteral("/proc/sys/kernel/tainted");
inline const QString KexecLoadDisabled = QStringLiteral("/proc/sys/kernel/kexec_load_disabled");
} // namespace HostPaths
