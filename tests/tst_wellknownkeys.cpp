// SPDX-License-Identifier: GPL-3.0-or-later
#include "EfiTestData.h"
#include "core/Certificate.h"
#include "core/WellKnownKeys.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{
const QString kCanonical = u"ED:1F:E7:2C:B9:CA:31:C9:AF:5B:75:7A:FC:D7:33:32:3D:67:58:25:03:2E:6C:ED:7F:E1:AE:9E:B7:67:99:8C"_s;

QJsonObject key(const QString &vendor, const QString &subject, const QString &sha256)
{
    QJsonObject entry{{u"sha256"_s, sha256}};
    if (!vendor.isEmpty()) {
        entry.insert(u"vendor"_s, vendor);
    }
    if (!subject.isEmpty()) {
        entry.insert(u"subject"_s, subject);
    }
    return entry;
}

QByteArray keysJson(const QJsonArray &keys)
{
    return QJsonDocument(QJsonObject{{u"keys"_s, keys}}).toJson();
}
} // namespace

class TestWellKnownKeys : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void builtInList()
    {
        const QList<WellKnownKey> &keys = WellKnownKeys::builtIn().keys();
        QVERIFY(!keys.isEmpty());

        // Every vendor Epic names has at least one certificate, and no other vendor appears.
        const QSet<QString> published{
            u"Canonical Ltd. Master Certificate Authority"_s,
            u"Fedora Secure Boot CA"_s,
            u"Universal Blue"_s,
            u"Debian Secure Boot CA"_s,
            u"openSUSE Secure Boot CA"_s,
            u"SUSE Linux Enterprise Secure Boot CA"_s,
            u"Rocky Linux Secure Boot Root CA"_s,
            u"Microsoft UEFI CA"_s,
            u"AlmaLinux Secure Boot CA"_s,
        };
        QSet<QString> vendors;
        for (const WellKnownKey &key : keys) {
            vendors.insert(key.vendor);
        }
        QCOMPARE(vendors, published);
    }

    void matchesByFingerprint()
    {
        const WellKnownKey *canonical = WellKnownKeys::builtIn().findCertificate(EfiTestData::canonicalCertificate());
        QVERIFY(canonical);
        QCOMPARE(canonical->vendor, u"Canonical Ltd. Master Certificate Authority"_s);
        QVERIFY(!WellKnownKeys::builtIn().findCertificate(EfiTestData::unknownCertificate()));
        QVERIFY(!WellKnownKeys::builtIn().findCertificate({}));
    }

    void certificateHelpers()
    {
        const QByteArray der = EfiTestData::canonicalCertificate();
        QCOMPARE(Certificate::fingerprint(Certificate::sha256(der)), kCanonical);
        QCOMPARE(Certificate::subject(der), u"CN=Canonical Ltd. Master Certificate Authority, O=Canonical Ltd."_s);
        QCOMPARE(Certificate::subject("not a certificate"), QString());

        QVERIFY(Certificate::isDer(der));
        QVERIFY(!Certificate::isDer(der + '\0'));
        QVERIFY(!Certificate::isDer(der.chopped(1)));
        QVERIFY(!Certificate::isDer(QByteArray(16, '\0') + der)); // EFI_SIGNATURE_DATA: owner GUID first
        QVERIFY(Certificate::isDer(QByteArray("\x30\x01\x00", 3)));
        QVERIFY(!Certificate::isDer(QByteArray("\x30\x80", 2))); // indefinite length isn't DER
        QVERIFY(!Certificate::isDer({}));
    }

    void fromJson()
    {
        const auto keys = WellKnownKeys::fromJson(keysJson({key(u"V"_s, u"CN=a"_s, kCanonical)}));
        QVERIFY(keys);
        QCOMPARE(keys->keys().size(), 1);
        QVERIFY(keys->findCertificate(EfiTestData::canonicalCertificate()));
    }

    void fromJsonRejects_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("not json") << QByteArray("{");
        QTest::newRow("no keys") << QJsonDocument(QJsonObject{{u"list"_s, QJsonArray()}}).toJson();
        QTest::newRow("no vendor") << keysJson({key(QString(), u"CN=a"_s, kCanonical)});
        QTest::newRow("no subject") << keysJson({key(u"V"_s, QString(), kCanonical)});
        QTest::newRow("short fingerprint") << keysJson({key(u"V"_s, u"CN=a"_s, u"ED:1F"_s)});
        QTest::newRow("not hex") << keysJson({key(u"V"_s, u"CN=a"_s, kCanonical.chopped(2) + u"ZZ"_s)});
        QTest::newRow("duplicate") << keysJson({key(u"V"_s, u"CN=a"_s, kCanonical), key(u"W"_s, u"CN=b"_s, kCanonical)});
    }
    void fromJsonRejects()
    {
        QFETCH(QByteArray, json);
        QString error;
        QVERIFY(!WellKnownKeys::fromJson(json, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestWellKnownKeys)
#include "tst_wellknownkeys.moc"
