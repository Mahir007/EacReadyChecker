// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks/AllChecks.h"
#include "core/CheckRunner.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

// Every tree in tests/fixtures/ must produce the statuses in its expected.json, through both
// transports and after a round-trip through tools/capture-fixture.sh.
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

    static Report run(HostAccess::Transport transport, const QString &sysroot)
    {
        return CheckRunner::run(HostAccess(transport, {}, sysroot), createAllChecks(), false);
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
        const QJsonObject checks = expected(fixture).value(u"checks"_s).toObject();
        QVERIFY(!checks.isEmpty());

        const Report direct = run(HostAccess::Transport::Direct, sysroot);
        compareStatuses(direct, checks);

        const Report shell = run(HostAccess::Transport::Shell, sysroot);
        for (qsizetype i = 0; i < direct.results.size(); ++i) {
            QCOMPARE(shell.results[i].status, direct.results[i].status);
            QCOMPARE(shell.results[i].detail, direct.results[i].detail);
            QCOMPARE(shell.results[i].raw, direct.results[i].raw);
        }
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

        const Report original = run(HostAccess::Transport::Direct, sysroot);
        const Report captured = run(HostAccess::Transport::Direct, capture.path() + u"/tree"_s);
        for (qsizetype i = 0; i < original.results.size(); ++i) {
            QCOMPARE(captured.results[i].status, original.results[i].status);
            QCOMPARE(captured.results[i].raw, original.results[i].raw);
        }
    }
};

QTEST_GUILESS_MAIN(TestFixtures)
#include "tst_fixtures.moc"
