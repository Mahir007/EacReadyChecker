// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"

#include <QIcon>
#include <QStringList>

// First theme icon that exists, so we look right with Breeze and with other themes. If none does,
// the bundled icon `:/icons/<bundled>.svg` (when given).
inline QIcon themeIcon(const QStringList &names, const QString &bundled = {})
{
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    if (!bundled.isEmpty()) {
        return QIcon(QStringLiteral(":/icons/%1.svg").arg(bundled));
    }
    return QIcon::fromTheme(names.value(0));
}

inline QIcon statusIcon(CheckStatus status)
{
    switch (status) {
    case CheckStatus::Pass:
        return themeIcon({QStringLiteral("emblem-success"), QStringLiteral("emblem-ok-symbolic"), QStringLiteral("emblem-ok")}, QStringLiteral("status-pass"));
    case CheckStatus::Fail:
        return themeIcon({QStringLiteral("emblem-error"), QStringLiteral("dialog-error")}, QStringLiteral("status-fail"));
    case CheckStatus::NotRequired:
        return themeIcon({QStringLiteral("emblem-unavailable"), QStringLiteral("action-unavailable-symbolic")}, QStringLiteral("status-not-required"));
    case CheckStatus::NeedsAdmin:
        return themeIcon({QStringLiteral("emblem-locked"), QStringLiteral("dialog-password")}, QStringLiteral("status-needs-admin"));
    case CheckStatus::Unknown:
        break;
    }
    return themeIcon({QStringLiteral("emblem-question"), QStringLiteral("dialog-question")}, QStringLiteral("status-unknown"));
}

inline QIcon verdictIcon(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ready:
        return themeIcon({QStringLiteral("security-high")}, QStringLiteral("verdict-ready"));
    case Verdict::NotReady:
        return themeIcon({QStringLiteral("security-low")}, QStringLiteral("verdict-not-ready"));
    case Verdict::Incomplete:
        break;
    }
    return themeIcon({QStringLiteral("security-medium")}, QStringLiteral("verdict-incomplete"));
}
