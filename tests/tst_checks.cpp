// SPDX-License-Identifier: GPL-3.0-or-later
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

private Q_SLOTS:
    void evaluate_data()
    {
        QTest::addColumn<QString>("check");
        QTest::addColumn<Files>("files");
        QTest::addColumn<QString>("subCheck"); // empty: the check itself
        QTest::addColumn<QString>("status");
        QTest::addColumn<QString>("detail"); // substring of the detail line

        const QString tpm = u"tpm"_s;
        QTest::newRow("tpm: 2.0") << tpm << Files{{HostPaths::TpmVersionMajor, "2\n"}} << QString() << u"pass"_s << u"TPM 2.0"_s;
        QTest::newRow("tpm: 1.2") << tpm << Files{{HostPaths::TpmVersionMajor, "1\n"}} << QString() << u"fail"_s << u"TPM 1.2"_s;
        QTest::newRow("tpm: none") << tpm << Files{} << QString() << u"fail"_s << u"No TPM"_s;
        QTest::newRow("tpm: old kernel") << tpm << Files{{u"/sys/class/tpm/tpm0/"_s, {}}} << QString() << u"unknown"_s << u"doesn't report"_s;
        QTest::newRow("tpm: garbage") << tpm << Files{{HostPaths::TpmVersionMajor, "x\n"}} << QString() << u"unknown"_s << u"Unexpected"_s;

        const QString sb = u"secure-boot"_s;
        QTest::newRow("sb: on") << sb << withSecureBoot({}) << u"Enabled"_s << u"pass"_s << u"on"_s;
        QTest::newRow("sb: on, event log pending") << sb << withSecureBoot({}) << QString() << u"needs-admin"_s << u"Needs admin"_s;
        QTest::newRow("sb: off") << sb << withSecureBoot({}, 0) << QString() << u"fail"_s << u"Secure Boot is off"_s;
        QTest::newRow("sb: setup mode") << sb << withSecureBoot({}, 1, 1) << QString() << u"fail"_s << u"Setup Mode"_s;
        QTest::newRow("sb: user mode") << sb << withSecureBoot({}) << u"Not in Setup Mode"_s << u"pass"_s << u"User Mode"_s;
        QTest::newRow("sb: legacy boot") << sb << Files{} << QString() << u"fail"_s << u"Not booted in UEFI mode"_s;
        QTest::newRow("sb: no variable") << sb << Files{{u"/sys/firmware/efi/efivars/"_s, {}}} << u"Enabled"_s << u"unknown"_s << u"SecureBoot not found"_s;
        QTest::newRow("sb: truncated") << sb << Files{{HostPaths::SecureBoot, kEfiAttr}} << u"Enabled"_s << u"unknown"_s << u"Unexpected contents"_s;
        QTest::newRow("sb: value 2") << sb << Files{{HostPaths::SecureBoot, efiByte(2)}} << u"Enabled"_s << u"unknown"_s << u"Unexpected contents"_s;

        const QString iommu = u"iommu"_s;
        QTest::newRow("iommu: on") << iommu << withIommu({}) << QString() << u"pass"_s << u"no passthrough"_s;
        QTest::newRow("iommu: no chassis info") << iommu << withIommu({}) << QString() << u"pass"_s << QString();
        QTest::newRow("iommu: handheld") << iommu << Files{{HostPaths::ChassisType, "11\n"}} << QString() << u"not-required"_s << u"Handheld"_s;
        QTest::newRow("iommu: tablet") << iommu << Files{{HostPaths::ChassisType, "30\n"}} << QString() << u"not-required"_s << u"Tablet"_s;
        QTest::newRow("iommu: notebook, none") << iommu << Files{{HostPaths::ChassisType, "10\n"}} << QString() << u"fail"_s << u"No IOMMU found"_s;
        QTest::newRow("iommu: convertible, none") << iommu << Files{{HostPaths::ChassisType, "31\n"}} << QString() << u"fail"_s << u"No IOMMU found"_s;
        {
            Files files = withIommu({});
            files.insert(u"/sys/kernel/iommu_groups/1/type"_s, "identity\n");
            QTest::newRow("iommu: identity group") << iommu << files << QString() << u"fail"_s << u"1 of 2 groups use passthrough"_s;
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
                                             << u"Lockdown mode: integrity"_s;
        QTest::newRow("lockdown: integrity via cmdline")
            << lockdown << Files{{HostPaths::Lockdown, "none [integrity] confidentiality\n"}, {HostPaths::Cmdline, "lockdown=integrity"}} << QString()
            << u"pass"_s << u"set via cmdline"_s;
        QTest::newRow("lockdown: confidentiality") << lockdown << Files{{HostPaths::Lockdown, "none integrity [confidentiality]\n"}} << QString()
                                                   << u"fail"_s << u"confidentiality"_s;
        QTest::newRow("lockdown: none") << lockdown << Files{{HostPaths::Lockdown, "[none] integrity confidentiality\n"}} << QString() << u"fail"_s
                                        << u"off"_s;
        QTest::newRow("lockdown: unsupported") << lockdown << Files{{HostPaths::Lsm, lsm}} << QString() << u"fail"_s << u"doesn't support"_s;
        QTest::newRow("lockdown: no securityfs") << lockdown << Files{} << QString() << u"unknown"_s << u"securityfs"_s;
        QTest::newRow("lockdown: garbage") << lockdown << Files{{HostPaths::Lockdown, "integrity\n"}} << QString() << u"unknown"_s << u"Unexpected"_s;

        const QString kexec = u"kexec"_s;
        QTest::newRow("kexec: disabled") << kexec << Files{{HostPaths::KexecLoadDisabled, "1\n"}} << QString() << u"pass"_s << QString();
        QTest::newRow("kexec: allowed") << kexec << Files{{HostPaths::KexecLoadDisabled, "0\n"}} << QString() << u"fail"_s << u"allowed"_s;
        QTest::newRow("kexec: not built") << kexec << Files{} << QString() << u"pass"_s << u"isn't built"_s;
        QTest::newRow("kexec: garbage") << kexec << Files{{HostPaths::KexecLoadDisabled, "2\n"}} << QString() << u"unknown"_s << u"Unexpected"_s;

        const QString bpf = u"bpf-lsm"_s;
        QTest::newRow("bpf: active") << bpf << Files{{HostPaths::Lsm, lsm}} << QString() << u"pass"_s << u"bpf"_s;
        QTest::newRow("bpf: inactive") << bpf << Files{{HostPaths::Lsm, "capability,yama,apparmor"}} << QString() << u"fail"_s << u"apparmor"_s;
        QTest::newRow("bpf: exact token") << bpf << Files{{HostPaths::Lsm, "capability,bpfx"}} << QString() << u"fail"_s << QString();
        QTest::newRow("bpf: empty list") << bpf << Files{{HostPaths::Lsm, ""}} << QString() << u"fail"_s << u"none"_s;
        QTest::newRow("bpf: no securityfs") << bpf << Files{} << QString() << u"unknown"_s << u"securityfs"_s;

        const QString signing = u"kernel-signing"_s;
        const QString modules = u"No unsigned modules loaded"_s;
        QTest::newRow("taint: clean") << signing << Files{{HostPaths::Tainted, "0\n"}} << modules << u"pass"_s << u"isn't tainted"_s;
        QTest::newRow("taint: out-of-tree") << signing << Files{{HostPaths::Tainted, "4096\n"}} << modules << u"pass"_s << u"taint flags: O"_s;
        QTest::newRow("taint: proprietary + out-of-tree") << signing << Files{{HostPaths::Tainted, "4097\n"}} << modules << u"pass"_s << u"P, O"_s;
        QTest::newRow("taint: unsigned") << signing << Files{{HostPaths::Tainted, "8192\n"}} << modules << u"fail"_s << u"taint flags: E"_s;
        QTest::newRow("taint: unsigned, parent") << signing << Files{{HostPaths::Tainted, "12288\n"}} << QString() << u"fail"_s << u"O, E"_s;
        QTest::newRow("taint: garbage") << signing << Files{{HostPaths::Tainted, "abc\n"}} << modules << u"unknown"_s << u"Unexpected"_s;
        QTest::newRow("taint: missing") << signing << Files{} << modules << u"unknown"_s << u"Couldn't read"_s;
        QTest::newRow("signing: event log pending") << signing << Files{{HostPaths::Tainted, "0\n"}} << QString() << u"needs-admin"_s << QString();
    }

    void evaluate()
    {
        QFETCH(QString, check);
        QFETCH(Files, files);
        QFETCH(QString, subCheck);
        QFETCH(QString, status);
        QFETCH(QString, detail);

        TestSysroot root;
        for (auto it = files.begin(); it != files.end(); ++it) {
            if (it.key().endsWith(u'/')) {
                root.mkdir(it.key());
            } else {
                root.write(it.key(), it.value());
            }
        }

        const Report report = CheckRunner::run(HostAccess(HostAccess::Transport::Direct, {}, root.path()), createAllChecks(), false);
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

    void elevatedSubChecks()
    {
        // With a fixture sysroot the admin read is "granted", so Phase 4 sub-checks stop asking.
        TestSysroot root;
        root.write(HostPaths::Tainted, "0\n");
        const Report report = CheckRunner::run(HostAccess(HostAccess::Transport::Direct, {}, root.path()), createAllChecks(), true);
        QCOMPARE(report.results[0].id, u"kernel-signing"_s);
        QCOMPARE(report.results[0].subChecks[0].status, CheckStatus::Unknown);
        QCOMPARE(report.results[0].status, CheckStatus::Unknown);
    }
};

QTEST_GUILESS_MAIN(TestChecks)
#include "tst_checks.moc"
