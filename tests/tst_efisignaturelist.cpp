// SPDX-License-Identifier: GPL-3.0-or-later
#include "EfiTestData.h"
#include "parsers/EfiSignatureList.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace EfiTestData;

class TestEfiSignatureList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void empty()
    {
        const auto entries = EfiSignatureList::parse({});
        QVERIFY(entries);
        QVERIFY(entries->isEmpty());
    }

    void severalLists()
    {
        const QByteArray hash1(32, 'a');
        const QByteArray hash2(32, 'b');
        const QByteArray data = x509("cert-one") + list(EfiSignatureList::CertSha256, {hash1, hash2}) + x509("cert-two-longer");
        const auto entries = EfiSignatureList::parse(data);
        QVERIFY(entries);
        QCOMPARE(entries->size(), 4);
        QCOMPARE((*entries)[0].type, EfiSignatureList::CertX509);
        QCOMPARE((*entries)[0].owner, ShimLock);
        QCOMPARE((*entries)[0].data, "cert-one");
        QCOMPARE((*entries)[1].type, EfiSignatureList::CertSha256);
        QCOMPARE((*entries)[2].data, hash2);
        QCOMPARE((*entries)[3].data, "cert-two-longer");
    }

    void signatureHeaderSkipped()
    {
        QByteArray data = guid(EfiSignatureList::CertX509) + le32(28 + 3 + 20) + le32(3) + le32(20) + "hdr" + guid(ShimLock) + "body";
        const auto entries = EfiSignatureList::parse(data);
        QVERIFY(entries);
        QCOMPARE(entries->size(), 1);
        QCOMPARE(entries->first().data, "body");
    }

    void emptyList()
    {
        const auto entries = EfiSignatureList::parse(list(EfiSignatureList::CertX509, {}));
        QVERIFY(entries);
        QVERIFY(entries->isEmpty());
    }

    void malformed_data()
    {
        QTest::addColumn<QByteArray>("data");
        const QByteArray type = guid(EfiSignatureList::CertX509);
        QTest::newRow("short header") << type.left(10);
        QTest::newRow("trailing bytes") << x509("cert") + "xyz";
        QTest::newRow("list longer than data") << type + le32(100) + le32(0) + le32(20) + QByteArray(20, 'x');
        QTest::newRow("list shorter than header") << type + le32(20) + le32(0) + le32(20);
        QTest::newRow("header past the list") << type + le32(48) + le32(30) + le32(20) + QByteArray(20, 'x');
        QTest::newRow("signature smaller than owner") << type + le32(28 + 8) + le32(0) + le32(8) + QByteArray(8, 'x');
        QTest::newRow("partial signature") << type + le32(28 + 30) + le32(0) + le32(20) + QByteArray(30, 'x');
        // The size field is 32-bit; make sure a huge value doesn't wrap.
        QTest::newRow("huge list size") << type + le32(0xffffffff) + le32(0) + le32(20) + QByteArray(20, 'x');
        QTest::newRow("huge header size") << type + le32(48) + le32(0xfffffff0) + le32(20) + QByteArray(20, 'x');
    }
    void malformed()
    {
        QFETCH(QByteArray, data);
        QVERIFY(!EfiSignatureList::parse(data));
    }

    void typeNames()
    {
        QCOMPARE(EfiSignatureList::typeName(EfiSignatureList::CertX509), u"x509"_s);
        QCOMPARE(EfiSignatureList::typeName(EfiSignatureList::CertSha256), u"sha256"_s);
        QCOMPARE(EfiSignatureList::typeName(ShimLock), u"605dab50-e046-4300-abb6-3dd810dd8b23"_s);
    }
};

QTEST_GUILESS_MAIN(TestEfiSignatureList)
#include "tst_efisignaturelist.moc"
