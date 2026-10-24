// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestSysroot.h"
#include "checks/AllChecks.h"
#include "core/CheckRunner.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{
// Passes if a file reads "1". Optionally needs admin to read a second file.
class FakeCheck : public Check
{
public:
    FakeCheck(QString path, QString privilegedPath = {})
        : m_path(std::move(path))
        , m_privilegedPath(std::move(privilegedPath))
    {
    }

    QString id() const override { return u"fake"_s; }
    QString title() const override { return u"Fake"_s; }
    QList<HostRequest> requests() const override { return {HostRequest::read(m_path)}; }
    QList<HostRequest> privilegedRequests() const override
    {
        return m_privilegedPath.isEmpty() ? QList<HostRequest>{} : QList{HostRequest::read(m_privilegedPath)};
    }

    CheckResult evaluate(const HostSnapshot &snapshot) const override
    {
        CheckResult result;
        if (!m_privilegedPath.isEmpty() && snapshot.privilegedState != PrivilegedState::Granted) {
            result.status = CheckStatus::NeedsAdmin;
            return result;
        }
        const HostEntry entry = snapshot.file(m_path);
        result.status = !entry.ok() ? CheckStatus::Unknown : entry.trimmed() == "1" ? CheckStatus::Pass : CheckStatus::Fail;
        return result;
    }

private:
    QString m_path;
    QString m_privilegedPath;
};

CheckList fakeChecks(std::initializer_list<FakeCheck *> checks)
{
    CheckList list;
    for (FakeCheck *check : checks) {
        list.emplace_back(check);
    }
    return list;
}
} // namespace

class TestCheckRunner : public QObject
{
    Q_OBJECT

private:
    TestSysroot m_root;

private Q_SLOTS:
    void initTestCase()
    {
        m_root.write(u"/one"_s, "1\n");
        m_root.write(u"/zero"_s, "0\n");
        m_root.write(u"/proc/sys/kernel/kexec_load_disabled"_s, "1\n");
        m_root.write(u"/sys/kernel/security/lsm"_s, "capability,bpf");
        m_root.write(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s, QByteArray(1024, '\0'));
        m_root.write(u"/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c"_s, QByteArray("\x06\x00\x00\x00\x01", 5));
    }

    void fillsIdAndTitle()
    {
        const HostAccess host(HostAccess::Transport::Direct, {}, m_root.path());
        const Report report = CheckRunner::run(host, fakeChecks({new FakeCheck(u"/one"_s)}), false);
        QCOMPARE(report.results.size(), 1);
        QCOMPARE(report.results[0].id, u"fake"_s);
        QCOMPARE(report.results[0].title, u"Fake"_s);
        QCOMPARE(report.results[0].status, CheckStatus::Pass);
        QCOMPARE(report.verdict(), Verdict::Ready);
        QCOMPARE(report.transport, u"direct"_s);
    }

    void verdictFromChecks()
    {
        const HostAccess host(HostAccess::Transport::Direct, {}, m_root.path());
        const Report report = CheckRunner::run(host, fakeChecks({new FakeCheck(u"/one"_s), new FakeCheck(u"/zero"_s)}), false);
        QCOMPARE(report.results[1].status, CheckStatus::Fail);
        QCOMPARE(report.verdict(), Verdict::NotReady);
    }

    void privilegedOnlyWhenElevated()
    {
        const HostAccess host(HostAccess::Transport::Direct, {}, m_root.path());

        const Report plain = CheckRunner::run(host, fakeChecks({new FakeCheck(u"/one"_s, u"/zero"_s)}), false);
        QCOMPARE(plain.privilegedState, PrivilegedState::NotRequested);
        QCOMPARE(plain.results[0].status, CheckStatus::NeedsAdmin);
        QCOMPARE(plain.verdict(), Verdict::Incomplete);

        const Report elevated = CheckRunner::run(host, fakeChecks({new FakeCheck(u"/one"_s, u"/zero"_s)}), true);
        QCOMPARE(elevated.privilegedState, PrivilegedState::Granted);
        QCOMPARE(elevated.results[0].status, CheckStatus::Pass);
    }

    void allChecksInOrder()
    {
        const HostAccess host(HostAccess::Transport::Direct, {}, m_root.path());
        const CheckList checks = createAllChecks();
        QCOMPARE(checks.size(), size_t(8));

        const Report report = CheckRunner::run(host, checks, false);
        const QStringList ids{u"kernel-signing"_s, u"tpm"_s, u"secure-boot"_s, u"measured-boot"_s, u"iommu"_s, u"lockdown"_s, u"kexec"_s, u"bpf-lsm"_s};
        for (qsizetype i = 0; i < ids.size(); ++i) {
            QCOMPARE(report.results[i].id, ids[i]);
        }
    }

    void rawValues()
    {
        const HostAccess host(HostAccess::Transport::Direct, {}, m_root.path());
        const CheckList checks = createAllChecks();
        const Report report = CheckRunner::run(host, checks, false);
        QCOMPARE(report.results[6].raw.value(u"/proc/sys/kernel/kexec_load_disabled"_s).toString(), u"1"_s);
        QCOMPARE(report.results[7].raw.value(u"/sys/kernel/security/lsm"_s).toString(), u"capability,bpf"_s);
        QCOMPARE(report.results[1].raw.value(u"/sys/class/tpm/tpm0/tpm_version_major"_s).toString(), u"<missing>"_s);
        QCOMPARE(report.results[4].raw.value(u"/sys/class/iommu/*"_s).toArray().size(), 0);
        QCOMPARE(report.results[0].raw.value(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s).toString(), u"<not read>"_s);

        // Large binaries are summarised, small ones (EFI variables) shown as hex.
        const Report elevated = CheckRunner::run(host, checks, true);
        const QJsonObject log = elevated.results[0].raw.value(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s).toObject();
        QCOMPARE(log.value(u"size"_s).toInt(), 1024);
        QCOMPARE(log.value(u"sha256"_s).toString().size(), 64);
        QCOMPARE(elevated.results[2].raw.value(u"/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c"_s).toObject().value(u"hex"_s).toString(),
                 u"0600000001"_s);
    }

    void reportJson()
    {
        const HostAccess host(HostAccess::Transport::Direct, {}, m_root.path());
        const QJsonObject json = CheckRunner::run(host, createAllChecks(), false).toJson();
        QCOMPARE(json.value(u"verdict"_s).toString(), u"not-ready"_s); // no TPM in this sysroot
        QCOMPARE(json.value(u"app"_s).toObject().value(u"id"_s).toString(), u"io.github.Mahir007.EacReadyChecker"_s);
        QCOMPARE(json.value(u"host"_s).toObject().value(u"sysroot"_s).toString(), m_root.path());
        QCOMPARE(json.value(u"host"_s).toObject().value(u"privilegedRead"_s).toString(), u"not-requested"_s);
        const QJsonArray checks = json.value(u"checks"_s).toArray();
        QCOMPARE(checks.size(), 8);
        QCOMPARE(checks[0].toObject().value(u"status"_s).toString(), u"needs-admin"_s);
    }

    void asyncRun()
    {
        CheckRunner runner(HostAccess(HostAccess::Transport::Direct, {}, m_root.path()), fakeChecks({new FakeCheck(u"/one"_s)}));
        QSignalSpy started(&runner, &CheckRunner::started);
        QSignalSpy finished(&runner, &CheckRunner::finished);
        runner.start(false);
        QCOMPARE(started.size(), 1);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().first().value<Report>().results.value(0).status, CheckStatus::Pass);
    }
};

QTEST_GUILESS_MAIN(TestCheckRunner)
#include "tst_checkrunner.moc"
