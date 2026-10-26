// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QUuid>
#include <QtEndian>

// EFI GUIDs store their first three fields little-endian. `p` must point at 16 bytes.
inline QUuid readEfiGuid(const char *p)
{
    const auto *b = reinterpret_cast<const uchar *>(p);
    return QUuid(qFromLittleEndian<quint32>(b), qFromLittleEndian<quint16>(b + 4), qFromLittleEndian<quint16>(b + 6), b[8], b[9], b[10], b[11],
                 b[12], b[13], b[14], b[15]);
}
