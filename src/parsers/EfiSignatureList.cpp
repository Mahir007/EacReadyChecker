// SPDX-License-Identifier: GPL-3.0-or-later
#include "EfiSignatureList.h"

#include <QtEndian>

using namespace Qt::StringLiterals;

namespace
{
constexpr qsizetype kGuidSize = 16;
// SignatureType, SignatureListSize, SignatureHeaderSize, SignatureSize.
constexpr qsizetype kListHeaderSize = kGuidSize + 3 * 4;

// EFI GUIDs store their first three fields little-endian.
QUuid readGuid(const char *p)
{
    const auto *b = reinterpret_cast<const uchar *>(p);
    return QUuid(qFromLittleEndian<quint32>(b), qFromLittleEndian<quint16>(b + 4), qFromLittleEndian<quint16>(b + 6), b[8], b[9], b[10], b[11],
                 b[12], b[13], b[14], b[15]);
}
} // namespace

namespace EfiSignatureList
{
std::optional<QList<Entry>> parse(const QByteArray &data)
{
    QList<Entry> entries;
    qsizetype offset = 0;
    while (offset < data.size()) {
        const qsizetype remaining = data.size() - offset;
        if (remaining < kListHeaderSize) {
            return std::nullopt;
        }
        const char *list = data.constData() + offset;
        const QUuid type = readGuid(list);
        const quint32 listSize = qFromLittleEndian<quint32>(list + kGuidSize);
        const quint32 headerSize = qFromLittleEndian<quint32>(list + kGuidSize + 4);
        const quint32 signatureSize = qFromLittleEndian<quint32>(list + kGuidSize + 8);
        // The same checks as the kernel's parse_efi_signature_list().
        if (listSize < kListHeaderSize || listSize > quint64(remaining) || headerSize > listSize - kListHeaderSize
            || signatureSize < kGuidSize || (listSize - kListHeaderSize - headerSize) % signatureSize != 0) {
            return std::nullopt;
        }
        for (qsizetype p = kListHeaderSize + headerSize; p < qsizetype(listSize); p += signatureSize) {
            entries.append({type, readGuid(list + p), QByteArray(list + p + kGuidSize, signatureSize - kGuidSize)});
        }
        offset += listSize;
    }
    return entries;
}

QString typeName(const QUuid &type)
{
    if (type == CertX509) {
        return u"x509"_s;
    }
    if (type == CertSha256) {
        return u"sha256"_s;
    }
    return type.toString(QUuid::WithoutBraces);
}
} // namespace EfiSignatureList
