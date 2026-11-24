// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/CheckResult.h"
#include "core/Report.h"

#include <QTest>

using namespace Qt::StringLiterals;

class TestCheckResult : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void verdict_data()
    {
        QTest::addColumn<QList<CheckStatus>>("statuses");
        QTest::addColumn<Verdict>("expected");

        using S = CheckStatus;
        QTest::newRow("all pass") << QList{S::Pass, S::Pass} << Verdict::Ready;
        QTest::newRow("pass + not required") << QList{S::Pass, S::NotRequired} << Verdict::Ready;
        QTest::newRow("one fail") << QList{S::Pass, S::Fail} << Verdict::NotReady;
        QTest::newRow("fail beats needs admin") << QList{S::NeedsAdmin, S::Fail, S::Unknown} << Verdict::NotReady;
        QTest::newRow("needs admin") << QList{S::Pass, S::NeedsAdmin} << Verdict::Incomplete;
        QTest::newRow("unknown") << QList{S::Pass, S::Unknown} << Verdict::Incomplete;
        QTest::newRow("no checks") << QList<S>{} << Verdict::Ready;
    }

    void verdict()
    {
        QFETCH(QList<CheckStatus>, statuses);
        QFETCH(Verdict, expected);

        QList<CheckResult> results;
        for (CheckStatus status : std::as_const(statuses)) {
            CheckResult r;
            r.status = status;
            results.append(r);
        }
        QCOMPARE(overallVerdict(results), expected);
    }

    void keys()
    {
        QCOMPARE(statusKey(CheckStatus::NeedsAdmin), u"needs-admin"_s);
        QCOMPARE(statusKey(CheckStatus::NotRequired), u"not-required"_s);
        QCOMPARE(verdictKey(Verdict::NotReady), u"not-ready"_s);
    }

    void textReportNamesAccessProblems()
    {
        // A copied report says why rows still need admin, as the window's header does.
        Report report;
        CheckResult result;
        result.title = u"Measured boot"_s;
        result.status = CheckStatus::NeedsAdmin;
        report.results.append(result);
        QVERIFY(report.accessProblems().isEmpty());
        QVERIFY(!report.toText().contains(u"admin access"_s));

        report.privilegedState = PrivilegedState::Cancelled;
        QCOMPARE(report.accessProblems(), QStringList{u"admin access wasn't granted"_s});
        QVERIFY(report.toText().contains(u"\nAdmin access wasn't granted\n"_s));

        report.privilegedState = PrivilegedState::Failed;
        report.privilegedError = u"pkexec exited with 1"_s;
        report.hostReadError = u"sh not found"_s;
        QCOMPARE(report.accessProblems(), (QStringList{u"couldn't read system state: sh not found"_s, u"admin read failed: pkexec exited with 1"_s}));
    }
};

QTEST_GUILESS_MAIN(TestCheckResult)
#include "tst_checkresult.moc"
