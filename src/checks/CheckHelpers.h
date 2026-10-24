// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"
#include "core/HostSnapshot.h"

#include <QStringList>

namespace CheckHelpers
{
// Sets the check's status from its sub-checks. The detail is `passDetail` when it passes,
// otherwise the detail of the first sub-check that decided the status.
inline void finishFromSubChecks(CheckResult &result, const QString &passDetail)
{
    result.status = combineStatus(result.subChecks);
    if (result.status == CheckStatus::Pass || result.status == CheckStatus::NotRequired) {
        result.detail = passDetail;
        return;
    }
    for (const SubCheckResult &sub : std::as_const(result.subChecks)) {
        if (sub.status == result.status) {
            result.detail = sub.detail;
            return;
        }
    }
}

// A sub-check a later phase implements. Shows Needs admin until the privileged read is granted.
inline SubCheckResult notImplemented(const QString &title, const HostSnapshot &snapshot, bool needsAdmin)
{
    if (needsAdmin && snapshot.privilegedState != PrivilegedState::Granted) {
        return {title, CheckStatus::NeedsAdmin, QStringLiteral("Needs admin access to read the TPM event log")};
    }
    return {title, CheckStatus::Unknown, QStringLiteral("Not implemented yet")};
}

// "Couldn't read <path>" for entries that are unreadable or weren't read at all.
inline QString unreadable(const HostEntry &entry)
{
    return QStringLiteral("Couldn't read %1").arg(entry.path);
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
