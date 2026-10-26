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
