// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "EfiTestData.h"
#include "parsers/TcgEventLog.h"

#include <QCryptographicHash>
#include <QMap>
#include <QtEndian>

// Builds TCG2 crypto-agile event logs for tests. Each event's digest is the hash of its data.
namespace TcgTestData
{
struct Event {
    quint32 pcr = 0;
    quint32 type = 0;
    QByteArray data;
};

inline QByteArray le16(quint16 value)
{
    QByteArray bytes(2, '\0');
    qToLittleEndian(value, bytes.data());
    return bytes;
}

inline QByteArray le64(quint64 value)
{
    QByteArray bytes(8, '\0');
    qToLittleEndian(value, bytes.data());
    return bytes;
}

inline QCryptographicHash::Algorithm hashFor(quint16 algorithm)
{
    return algorithm == TcgEventLog::Sha1 ? QCryptographicHash::Sha1 : QCryptographicHash::Sha256;
}

// The SHA-1-format Spec ID event that starts every crypto-agile log.
inline QByteArray specIdEvent(const QList<quint16> &algorithms)
{
    QByteArray spec = QByteArray("Spec ID Event03\0", 16) + EfiTestData::le32(0) + QByteArray("\x00\x02\x00\x02", 4) + EfiTestData::le32(algorithms.size());
    for (quint16 algorithm : algorithms) {
        spec += le16(algorithm) + le16(QCryptographicHash::hashLength(hashFor(algorithm)));
    }
    spec += '\0'; // vendorInfoSize
    return EfiTestData::le32(0) + EfiTestData::le32(TcgEventLog::NoAction) + QByteArray(20, '\0') + EfiTestData::le32(spec.size()) + spec;
}

inline QByteArray digest(const Event &event, quint16 algorithm)
{
    // EV_NO_ACTION events carry zero digests.
    if (event.type == TcgEventLog::NoAction) {
        return QByteArray(QCryptographicHash::hashLength(hashFor(algorithm)), '\0');
    }
    return QCryptographicHash::hash(event.data, hashFor(algorithm));
}

inline QByteArray log(const QList<Event> &events, const QList<quint16> &algorithms = {TcgEventLog::Sha256})
{
    QByteArray out = specIdEvent(algorithms);
    for (const Event &event : events) {
        out += EfiTestData::le32(event.pcr) + EfiTestData::le32(event.type) + EfiTestData::le32(algorithms.size());
        for (quint16 algorithm : algorithms) {
            out += le16(algorithm) + digest(event, algorithm);
        }
        out += EfiTestData::le32(event.data.size()) + event.data;
    }
    return out;
}

// SHA-256 PCR values after `events`, starting from zero, as /sys/class/tpm/tpm0/pcr-sha256/<n> shows them.
inline QMap<quint32, QByteArray> pcrFiles(const QList<Event> &events)
{
    QMap<quint32, QByteArray> pcrs;
    for (const Event &event : events) {
        if (event.type == TcgEventLog::NoAction) {
            continue;
        }
        const QByteArray current = pcrs.value(event.pcr, QByteArray(32, '\0'));
        pcrs.insert(event.pcr, QCryptographicHash::hash(QByteArray(current + digest(event, TcgEventLog::Sha256)), QCryptographicHash::Sha256));
    }
    for (QByteArray &value : pcrs) {
        value = value.toHex().toUpper() + '\n';
    }
    return pcrs;
}

// UEFI_VARIABLE_DATA, as in EV_EFI_VARIABLE_* events.
inline QByteArray variable(const QUuid &guid, const QString &name, const QByteArray &data)
{
    const QByteArray utf16(reinterpret_cast<const char *>(name.utf16()), name.size() * 2);
    return EfiTestData::guid(guid) + le64(name.size()) + le64(data.size()) + utf16 + data;
}

// A PCR 7 authority event holding an EFI_SIGNATURE_DATA with `der`.
inline Event authority(const QString &name, const QByteArray &der)
{
    return {7, TcgEventLog::EfiVariableAuthority, variable(EfiTestData::ShimLock, name, EfiTestData::guid(EfiTestData::ShimLock) + der)};
}

inline QByteArray ascii(const char *text)
{
    return QByteArray(text) + '\0';
}

inline QByteArray utf16(const QString &text)
{
    return QByteArray(reinterpret_cast<const char *>(text.utf16()), (text.size() + 1) * 2);
}
} // namespace TcgTestData
