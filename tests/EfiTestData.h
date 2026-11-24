// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "parsers/EfiSignatureList.h"

#include <QFile>
#include <QUuid>
#include <QtEndian>

#include <algorithm>

// Builds EFI signature lists and efivarfs files for tests.
namespace EfiTestData
{
// shim's GUID, which it uses as the owner of MokListRT entries.
inline const QUuid ShimLock{0x605dab50, 0xe046, 0x4300, 0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23};

inline QByteArray guid(const QUuid &uuid)
{
    QByteArray bytes = uuid.toRfc4122();
    std::reverse(bytes.begin(), bytes.begin() + 4);
    std::reverse(bytes.begin() + 4, bytes.begin() + 6);
    std::reverse(bytes.begin() + 6, bytes.begin() + 8);
    return bytes;
}

inline QByteArray le32(quint32 value)
{
    QByteArray bytes(4, '\0');
    qToLittleEndian(value, bytes.data());
    return bytes;
}

// One list of `type` holding `bodies`, which must all be the same size.
inline QByteArray list(const QUuid &type, const QList<QByteArray> &bodies, const QUuid &owner = ShimLock)
{
    const quint32 signatureSize = 16 + (bodies.isEmpty() ? 0 : bodies.first().size());
    QByteArray out = guid(type) + le32(28 + signatureSize * bodies.size()) + le32(0) + le32(signatureSize);
    for (const QByteArray &body : bodies) {
        out += guid(owner) + body;
    }
    return out;
}

inline QByteArray x509(const QByteArray &der)
{
    return list(EfiSignatureList::CertX509, {der});
}

// An efivarfs file: attribute word, then the data.
inline QByteArray variable(const QByteArray &data)
{
    return QByteArray("\x06\x00\x00\x00", 4) + data;
}

inline QByteArray testFile(const QString &name)
{
    QFile file(QStringLiteral(EACHECK_TEST_DATA_DIR) + u'/' + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A certificate that isn't on the list: CN=EACheck test MOK, O=EACheck tests.
inline QByteArray unknownCertificate()
{
    return testFile(QStringLiteral("test-mok.der"));
}

// On the list, from Ubuntu's shim.
inline QByteArray canonicalCertificate()
{
    return testFile(QStringLiteral("canonical-master-ca.der"));
}
} // namespace EfiTestData
