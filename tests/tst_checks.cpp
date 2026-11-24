// SPDX-License-Identifier: GPL-3.0-or-later
#include "EfiTestData.h"
#include "TcgTestData.h"
#include "TestSysroot.h"
#include "checks/AllChecks.h"
#include "checks/HostPaths.h"
#include "core/CheckRunner.h"

#include <QTest>

using namespace Qt::StringLiterals;

// Edge cases for each check, on small hand-made sysroots. Whole-machine scenarios live in
// tests/fixtures/ (tst_fixtures).
//
// Files: host path → contents. A path ending in '/' creates an empty directory.
using Files = QMap<QString, QByteArray>;

namespace
{
const QByteArray kEfiAttr("\x06\x00\x00\x00", 4);

QByteArray efiByte(char value)
{
    return kEfiAttr + QByteArray(1, value);
}

Files withSecureBoot(Files files, char secureBoot = 1, char setupMode = 0)
{
    files.insert(HostPaths::SecureBoot, efiByte(secureBoot));
    files.insert(HostPaths::SetupMode, efiByte(setupMode));
    return files;
}

// The TPM event log and the PCR values it replays to.
Files withEventLog(const QList<TcgTestData::Event> &events, Files files = {})
{
    files.insert(HostPaths::EventLog, TcgTestData::log(events));
    const QMap<quint32, QByteArray> pcrs = TcgTestData::pcrFiles(events);
    for (auto it = pcrs.begin(); it != pcrs.end(); ++it) {
        files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/%1"_s.arg(it.key()), it.value());
    }
    return files;
}

// shim + GRUB as on Fedora: the firmware verifies shim against db and logs that authority. shim
// verifies GRUB and the kernel with its built-in certificate and logs it once, as MokListRT.
QList<TcgTestData::Event> grubBoot(const QByteArray &shimAuthority, const QByteArray &kernelAuthority)
{
    using namespace TcgEventLog;
    return {
        {7, Separator, QByteArray(4, '\0')},
        {4, Separator, QByteArray(4, '\0')},
        TcgTestData::authority(u"db"_s, shimAuthority),
        {4, EfiBootServicesApplication, "shimx64.efi"},
        {7, EfiVariableAuthority, TcgTestData::variable(EfiTestData::ShimLock, u"SbatLevel"_s, "sbat,1,2023012900\n")},
        TcgTestData::authority(u"MokListRT"_s, kernelAuthority),
        {4, EfiBootServicesApplication, "grubx64.efi"},
        {9, Ipl, TcgTestData::ascii("(hd0,gpt2)/vmlinuz-6.17.4")},
        {4, EfiBootServicesApplication, "vmlinuz"},
        {8, Ipl, TcgTestData::ascii("kernel_cmdline: (hd0,gpt2)/vmlinuz-6.17.4 root=UUID=0b1c ro quiet")},
    };
}

// A working IOMMU with two translated groups.
Files withIommu(Files files)
{
    files.insert(u"/sys/class/iommu/dmar0/"_s, {});
    files.insert(u"/sys/kernel/iommu_groups/0/type"_s, "DMA-FQ\n");
    files.insert(u"/sys/kernel/iommu_groups/1/type"_s, "DMA\n");
    files.insert(HostPaths::Cmdline, "root=/dev/sda1 quiet\n");
    return files;
}
} // namespace

class TestChecks : public QObject
{
    Q_OBJECT

private:
    static Report run(const Files &files, bool elevate)
    {
        TestSysroot root;
        for (auto it = files.begin(); it != files.end(); ++it) {
            if (it.key().endsWith(u'/')) {
                root.mkdir(it.key());
            } else {
                root.write(it.key(), it.value());
            }
        }
        return CheckRunner::run(HostAccess(HostAccess::Transport::Direct, {}, root.path()), createAllChecks(), elevate);
    }

    // Runs the checks on the current row's files and compares one check or sub-check.
    static void verify(bool elevate)
    {
        QFETCH(QString, check);
        QFETCH(Files, files);
        QFETCH(QString, subCheck);
        QFETCH(QString, status);
        QFETCH(QString, detail);

        const Report report = run(files, elevate);
        const auto result = std::find_if(report.results.begin(), report.results.end(), [&](const CheckResult &r) {
            return r.id == check;
        });
        QVERIFY(result != report.results.end());

        CheckStatus gotStatus = result->status;
        QString gotDetail = result->detail;
        if (!subCheck.isEmpty()) {
            const auto sub = std::find_if(result->subChecks.begin(), result->subChecks.end(), [&](const SubCheckResult &s) {
                return s.title == subCheck;
            });
            QVERIFY2(sub != result->subChecks.end(), qPrintable(u"no sub-check "_s + subCheck));
            gotStatus = sub->status;
            gotDetail = sub->detail;
        }
        QCOMPARE(statusKey(gotStatus), status);
        QVERIFY2(gotDetail.contains(detail), qPrintable(u"detail “%1” doesn't contain “%2”"_s.arg(gotDetail, detail)));
    }

private Q_SLOTS:
    void evaluate_data()
    {
        QTest::addColumn<QString>("check");
        QTest::addColumn<Files>("files");
        QTest::addColumn<QString>("subCheck"); // empty: the check itself
        QTest::addColumn<QString>("status");
        QTest::addColumn<QString>("detail"); // substring of the detail line

        const QString tpm = u"tpm"_s;
        QTest::newRow("tpm: 2.0") << tpm << Files{{HostPaths::TpmVersionMajor, "2\n"}} << QString() << u"pass"_s << QString();
        QTest::newRow("tpm: 1.2") << tpm << Files{{HostPaths::TpmVersionMajor, "1\n"}} << QString() << u"fail"_s << u"TPM 1.2"_s;
        QTest::newRow("tpm: none") << tpm << Files{} << QString() << u"fail"_s << u"No TPM"_s;
        QTest::newRow("tpm: old kernel") << tpm << Files{{u"/sys/class/tpm/tpm0/"_s, {}}} << QString() << u"unknown"_s << u"doesn't report"_s;
        QTest::newRow("tpm: garbage") << tpm << Files{{HostPaths::TpmVersionMajor, "x\n"}} << QString() << u"unknown"_s << u"Unexpected"_s;

        const QString sb = u"secure-boot"_s;
        QTest::newRow("sb: on") << sb << withSecureBoot({}) << u"Enabled"_s << u"pass"_s << QString();
        QTest::newRow("sb: on, event log pending") << sb << withSecureBoot({}) << QString() << u"needs-admin"_s << u"Needs admin"_s;
        QTest::newRow("sb: off") << sb << withSecureBoot({}, 0) << QString() << u"fail"_s << u"Secure Boot is off"_s;
        QTest::newRow("sb: setup mode") << sb << withSecureBoot({}, 1, 1) << QString() << u"fail"_s << u"Setup Mode"_s;
        QTest::newRow("sb: user mode") << sb << withSecureBoot({}) << u"Not in Setup Mode"_s << u"pass"_s << QString();
        QTest::newRow("sb: legacy boot") << sb << Files{} << QString() << u"fail"_s << u"Not booted in UEFI mode"_s;
        QTest::newRow("sb: no variable") << sb << Files{{u"/sys/firmware/efi/efivars/"_s, {}}} << u"Enabled"_s << u"unknown"_s
                                         << u"SecureBoot variable not found"_s;
        QTest::newRow("sb: truncated") << sb << Files{{HostPaths::SecureBoot, kEfiAttr}} << u"Enabled"_s << u"unknown"_s << u"Unexpected SecureBoot value"_s;
        QTest::newRow("sb: value 2") << sb << Files{{HostPaths::SecureBoot, efiByte(2)}} << u"Enabled"_s << u"unknown"_s << u"Unexpected SecureBoot value"_s;

        QTest::newRow("measured: event log pending") << u"measured-boot"_s << withEventLog(grubBoot({}, {})) << QString() << u"needs-admin"_s
                                                     << u"Needs admin"_s;

        const QString iommu = u"iommu"_s;
        QTest::newRow("iommu: on") << iommu << withIommu({}) << QString() << u"pass"_s << QString();
        QTest::newRow("iommu: no chassis info") << iommu << withIommu({}) << QString() << u"pass"_s << QString();
        QTest::newRow("iommu: handheld") << iommu << Files{{HostPaths::ChassisType, "11\n"}} << QString() << u"not-required"_s
                                         << u"Not required on handhelds"_s;
        QTest::newRow("iommu: tablet") << iommu << Files{{HostPaths::ChassisType, "30\n"}} << QString() << u"not-required"_s << u"Not required on tablets"_s;
        QTest::newRow("iommu: notebook, none") << iommu << Files{{HostPaths::ChassisType, "10\n"}} << QString() << u"fail"_s << u"No IOMMU found"_s;
        QTest::newRow("iommu: convertible, none") << iommu << Files{{HostPaths::ChassisType, "31\n"}} << QString() << u"fail"_s << u"No IOMMU found"_s;
        {
            Files files = withIommu({});
            files.insert(u"/sys/kernel/iommu_groups/1/type"_s, "identity\n");
            // One device in identity mode (e.g. an AMD APU's integrated GPU) isn't system-wide passthrough.
            QTest::newRow("iommu: one identity group") << iommu << files << QString() << u"pass"_s << QString();
            QTest::newRow("iommu: one identity group, sub-check")
                << iommu << files << u"No passthrough (group types)"_s << u"pass"_s << u"Only group 1 uses passthrough"_s;
            files.insert(u"/sys/kernel/iommu_groups/0/type"_s, "identity\n");
            QTest::newRow("iommu: all groups identity") << iommu << files << QString() << u"fail"_s << u"All 2 groups use passthrough"_s;
            files.insert(u"/sys/kernel/iommu_groups/0/type"_s, "DMA-FQ\n");
            files.insert(u"/sys/kernel/iommu_groups/1/type"_s, "blocked\n");
            QTest::newRow("iommu: unexpected type") << iommu << files << QString() << u"fail"_s << u"Unexpected group types: 1 (blocked)"_s;
            files.remove(u"/sys/kernel/iommu_groups/1/type"_s);
            files.insert(u"/sys/kernel/iommu_groups/1/"_s, {});
            QTest::newRow("iommu: no type attribute") << iommu << files << QString() << u"unknown"_s << u"doesn't report"_s;
        }
        const auto cmdline = [](const QByteArray &line) {
            Files files = withIommu({});
            files.insert(HostPaths::Cmdline, line);
            return files;
        };
        QTest::newRow("iommu: iommu=pt") << iommu << cmdline("ro iommu=pt") << QString() << u"fail"_s << u"iommu=pt"_s;
        QTest::newRow("iommu: iommu=force,pt") << iommu << cmdline("iommu=force,pt") << QString() << u"fail"_s << u"iommu=force,pt"_s;
        QTest::newRow("iommu: pt then nopt") << iommu << cmdline("iommu=pt iommu=nopt") << QString() << u"pass"_s << QString();
        QTest::newRow("iommu: iommu.passthrough=1") << iommu << cmdline("iommu.passthrough=1") << QString() << u"fail"_s << u"iommu.passthrough=1"_s;
        QTest::newRow("iommu: iommu.passthrough=on") << iommu << cmdline("iommu.passthrough=on") << QString() << u"fail"_s << u"iommu.passthrough=on"_s;
        QTest::newRow("iommu: iommu.passthrough=0") << iommu << cmdline("iommu.passthrough=0") << QString() << u"pass"_s << QString();
        QTest::newRow("iommu: pt after --") << iommu << cmdline("ro -- iommu=pt") << QString() << u"pass"_s << QString();

        const QString lockdown = u"lockdown"_s;
        const QByteArray lsm("capability,yama,bpf");
        QTest::newRow("lockdown: integrity") << lockdown << Files{{HostPaths::Lockdown, "none [integrity] confidentiality\n"}} << QString() << u"pass"_s
                                             << QString();
        QTest::newRow("lockdown: integrity via cmdline")
            << lockdown << Files{{HostPaths::Lockdown, "none [integrity] confidentiality\n"}, {HostPaths::Cmdline, "lockdown=integrity"}} << QString()
            << u"pass"_s << u"Set on the kernel command line"_s;
        QTest::newRow("lockdown: confidentiality") << lockdown << Files{{HostPaths::Lockdown, "none integrity [confidentiality]\n"}} << QString() << u"fail"_s
                                                   << u"Lockdown is in confidentiality mode"_s;
        QTest::newRow("lockdown: none") << lockdown << Files{{HostPaths::Lockdown, "[none] integrity confidentiality\n"}} << QString() << u"fail"_s << u"off"_s;
        QTest::newRow("lockdown: LSM not active") << lockdown << Files{{HostPaths::Lsm, lsm}} << QString() << u"fail"_s << u"Lockdown LSM isn't active"_s;
        QTest::newRow("lockdown: no securityfs") << lockdown << Files{} << QString() << u"unknown"_s << u"securityfs"_s;
        QTest::newRow("lockdown: garbage") << lockdown << Files{{HostPaths::Lockdown, "integrity\n"}} << QString() << u"unknown"_s << u"Unexpected"_s;

        const QString kexec = u"kexec"_s;
        QTest::newRow("kexec: disabled") << kexec << Files{{HostPaths::KexecLoadDisabled, "1\n"}} << QString() << u"pass"_s << QString();
        QTest::newRow("kexec: allowed") << kexec << Files{{HostPaths::KexecLoadDisabled, "0\n"}} << QString() << u"fail"_s << u"allowed"_s;
        QTest::newRow("kexec: not built") << kexec << Files{} << QString() << u"pass"_s << u"isn't built"_s;
        QTest::newRow("kexec: garbage") << kexec << Files{{HostPaths::KexecLoadDisabled, "2\n"}} << QString() << u"unknown"_s << u"Unexpected"_s;

        const QString bpf = u"bpf-lsm"_s;
        QTest::newRow("bpf: active") << bpf << Files{{HostPaths::Lsm, lsm}} << QString() << u"pass"_s << QString();
        QTest::newRow("bpf: inactive") << bpf << Files{{HostPaths::Lsm, "capability,yama,apparmor"}} << QString() << u"fail"_s << u"BPF isn't an active LSM"_s;
        QTest::newRow("bpf: exact token") << bpf << Files{{HostPaths::Lsm, "capability,bpfx"}} << QString() << u"fail"_s << QString();
        QTest::newRow("bpf: empty list") << bpf << Files{{HostPaths::Lsm, ""}} << QString() << u"fail"_s << u"BPF isn't an active LSM"_s;
        QTest::newRow("bpf: no securityfs") << bpf << Files{} << QString() << u"unknown"_s << u"securityfs"_s;

        const QString signing = u"kernel-signing"_s;
        const QString modules = u"No unsigned modules loaded"_s;
        QTest::newRow("taint: clean") << signing << Files{{HostPaths::Tainted, "0\n"}} << modules << u"pass"_s << QString();
        // Without /proc/modules there's nothing to name.
        QTest::newRow("taint: out-of-tree") << signing << Files{{HostPaths::Tainted, "4096\n"}} << modules << u"pass"_s << QString();
        QTest::newRow("taint: unsigned") << signing << Files{{HostPaths::Tainted, "8192\n"}} << modules << u"fail"_s
                                         << u"An unsigned module has been loaded since boot"_s;
        QTest::newRow("taint: unsigned, parent") << signing << Files{{HostPaths::Tainted, "12288\n"}} << QString() << u"fail"_s
                                                 << u"An unsigned module has been loaded since boot"_s;
        {
            const QByteArray loaded = "vmw_vsock_vmci_transport 45056 1 - Live 0x0000000000000000\n"
                                      "acer_wmi 36864 0 - Live 0x0000000000000000 (OE)\n"
                                      "nvidia 9945088 2 - Live 0x0000000000000000 (PO)\n";
            const QByteArray two = loaded + "vmmon 20480 0 - Live 0x0000000000000000 (OE+)\n";
            const QByteArray none = "nvidia 9945088 2 - Live 0x0000000000000000 (PO)\n"
                                    "leaving 16384 0 - Unloading 0x0000000000000000 (-)\n";
            QTest::newRow("taint: unsigned module named") << signing << Files{{HostPaths::Tainted, "12289\n"}, {HostPaths::Modules, loaded}} << modules
                                                          << u"fail"_s << u"Unsigned module loaded: acer_wmi"_s;
            QTest::newRow("taint: unsigned modules named") << signing << Files{{HostPaths::Tainted, "12289\n"}, {HostPaths::Modules, two}} << modules
                                                           << u"fail"_s << u"Unsigned modules loaded: acer_wmi, vmmon"_s;
            QTest::newRow("taint: unsigned module unloaded") << signing << Files{{HostPaths::Tainted, "12289\n"}, {HostPaths::Modules, none}} << modules
                                                             << u"fail"_s << u"An unsigned module has been loaded and unloaded since boot"_s;
            const QByteArray signedOnly = "vmw_vsock_vmci_transport 45056 1 - Live 0x0000000000000000\n"
                                          "nvidia 9945088 2 - Live 0x0000000000000000 (PO)\n"
                                          "nvidia_modeset 1638400 1 nvidia, Live 0x0000000000000000 (PO)\n";
            QTest::newRow("taint: out-of-tree named") << signing << Files{{HostPaths::Tainted, "4097\n"}, {HostPaths::Modules, signedOnly}} << modules
                                                      << u"pass"_s << u"Out-of-tree: nvidia, nvidia_modeset"_s;
        }
        QTest::newRow("taint: garbage") << signing << Files{{HostPaths::Tainted, "abc\n"}} << modules << u"unknown"_s << u"Unexpected"_s;
        QTest::newRow("taint: missing") << signing << Files{} << modules << u"unknown"_s << u"Couldn't read"_s;
        QTest::newRow("signing: event log pending") << signing << Files{{HostPaths::Tainted, "0\n"}} << QString() << u"needs-admin"_s << QString();

        const QString moks = u"Module-signing keys (MOKs) are well-known"_s;
        const QString mokList = u"/sys/firmware/efi/efivars/MokListRT-"_s + HostPaths::ShimLockGuid;
        const QString mokList1 = u"/sys/firmware/efi/efivars/MokListRT1-"_s + HostPaths::ShimLockGuid;
        const QByteArray known = EfiTestData::x509(EfiTestData::canonicalCertificate());
        const QByteArray unknown = EfiTestData::x509(EfiTestData::unknownCertificate());
        const QByteArray hash = EfiTestData::list(EfiSignatureList::CertSha256, {QByteArray(32, 'a')});
        const auto mok = [&](const QByteArray &data) {
            return Files{{HostPaths::Tainted, "0\n"}, {mokList, EfiTestData::variable(data)}};
        };
        QTest::newRow("mok: none") << signing << Files{} << moks << u"pass"_s << u"No MOKs enrolled"_s;
        QTest::newRow("mok: empty variable") << signing << mok({}) << moks << u"pass"_s << u"No MOKs enrolled"_s;
        QTest::newRow("mok: shim vendor cert") << signing << mok(known) << moks << u"pass"_s << u"Canonical Ltd. Master Certificate Authority"_s;
        QTest::newRow("mok: same vendor twice") << signing << mok(known + known) << moks << u"pass"_s << u"Canonical Ltd. Master Certificate Authority"_s;
        QTest::newRow("mok: unknown key") << signing << mok(unknown + known) << moks << u"fail"_s
                                          << u"Unrecognised key: CN=EACheck test MOK, O=EACheck tests"_s;
        QTest::newRow("mok: unknown key, parent") << signing << mok(unknown + known) << QString() << u"fail"_s << u"Unrecognised key: CN=EACheck test MOK"_s;
        QTest::newRow("mok: hash") << signing << mok(hash) << moks << u"fail"_s << u"Unrecognised key: SHA-256 hash 6161616161616161"_s;
        QTest::newRow("mok: two unknown") << signing << mok(hash + unknown) << moks << u"fail"_s
                                          << u"Unrecognised keys: SHA-256 hash 6161616161616161 (+1 more)"_s;
        QTest::newRow("mok: other type") << signing << mok(EfiTestData::list(EfiTestData::ShimLock, {"x"})) << moks << u"fail"_s
                                         << u"605dab50-e046-4300-abb6-3dd810dd8b23 entry"_s;
        {
            Files files = mok(known);
            files.insert(mokList1, EfiTestData::variable(unknown));
            QTest::newRow("mok: split list") << signing << files << moks << u"fail"_s << u"CN=EACheck test MOK"_s;
        }
        QTest::newRow("mok: malformed") << signing << mok(known + "x") << moks << u"unknown"_s << u"Couldn't parse MokListRT"_s;
        QTest::newRow("mok: no attributes") << signing << Files{{mokList, "\x06"}} << moks << u"unknown"_s << u"Couldn't parse MokListRT"_s;
    }

    void evaluate() { verify(false); }

    // Checks that read the TPM event log, with admin access granted.
    void evaluateElevated_data()
    {
        using namespace TcgEventLog;
        using TcgTestData::Event;
        QTest::addColumn<QString>("check");
        QTest::addColumn<Files>("files");
        QTest::addColumn<QString>("subCheck");
        QTest::addColumn<QString>("status");
        QTest::addColumn<QString>("detail");

        const QByteArray known = EfiTestData::canonicalCertificate();
        const QByteArray unknown = EfiTestData::unknownCertificate();
        const QList<Event> grub = grubBoot(known, known);

        const QString measured = u"measured-boot"_s;
        const QString matches = u"Event log matches the TPM"_s;
        const QString kernel = u"Boot manager measured the kernel"_s;
        QTest::newRow("measured: grub") << measured << withEventLog(grub) << kernel << u"pass"_s << u"GRUB"_s;
        QTest::newRow("measured: pcrs match") << measured << withEventLog(grub) << matches << u"pass"_s << u"PCRs 4, 7–9"_s;
        {
            Files files = withEventLog(grub);
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/4"_s, QByteArray(64, '0') + '\n');
            QTest::newRow("measured: pcr mismatch") << measured << files << QString() << u"fail"_s << u"PCR 4 doesn't match the TPM event log"_s;
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/8"_s, QByteArray(64, '0') + '\n');
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/9"_s, QByteArray(64, '0') + '\n');
            QTest::newRow("measured: pcrs mismatch") << measured << files << matches << u"fail"_s << u"PCRs 4, 8–9 don't match"_s;
            files.remove(u"/sys/class/tpm/tpm0/pcr-sha256/9"_s);
            QTest::newRow("measured: pcr file missing") << measured << files << matches << u"unknown"_s << u"Couldn't read PCR 9"_s;
        }
        {
            // systemd extends these strings after boot, outside the firmware log. PCR 4 gets
            // os-separator, PCR 9 also nvpcr-separator. grubBoot's PCRs 7 and 8 stay as they are.
            const auto extended = [&grub](quint32 pcr, const QList<QByteArray> &strings) {
                QList<Event> events = grub;
                for (const QByteArray &string : strings) {
                    events.append({pcr, EfiBootServicesApplication, string});
                }
                return TcgTestData::pcrFiles(events).value(pcr);
            };
            Files files = withEventLog(grub);
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/4"_s, extended(4, {"os-separator"}));
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/9"_s, extended(9, {"os-separator", "nvpcr-separator"}));
            QTest::newRow("measured: systemd separators") << measured << files << matches << u"pass"_s << u"PCRs 4, 7–9"_s;
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/4"_s, extended(4, {"os-separator", "something-else"}));
            QTest::newRow("measured: more than the separators") << measured << files << matches << u"fail"_s << u"PCR 4 doesn't match"_s;
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/4"_s, extended(4, {"something-else"}));
            QTest::newRow("measured: not a separator") << measured << files << matches << u"fail"_s << u"PCR 4 doesn't match"_s;
        }
        {
            Files files{{HostPaths::EventLog, TcgTestData::log(grub)}};
            QTest::newRow("measured: no pcr-sha256") << measured << files << matches << u"unknown"_s << u"doesn't report"_s;
        }
        {
            // The kernel and the command line GRUB measured don't belong together, and the only
            // PCR 4 application is GRUB itself.
            const QList<Event> events{{4, EfiBootServicesApplication, "grubx64.efi"},
                                      {9, Ipl, TcgTestData::ascii("(hd0,gpt2)/vmlinuz-a")},
                                      {8, Ipl, TcgTestData::ascii("kernel_cmdline: (hd0,gpt2)/vmlinuz-b ro")}};
            QTest::newRow("measured: grub, other kernel") << measured << withEventLog(events) << kernel << u"fail"_s << u"No kernel measurement"_s;
        }
        {
            // systemd-pcrphase extends PCR 11 after boot, so it no longer matches the log.
            const QList<Event> events{
                {4, EfiBootServicesApplication, "uki.efi"}, {11, Ipl, TcgTestData::utf16(u".linux"_s)}, {11, Ipl, TcgTestData::utf16(u".osrel"_s)}};
            Files files = withEventLog(events);
            files.insert(u"/sys/class/tpm/tpm0/pcr-sha256/11"_s, QByteArray(64, 'A') + '\n');
            QTest::newRow("measured: uki") << measured << files << kernel << u"pass"_s << u"systemd-stub (UKI)"_s;
        }
        QTest::newRow("measured: efi applications") << measured
                                                    << withEventLog({{4, EfiBootServicesApplication, "loader.efi"}, {4, EfiBootServicesApplication, "vmlinuz"}})
                                                    << kernel << u"pass"_s << u"As an EFI application"_s;
        QTest::newRow("measured: one application") << measured << withEventLog({{4, EfiBootServicesApplication, "loader.efi"}}) << kernel << u"fail"_s
                                                   << u"No kernel measurement"_s;
        QTest::newRow("measured: nothing measured") << measured << withEventLog({{0, NoAction, "info"}}) << matches << u"fail"_s << u"no boot measurements"_s;
        QTest::newRow("measured: no log") << measured << Files{} << QString() << u"fail"_s << u"No TPM event log"_s;
        QTest::newRow("measured: garbage log") << measured << Files{{HostPaths::EventLog, "garbage"}} << QString() << u"unknown"_s
                                               << u"Couldn't parse the TPM event log"_s;
        // A TPM 1.2 log starts straight away with a measured SHA-1 event.
        const QByteArray legacy = EfiTestData::le32(0) + EfiTestData::le32(0x8) + QByteArray(20, 'x') + EfiTestData::le32(4) + "crtm";
        QTest::newRow("measured: legacy log") << measured << Files{{HostPaths::EventLog, legacy}} << QString() << u"fail"_s << u"old SHA-1-only format"_s;
        QTest::newRow("measured: no sha256 bank") << measured << Files{{HostPaths::EventLog, TcgTestData::log(grub, {Sha1})}} << matches << u"unknown"_s
                                                  << u"no SHA-256 digests"_s;

        const QString sb = u"secure-boot"_s;
        const QString usedToBoot = u"Only well-known keys used to boot"_s;
        const QString signing = u"kernel-signing"_s;
        const QString kernelSigned = u"Kernel signed by a well-known key"_s;
        QTest::newRow("authorities: well-known") << sb << withEventLog(grub) << usedToBoot << u"pass"_s << u"Canonical Ltd. Master Certificate Authority"_s;
        QTest::newRow("authorities: unknown kernel key")
            << sb << withEventLog(grubBoot(known, unknown)) << usedToBoot << u"fail"_s << u"Unrecognised key: CN=EACheck test MOK, O=EACheck tests"_s;
        QTest::newRow("authorities: unknown kernel key, signing")
            << signing << withEventLog(grubBoot(known, unknown)) << kernelSigned << u"fail"_s << u"Unrecognised key: CN=EACheck test MOK"_s;
        QTest::newRow("authorities: two unknown") << sb << withEventLog(grubBoot(unknown, unknown)) << usedToBoot << u"fail"_s << u"(+1 more)"_s;
        QTest::newRow("authorities: well-known, signing") << signing << withEventLog(grub) << kernelSigned << u"pass"_s << u"Canonical"_s;
        QTest::newRow("authorities: none") << sb << withEventLog({{4, EfiBootServicesApplication, "grubx64.efi"}}) << usedToBoot << u"fail"_s
                                           << u"no Secure Boot verification"_s;
        QTest::newRow("authorities: sbat only") << sb
                                                << withEventLog(
                                                       {{7, EfiVariableAuthority, TcgTestData::variable(EfiTestData::ShimLock, u"SbatLevel"_s, "sbat")}})
                                                << usedToBoot << u"fail"_s << u"no Secure Boot verification"_s;
        QTest::newRow("authorities: bare certificate")
            << sb << withEventLog({{7, EfiVariableAuthority, TcgTestData::variable(EfiTestData::ShimLock, u"db"_s, known)}}) << usedToBoot << u"pass"_s
            << u"Canonical"_s;
        QTest::newRow("authorities: malformed") << sb << withEventLog({{7, EfiVariableAuthority, "short"}}) << usedToBoot << u"unknown"_s
                                                << u"Couldn't parse the TPM event log"_s;
        {
            // A GPU's option ROM, verified by a key that isn't well-known, before the boot chain.
            QList<Event> events{TcgTestData::authority(u"db"_s, unknown), {2, EfiBootServicesDriver, "gop.efi"}};
            events += grub;
            QTest::newRow("authorities: option rom") << sb << withEventLog(events) << usedToBoot << u"pass"_s
                                                     << u"driver key not counted: CN=EACheck test MOK"_s;
            QTest::newRow("authorities: option rom, signing") << signing << withEventLog(events) << kernelSigned << u"pass"_s << u"Canonical"_s;
            QTest::newRow("authorities: option rom only")
                << sb << withEventLog(events.mid(0, 2)) << usedToBoot << u"fail"_s << u"no Secure Boot verification"_s;
            // A well-known driver key adds nothing to the detail.
            events[0] = TcgTestData::authority(u"db"_s, known);
            QTest::newRow("authorities: well-known option rom") << sb << withEventLog(events) << usedToBoot << u"pass"_s << u"Master Certificate Authority"_s;
        }
        {
            // The next image after the authority is an application, so it verified the boot chain
            // even though a driver comes later.
            const QList<Event> events{
                TcgTestData::authority(u"db"_s, unknown), {4, EfiBootServicesApplication, "shimx64.efi"}, {2, EfiBootServicesDriver, "x.efi"}};
            QTest::newRow("authorities: driver after application") << sb << withEventLog(events) << usedToBoot << u"fail"_s << u"Unrecognised key"_s;
        }
        QTest::newRow("authorities: no log") << sb << Files{} << usedToBoot << u"unknown"_s << u"No TPM event log"_s;
        QTest::newRow("authorities: legacy log") << signing << Files{{HostPaths::EventLog, legacy}} << kernelSigned << u"unknown"_s
                                                 << u"old SHA-1-only format"_s;
    }
    void evaluateElevated() { verify(true); }

    // A sub-check whose precondition failed is left out, not failed again.
    void dependentSubChecks_data()
    {
        QTest::addColumn<QString>("check");
        QTest::addColumn<Files>("files");
        QTest::addColumn<QStringList>("titles");

        const QString iommu = u"iommu"_s;
        const QString active = u"IOMMU active"_s;
        const QString groups = u"IOMMU groups exist"_s;
        const QString types = u"No passthrough (group types)"_s;
        const QString cmdline = u"No passthrough (kernel command line)"_s;
        QTest::newRow("iommu: on") << iommu << withIommu({}) << QStringList{active, groups, types, cmdline};
        QTest::newRow("iommu: none") << iommu << Files{{HostPaths::Cmdline, "iommu=pt\n"}} << QStringList{active, cmdline};
        QTest::newRow("iommu: no groups") << iommu << Files{{u"/sys/class/iommu/dmar0/"_s, {}}, {HostPaths::Cmdline, "quiet\n"}}
                                          << QStringList{active, groups, cmdline};

        const QString sb = u"secure-boot"_s;
        const QString enabled = u"Enabled"_s;
        const QString setupMode = u"Not in Setup Mode"_s;
        const QString keys = u"Only well-known keys used to boot"_s;
        QTest::newRow("sb: uefi") << sb << withSecureBoot({}) << QStringList{enabled, setupMode, keys};
        QTest::newRow("sb: legacy boot") << sb << Files{} << QStringList{enabled, keys};
    }

    void dependentSubChecks()
    {
        QFETCH(QString, check);
        QFETCH(Files, files);
        QFETCH(QStringList, titles);

        const Report report = run(files, false);
        const auto result = std::find_if(report.results.begin(), report.results.end(), [&](const CheckResult &r) {
            return r.id == check;
        });
        QVERIFY(result != report.results.end());
        QStringList got;
        for (const SubCheckResult &sub : result->subChecks) {
            got.append(sub.title);
        }
        QCOMPARE(got, titles);
    }

    void combineStatus_data()
    {
        QTest::addColumn<QList<CheckStatus>>("subs");
        QTest::addColumn<CheckStatus>("expected");
        using S = CheckStatus;
        QTest::newRow("all pass") << QList{S::Pass, S::Pass} << S::Pass;
        QTest::newRow("pass + not required") << QList{S::Pass, S::NotRequired} << S::Pass;
        QTest::newRow("all not required") << QList{S::NotRequired} << S::NotRequired;
        QTest::newRow("fail wins") << QList{S::NeedsAdmin, S::Unknown, S::Fail} << S::Fail;
        QTest::newRow("needs admin over unknown") << QList{S::Unknown, S::NeedsAdmin, S::Pass} << S::NeedsAdmin;
        QTest::newRow("unknown over pass") << QList{S::Pass, S::Unknown} << S::Unknown;
        QTest::newRow("empty") << QList<S>{} << S::Unknown;
    }
    void combineStatus()
    {
        QFETCH(QList<CheckStatus>, subs);
        QFETCH(CheckStatus, expected);
        QList<SubCheckResult> results;
        for (CheckStatus s : std::as_const(subs)) {
            results.append({QString(), s, QString()});
        }
        QCOMPARE(::combineStatus(results), expected);
    }
};

QTEST_GUILESS_MAIN(TestChecks)
#include "tst_checks.moc"
