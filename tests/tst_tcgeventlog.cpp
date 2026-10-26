// SPDX-License-Identifier: GPL-3.0-or-later
#include "TcgTestData.h"
#include "parsers/TcgEventLog.h"

#include <QFile>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace TcgTestData;
using Error = TcgEventLog::ParseResult::Error;

class TestTcgEventLog : public QObject
{
    Q_OBJECT

private:
    static QByteArray fixtureFile(const QString &path)
    {
        QFile file(QStringLiteral(EACHECK_FIXTURES_DIR) + u"/fedora44-vmware"_s + path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private Q_SLOTS:
    // The Fedora 44 VM's log: three banks, an H-CRTM event, and PCR values captured with it.
    void realLog()
    {
        const auto parsed = TcgEventLog::parse(fixtureFile(u"/sys/kernel/security/tpm0/binary_bios_measurements"_s));
        QCOMPARE(parsed.error, Error::None);
        QCOMPARE(parsed.log.algorithms, (QList<quint16>{TcgEventLog::Sha256, TcgEventLog::Sha384, TcgEventLog::Sha512}));
        QCOMPARE(parsed.log.events.size(), 111);
        QCOMPARE(parsed.log.events[0].type, TcgEventLog::EfiHcrtmEvent);

        const auto pcrs = TcgEventLog::replay(parsed.log, TcgEventLog::Sha256);
        QVERIFY(pcrs);
        QCOMPARE(pcrs->keys(), (QList<quint32>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14}));
        for (auto it = pcrs->begin(); it != pcrs->end(); ++it) {
            const QByteArray tpm = fixtureFile(u"/sys/class/tpm/tpm0/pcr-sha256/%1"_s.arg(it.key())).trimmed();
            QCOMPARE(it.value().toHex().toUpper(), tpm);
        }
        QVERIFY(TcgEventLog::replay(parsed.log, TcgEventLog::Sha384));
        QVERIFY(!TcgEventLog::replay(parsed.log, TcgEventLog::Sha1)); // not a bank in this log
    }

    void startupLocality()
    {
        const Event locality{0, TcgEventLog::NoAction, QByteArray("StartupLocality\0\x03", 17)};
        const Event crtm{0, 0x8, "crtm"};
        const auto parsed = TcgEventLog::parse(log({locality, crtm}));
        QCOMPARE(parsed.error, Error::None);
        QByteArray start(32, '\0');
        start[31] = 3;
        const QByteArray expected = QCryptographicHash::hash(QByteArray(start + QCryptographicHash::hash("crtm", QCryptographicHash::Sha256)), QCryptographicHash::Sha256);
        QCOMPARE(TcgEventLog::replay(parsed.log, TcgEventLog::Sha256)->value(0), expected);
    }

    void twoBanks()
    {
        const QList<Event> events{{4, TcgEventLog::EfiBootServicesApplication, "app"}, {4, TcgEventLog::Separator, "sep"}};
        const auto parsed = TcgEventLog::parse(log(events, {TcgEventLog::Sha1, TcgEventLog::Sha256}));
        QCOMPARE(parsed.error, Error::None);
        QCOMPARE(parsed.log.events.size(), 2);
        QCOMPARE(parsed.log.events[1].data, "sep");
        QCOMPARE(TcgEventLog::replay(parsed.log, TcgEventLog::Sha1)->value(4).size(), 20);
        QCOMPARE(TcgEventLog::replay(parsed.log, TcgEventLog::Sha256)->value(4).toHex().toUpper() + '\n', pcrFiles(events).value(4));
    }

    void notCryptoAgile()
    {
        // A TPM 1.2 log starts straight away with a measured SHA-1 event.
        const QByteArray legacy = EfiTestData::le32(0) + EfiTestData::le32(0x8) + QByteArray(20, 'x') + EfiTestData::le32(4) + "crtm";
        QCOMPARE(TcgEventLog::parse(legacy).error, Error::NotCryptoAgile);
    }

    void malformed_data()
    {
        QTest::addColumn<QByteArray>("data");
        const QByteArray good = log({{4, TcgEventLog::Separator, "sep"}});
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("truncated") << good.chopped(1);
        QTest::newRow("trailing byte") << good + 'x';
        QTest::newRow("spec id only partly") << specIdEvent({TcgEventLog::Sha256}).chopped(3);
        {
            // A digest of an algorithm the Spec ID event didn't list.
            QByteArray bad = specIdEvent({TcgEventLog::Sha256}) + EfiTestData::le32(4) + EfiTestData::le32(4) + EfiTestData::le32(1)
                + le16(TcgEventLog::Sha1) + QByteArray(20, '\0') + EfiTestData::le32(0);
            QTest::newRow("unlisted algorithm") << bad;
        }
        QTest::newRow("no algorithms") << specIdEvent({});
    }
    void malformed()
    {
        QFETCH(QByteArray, data);
        QCOMPARE(TcgEventLog::parse(data).error, Error::Malformed);
    }

    void variableData()
    {
        const TcgEventLog::Event event{7, TcgEventLog::EfiVariableAuthority, {}, variable(EfiTestData::ShimLock, u"MokListRT"_s, "cert")};
        const auto data = TcgEventLog::variableData(event);
        QVERIFY(data);
        QCOMPARE(data->guid, EfiTestData::ShimLock);
        QCOMPARE(data->name, u"MokListRT"_s);
        QCOMPARE(data->data, "cert");

        const TcgEventLog::Event truncated{7, TcgEventLog::EfiVariableAuthority, {}, event.data.chopped(1)};
        QVERIFY(!TcgEventLog::variableData(truncated));
        // A name length that would overflow when doubled.
        const TcgEventLog::Event huge{7, TcgEventLog::EfiVariableAuthority, {}, EfiTestData::guid(EfiTestData::ShimLock) + le64(~quint64(0) / 2 + 1) + le64(0)};
        QVERIFY(!TcgEventLog::variableData(huge));
    }

    void text()
    {
        QCOMPARE(TcgEventLog::text({8, TcgEventLog::Ipl, {}, ascii("kernel_cmdline: /vmlinuz ro")}), u"kernel_cmdline: /vmlinuz ro"_s);
        QCOMPARE(TcgEventLog::text({11, TcgEventLog::Ipl, {}, utf16(u".linux"_s)}), u".linux"_s);
        QCOMPARE(TcgEventLog::text({9, TcgEventLog::Ipl, {}, "/boot/vmlinuz"}), u"/boot/vmlinuz"_s);
    }
};

QTEST_GUILESS_MAIN(TestTcgEventLog)
#include "tst_tcgeventlog.moc"
