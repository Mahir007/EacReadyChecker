// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"
#include "core/HostSnapshot.h"

#include <QStringList>

namespace CheckHelpers
{
// Sets the check's status from its sub-checks. A passing check has no detail, since its icon and
// title already say it. Otherwise the detail is that of the first sub-check that decided the status.
inline void finishFromSubChecks(CheckResult &result)
{
    result.status = combineStatus(result.subChecks);
    if (result.status == CheckStatus::Pass || result.status == CheckStatus::NotRequired) {
        return;
    }
    for (const SubCheckResult &sub : std::as_const(result.subChecks)) {
        if (sub.status == result.status) {
            result.detail = sub.detail;
            return;
        }
    }
}

// "Couldn't read <path>" for entries that are unreadable or weren't read at all.
inline QString unreadable(const HostEntry &entry)
{
    return QStringLiteral("Couldn't read %1").arg(entry.path);
}

// "Unrecognised key: <first>" or "Unrecognised keys: <first> (+n more)". `keys` must not be empty.
inline QString unrecognisedKeys(const QStringList &keys)
{
    if (keys.size() == 1) {
        return QStringLiteral("Unrecognised key: %1").arg(keys.first());
    }
    return QStringLiteral("Unrecognised keys: %1 (+%2 more)").arg(keys.first()).arg(keys.size() - 1);
}

// Comma-separated list from a sysfs file such as /sys/kernel/security/lsm.
inline QStringList commaList(const HostEntry &entry)
{
    QStringList items;
    const QList<QByteArray> parts = entry.trimmed().split(',');
    for (const QByteArray &part : parts) {
        if (!part.trimmed().isEmpty()) {
            items.append(QString::fromUtf8(part.trimmed()));
        }
    }
    return items;
}
} // namespace CheckHelpers
