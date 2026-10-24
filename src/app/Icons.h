// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"

#include <QIcon>
#include <QStringList>

// First theme icon that exists, so we look right with Breeze and with other themes.
inline QIcon themeIcon(const QStringList &names)
{
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    return QIcon::fromTheme(names.value(0));
}

inline QIcon statusIcon(CheckStatus status)
{
    switch (status) {
    case CheckStatus::Pass:
        return themeIcon({QStringLiteral("emblem-success"), QStringLiteral("emblem-ok-symbolic"), QStringLiteral("emblem-ok")});
    case CheckStatus::Fail:
        return themeIcon({QStringLiteral("emblem-error"), QStringLiteral("dialog-error")});
    case CheckStatus::NotRequired:
        return themeIcon({QStringLiteral("emblem-unavailable"), QStringLiteral("action-unavailable-symbolic")});
    case CheckStatus::NeedsAdmin:
        return themeIcon({QStringLiteral("emblem-locked"), QStringLiteral("dialog-password")});
    case CheckStatus::Unknown:
        break;
    }
    return themeIcon({QStringLiteral("emblem-question"), QStringLiteral("dialog-question")});
}

inline QIcon verdictIcon(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ready:
        return themeIcon({QStringLiteral("security-high"), QStringLiteral("emblem-ok")});
    case Verdict::NotReady:
        return themeIcon({QStringLiteral("security-low"), QStringLiteral("dialog-error")});
    case Verdict::Incomplete:
        break;
    }
    return themeIcon({QStringLiteral("security-medium"), QStringLiteral("dialog-warning")});
}
