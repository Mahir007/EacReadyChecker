// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks/AllChecks.h"
#include "core/CheckRunner.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace Qt::StringLiterals;

// Every tree in tests/fixtures/ must produce the statuses in its expected.json, without and with
// admin access ("checks" and "checksElevated"), through both transports and after a round-trip
// through tools/capture-fixture.sh.
class TestFixtures : public QObject
{
    Q_OBJECT

private:
    static QJsonObject expected(const QString &fixture)
    {
        QFile file(QStringLiteral(EACHECK_FIXTURES_DIR) + u'/' + fixture + u"/expected.json"_s);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    static Report run(HostAccess::Transport transport, const QString &sysroot, bool elevate)
    {
        return CheckRunner::run(HostAccess(transport, {}, sysroot), createAllChecks(), elevate);
    }

    static void compareStatuses(const Report &report, const QJsonObject &expectedChecks)
    {
        QCOMPARE(report.results.size(), expectedChecks.size());
        for (const CheckResult &result : report.results) {
            QVERIFY2(expectedChecks.contains(result.id), qPrintable(u"no expectation for "_s + result.id));
            const QString want = expectedChecks.value(result.id).toString();
            QVERIFY2(statusKey(result.status) == want,
                     qPrintable(u"%1: got %2 (%3), expected %4"_s.arg(result.id, statusKey(result.status), result.detail, want)));
        }
    }

    static void addFixtures()
    {
        QTest::addColumn<QString>("fixture");
        const QStringList fixtures = QDir(QStringLiteral(EACHECK_FIXTURES_DIR)).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &fixture : fixtures) {
            QTest::newRow(qPrintable(fixture)) << fixture;
        }
    }

private Q_SLOTS:
    void statuses_data() { addFixtures(); }
    void statuses()
    {
        QFETCH(QString, fixture);
        const QString sysroot = QStringLiteral(EACHECK_FIXTURES_DIR) + u'/' + fixture;
        for (bool elevate : {false, true}) {
            const QJsonObject checks = expected(fixture).value(elevate ? u"checksElevated"_s : u"checks"_s).toObject();
            QVERIFY(!checks.isEmpty());

            const Report direct = run(HostAccess::Transport::Direct, sysroot, elevate);
            compareStatuses(direct, checks);

            const Report shell = run(HostAccess::Transport::Shell, sysroot, elevate);
            for (qsizetype i = 0; i < direct.results.size(); ++i) {
                QCOMPARE(shell.results[i].status, direct.results[i].status);
                QCOMPARE(shell.results[i].detail, direct.results[i].detail);
                QCOMPARE(shell.results[i].raw, direct.results[i].raw);
            }
        }
    }

    // measured-boot's status doesn't say which rule found the kernel.
    void kernelMeasuredBy_data()
    {
        QTest::addColumn<QString>("fixture");
        QTest::addColumn<QString>("rule");
        QTest::addColumn<int>("afterLog");
        QTest::newRow("shim + GRUB") << u"fedora44-vmware"_s << u"grub"_s << 0;
        // Real firmware, with a GPU option ROM measured into PCR 2.
        QTest::newRow("shim + GRUB, desktop") << u"ubuntu2610-live-dgpu"_s << u"grub"_s << 0;
        // Recent systemd's separators: os-separator on PCRs 0–7, 9, and nvpcr-separator on 9.
        QTest::newRow("systemd-boot + UKI") << u"arch-vmware-sdboot-uki"_s << u"systemd-stub"_s << 9;
        // The same boot chain on real hardware. This one has no separators.
        QTest::newRow("systemd-boot + UKI, laptop") << u"arch-laptop-sdboot-uki-sb"_s << u"systemd-stub"_s << 0;
        QTest::newRow("systemd-boot + plain kernel") << u"popos2404-vmware-sdboot"_s << u"efi-application"_s << 0;
    }
    void kernelMeasuredBy()
    {
        QFETCH(QString, fixture);
        QFETCH(QString, rule);
        QFETCH(int, afterLog);
        const QString sysroot = QStringLiteral(EACHECK_FIXTURES_DIR) + u'/' + fixture;
        const Report report = run(HostAccess::Transport::Direct, sysroot, true);
        const auto measuredBoot = std::find_if(report.results.begin(), report.results.end(), [](const CheckResult &r) {
            return r.id == u"measured-boot"_s;
        });
        QVERIFY(measuredBoot != report.results.end());
        QCOMPARE(measuredBoot->raw.value(u"kernelMeasuredBy"_s).toString(), rule);

        // PCR 11 is left out: systemd-pcrphase extends it after boot, outside the firmware log.
        const QJsonObject replay = measuredBoot->raw.value(u"pcrReplay"_s).toObject();
        QVERIFY(!replay.isEmpty());
        QVERIFY(!replay.contains(u"11"_s));
        const bool allMatch = std::all_of(replay.begin(), replay.end(), [](const QJsonValueConstRef pcr) {
            return pcr.toObject().value(u"match"_s).toBool();
        });
        QVERIFY(allMatch);
        const auto separated = std::count_if(replay.begin(), replay.end(), [](const QJsonValueConstRef pcr) {
            return pcr.toObject().contains(u"afterLog"_s);
        });
        QCOMPARE(separated, afterLog);
    }

    void captureRoundTrip_data() { addFixtures(); }
    void captureRoundTrip()
    {
        QFETCH(QString, fixture);
        const QString sysroot = QStringLiteral(EACHECK_FIXTURES_DIR) + u'/' + fixture;
        QTemporaryDir capture;

        QProcess script;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(u"EACHECK_CAPTURE_ROOT"_s, sysroot);
        script.setProcessEnvironment(env);
        script.start(QStringLiteral(EACHECK_CAPTURE_SCRIPT), {capture.path() + u"/tree"_s, u"--with-event-log"_s});
        QVERIFY(script.waitForFinished());
        QCOMPARE(script.exitCode(), 0);

        for (bool elevate : {false, true}) {
            const Report original = run(HostAccess::Transport::Direct, sysroot, elevate);
            const Report captured = run(HostAccess::Transport::Direct, capture.path() + u"/tree"_s, elevate);
            for (qsizetype i = 0; i < original.results.size(); ++i) {
                QCOMPARE(captured.results[i].status, original.results[i].status);
                QCOMPARE(captured.results[i].raw, original.results[i].raw);
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestFixtures)
#include "tst_fixtures.moc"
