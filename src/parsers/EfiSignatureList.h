// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QUuid>

#include <optional>

// EFI_SIGNATURE_LIST, as stored in db, dbx and shim's MokListRT (section 32.4.1 of the UEFI spec). A variable
// holds any number of lists back to back. Each list has one signature type and fixed-size entries.
namespace EfiSignatureList
{
inline const QUuid CertX509{0xa5c059a1, 0x94e4, 0x4aa7, 0x87, 0xb5, 0xab, 0x15, 0x5c, 0x2b, 0xf0, 0x72};
inline const QUuid CertSha256{0xc1c41626, 0x504c, 0x4092, 0xac, 0xa9, 0x41, 0xf9, 0x36, 0x93, 0x43, 0x28};

struct Entry {
    QUuid type;  // signature type of the list it came from, such as CertX509
    QUuid owner; // SignatureOwner
    QByteArray data;
};

// Every entry of every list, in order. nullopt if a list header doesn't add up. Takes the variable's
// data without the efivarfs attribute word.
std::optional<QList<Entry>> parse(const QByteArray &data);

// "x509", "sha256", or the GUID for other types.
QString typeName(const QUuid &type);
} // namespace EfiSignatureList
