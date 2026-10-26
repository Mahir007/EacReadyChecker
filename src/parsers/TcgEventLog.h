// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QUuid>

#include <optional>

// The TCG2 crypto-agile event log in /sys/kernel/security/tpm0/binary_bios_measurements (TCG PC
// Client Platform Firmware Profile, §10). The first event is a SHA-1-format Spec ID event that
// lists the digest banks. Every later event carries one digest per bank.
namespace TcgEventLog
{
// Event types used by the checks (PFP §10.4.1).
enum EventType : quint32 {
    NoAction = 0x3,
    Separator = 0x4,
    EventTag = 0x6,
    Ipl = 0xd,
    EfiVariableDriverConfig = 0x80000001,
    EfiBootServicesApplication = 0x80000003,
    EfiBootServicesDriver = 0x80000004,
    EfiHcrtmEvent = 0x80000010,
    EfiVariableAuthority = 0x800000e0,
};

// TPM_ALG_ID of the digest banks.
enum Algorithm : quint16 {
    Sha1 = 0x4,
    Sha256 = 0xb,
    Sha384 = 0xc,
    Sha512 = 0xd,
};

struct Event {
    quint32 pcr = 0;
    quint32 type = 0;
    QHash<quint16, QByteArray> digests; // TPM_ALG_ID → digest
    QByteArray data;
};

struct Log {
    QList<quint16> algorithms; // banks, in the order the Spec ID event lists them
    QList<Event> events;       // every event after the Spec ID event
};

struct ParseResult {
    enum class Error {
        None,
        NotCryptoAgile, // no Spec ID Event03, e.g. a TPM 1.2 SHA-1 log
        Malformed,
    };
    Error error = Error::None;
    Log log; // only valid when error is None
};

ParseResult parse(const QByteArray &data);

// The value of every PCR the log extends in `algorithm`'s bank, after replaying its events.
// PCR 0 starts at the startup locality (a StartupLocality event, or 4 after an H-CRTM event), the
// rest at zero. nullopt if the bank isn't in the log or an event lacks its digest.
std::optional<QMap<quint32, QByteArray>> replay(const Log &log, quint16 algorithm);

// UEFI_VARIABLE_DATA, the data of EV_EFI_VARIABLE_* events.
struct VariableData {
    QUuid guid;
    QString name;
    QByteArray data;
};
std::optional<VariableData> variableData(const Event &event);

// Text of an EV_IPL event without trailing NULs. GRUB writes ASCII, systemd UTF-16LE.
QString text(const Event &event);

QString algorithmName(quint16 algorithm); // "sha256", or "0x…" for others
} // namespace TcgEventLog
