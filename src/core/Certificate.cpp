// SPDX-License-Identifier: GPL-3.0-or-later
#include "Certificate.h"

#include <QCryptographicHash>
#include <QSslCertificate>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace Certificate
{
QByteArray sha256(const QByteArray &der)
{
    return QCryptographicHash::hash(der, QCryptographicHash::Sha256);
}

QString fingerprint(const QByteArray &sha256)
{
    return QString::fromLatin1(sha256.toHex(':').toUpper());
}

bool isDer(const QByteArray &data)
{
    // Tag 0x30, then a short-form length or a long form of up to four length bytes.
    if (data.size() < 2 || quint8(data.at(0)) != 0x30) {
        return false;
    }
    const quint8 first = quint8(data.at(1));
    qsizetype header = 2;
    quint64 length = first;
    if (first & 0x80) {
        const int lengthBytes = first & 0x7f;
        if (lengthBytes == 0 || lengthBytes > 4 || data.size() < 2 + lengthBytes) {
            return false;
        }
        length = 0;
        for (int i = 0; i < lengthBytes; ++i) {
            length = (length << 8) | quint8(data.at(2 + i));
        }
        header += lengthBytes;
    }
    return quint64(data.size()) == header + length;
}

QString subject(const QByteArray &der)
{
    const QSslCertificate certificate(der, QSsl::Der);
    if (certificate.isNull()) {
        return {};
    }
    const std::pair<QSslCertificate::SubjectInfo, QString> fields[] = {
        {QSslCertificate::CommonName, u"CN"_s},
        {QSslCertificate::OrganizationalUnitName, u"OU"_s},
        {QSslCertificate::Organization, u"O"_s},
    };
    QStringList parts;
    for (const auto &[info, name] : fields) {
        const QStringList values = certificate.subjectInfo(info);
        for (const QString &value : values) {
            parts.append(name + u'=' + value);
        }
    }
    return parts.join(u", "_s);
}
} // namespace Certificate
