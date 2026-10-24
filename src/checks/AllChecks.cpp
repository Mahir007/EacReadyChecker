// SPDX-License-Identifier: GPL-3.0-or-later
#include "AllChecks.h"

#include "PlaceholderCheck.h"

using namespace Qt::StringLiterals;

namespace
{
const QString kEventLog = u"/sys/kernel/security/tpm0/binary_bios_measurements"_s;
const QString kEfiGlobal = u"8be4df61-93ca-11d2-aa0d-00e098032b8c"_s;
const QString kShimGuid = u"605dab50-e046-4300-abb6-3dd810dd8b23"_s;
} // namespace

CheckList createAllChecks()
{
    using R = HostRequest;
    CheckList checks;
    checks.push_back(std::make_unique<PlaceholderCheck>(u"kernel-signing"_s,
                                                        u"Signed kernel & kernel modules"_s,
                                                        QList{R::read(u"/proc/sys/kernel/tainted"_s),
                                                              R::read(u"/sys/firmware/efi/efivars/MokListRT-"_s + kShimGuid),
                                                              R::read(u"/sys/firmware/efi/mok-variables/MokListRT"_s)},
                                                        QList{R::read(kEventLog)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"tpm"_s, u"TPM 2.0"_s, QList{R::read(u"/sys/class/tpm/tpm0/tpm_version_major"_s)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"secure-boot"_s,
                                                        u"Secure Boot"_s,
                                                        QList{R::read(u"/sys/firmware/efi/efivars/SecureBoot-"_s + kEfiGlobal),
                                                              R::read(u"/sys/firmware/efi/efivars/SetupMode-"_s + kEfiGlobal)},
                                                        QList{R::read(kEventLog)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"measured-boot"_s,
                                                        u"Measured boot"_s,
                                                        QList{R::read(u"/sys/class/tpm/tpm0/pcr-sha256/*"_s)},
                                                        QList{R::read(kEventLog)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"iommu"_s,
                                                        u"IOMMU"_s,
                                                        QList{R::read(u"/sys/class/dmi/id/chassis_type"_s),
                                                              R::list(u"/sys/class/iommu/*"_s),
                                                              R::list(u"/sys/kernel/iommu_groups/*"_s),
                                                              R::read(u"/sys/kernel/iommu_groups/*/type"_s),
                                                              R::read(u"/proc/cmdline"_s)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"lockdown"_s,
                                                        u"Kernel lockdown (integrity)"_s,
                                                        QList{R::read(u"/sys/kernel/security/lockdown"_s), R::read(u"/proc/cmdline"_s)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"kexec"_s, u"Kexec disabled"_s, QList{R::read(u"/proc/sys/kernel/kexec_load_disabled"_s)}));
    checks.push_back(std::make_unique<PlaceholderCheck>(u"bpf-lsm"_s, u"BPF LSM"_s, QList{R::read(u"/sys/kernel/security/lsm"_s)}));
    return checks;
}
