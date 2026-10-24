// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/CheckResult.h"

#include <QTest>

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
        QCOMPARE(statusKey(CheckStatus::NeedsAdmin), QStringLiteral("needs-admin"));
        QCOMPARE(statusKey(CheckStatus::NotRequired), QStringLiteral("not-required"));
        QCOMPARE(verdictKey(Verdict::NotReady), QStringLiteral("not-ready"));
    }
};

QTEST_GUILESS_MAIN(TestCheckResult)
#include "tst_checkresult.moc"
