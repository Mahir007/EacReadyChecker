// SPDX-License-Identifier: GPL-3.0-or-later
#include "parsers/KernelCmdline.h"

#include <QTest>

using namespace Qt::StringLiterals;

class TestKernelCmdline : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tokenising()
    {
        const KernelCmdline cmdline = KernelCmdline::parse("BOOT_IMAGE=/vmlinuz  quiet\tro root=UUID=ab=cd\n");
        QCOMPARE(cmdline.params().size(), 4);
        QCOMPARE(cmdline.params()[1].name, u"quiet"_s);
        QVERIFY(!cmdline.params()[1].value);
        QCOMPARE(cmdline.values(u"root"), QStringList{u"UUID=ab=cd"_s});
    }

    void quotes()
    {
        const KernelCmdline cmdline = KernelCmdline::parse(R"(dyndbg="file foo.c +p" "lockdown=integrity" x)");
        QCOMPARE(cmdline.values(u"dyndbg"), QStringList{u"file foo.c +p"_s});
        QCOMPARE(cmdline.values(u"lockdown"), QStringList{u"integrity"_s});
        QVERIFY(cmdline.contains(u"x"));
    }

    void initArgumentsIgnored()
    {
        const KernelCmdline cmdline = KernelCmdline::parse("ro -- iommu=pt single");
        QVERIFY(!cmdline.contains(u"iommu"));
        QVERIFY(!cmdline.contains(u"single"));
    }

    void dashesAndUnderscores()
    {
        const KernelCmdline cmdline = KernelCmdline::parse("nvidia-drm.modeset=1 module_blacklist=a");
        QCOMPARE(cmdline.values(u"nvidia_drm.modeset"), QStringList{u"1"_s});
        QVERIFY(cmdline.contains(u"module-blacklist"));
    }

    void repeatedValuesInOrder() { QCOMPARE(KernelCmdline::parse("iommu=pt iommu=nopt").values(u"iommu"), (QStringList{u"pt"_s, u"nopt"_s})); }

    void parseBool_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<int>("expected"); // -1 = invalid
        QTest::newRow("1") << u"1"_s << 1;
        QTest::newRow("y") << u"y"_s << 1;
        QTest::newRow("Yes") << u"Yes"_s << 1;
        QTest::newRow("on") << u"on"_s << 1;
        QTest::newRow("0") << u"0"_s << 0;
        QTest::newRow("n") << u"n"_s << 0;
        QTest::newRow("off") << u"off"_s << 0;
        QTest::newRow("o") << u"o"_s << -1;
        QTest::newRow("empty") << QString() << -1;
        QTest::newRow("junk") << u"maybe"_s << -1;
    }
    void parseBool()
    {
        QFETCH(QString, value);
        QFETCH(int, expected);
        const auto parsed = KernelCmdline::parseBool(value);
        QCOMPARE(parsed ? int(*parsed) : -1, expected);
    }
};

QTEST_GUILESS_MAIN(TestKernelCmdline)
#include "tst_kernelcmdline.moc"
