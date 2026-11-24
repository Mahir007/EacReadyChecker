// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

// efivarfs files start with a 4-byte little-endian attribute word, followed by the data.
namespace EfiVariable
{
inline std::optional<QByteArray> data(const QByteArray &file)
{
    if (file.size() < 4) {
        return std::nullopt;
    }
    return file.mid(4);
}

// Single-byte boolean variables such as SecureBoot and SetupMode.
inline std::optional<quint8> byteValue(const QByteArray &file)
{
    const auto d = data(file);
    if (!d || d->size() != 1) {
        return std::nullopt;
    }
    return quint8(d->at(0));
}

// The variable's name from its efivarfs path: "MokListRT1" from
// /sys/firmware/efi/efivars/MokListRT1-605dab50-e046-4300-abb6-3dd810dd8b23.
inline QString nameFromPath(const QString &path)
{
    return path.section(u'/', -1).section(u'-', 0, 0);
}
} // namespace EfiVariable
