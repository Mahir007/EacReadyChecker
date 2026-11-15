// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestSysroot.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTest>

using namespace Qt::StringLiterals;

// Runs the real binary headless. No display is needed for CLI runs.
class TestCli : public QObject
{
    Q_OBJECT

private:
    TestSysroot m_root;

    struct Run {
        int exitCode = -1;
        QByteArray out;
    };

    Run run(const QStringList &args)
    {
        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(u"WAYLAND_DISPLAY"_s);
        env.remove(u"DISPLAY"_s);
        process.setProcessEnvironment(env);
        process.start(QStringLiteral(EACHECK_BINARY), args);
        process.waitForFinished();
        if (process.exitStatus() != QProcess::NormalExit) {
            return {};
        }
        return {process.exitCode(), process.readAllStandardOutput()};
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_root.write(u"/sys/kernel/security/lsm"_s, "capability,bpf");
    }

    void json()
    {
        const Run process = run({u"--json"_s, u"--sysroot"_s, m_root.path()});
        QCOMPARE(process.exitCode, 2); // not ready: this sysroot has no TPM, IOMMU, ...

        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(process.out, &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QCOMPARE(doc.object().value(u"verdict"_s).toString(), u"not-ready"_s);
        QCOMPARE(doc.object().value(u"checks"_s).toArray().size(), 8);
    }

    void text()
    {
        const Run process = run({u"--text"_s, u"--sysroot"_s, m_root.path()});
        QCOMPARE(process.exitCode, 2);
        const QString out = QString::fromUtf8(process.out);
        QVERIFY(out.contains(u"Not ready"_s));
        QVERIFY(out.contains(u"[Fail] TPM 2.0: No TPM found"_s));
        QVERIFY(out.contains(u"[Pass] BPF LSM"_s));
    }

    void version()
    {
        const Run process = run({u"--version"_s});
        QCOMPARE(process.exitCode, 0);
        QVERIFY(process.out.startsWith("eac-ready-checker "));
    }
};

QTEST_GUILESS_MAIN(TestCli)
#include "tst_cli.moc"
