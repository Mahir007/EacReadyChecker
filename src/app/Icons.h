// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/AppInfo.h"
#include "core/CheckResult.h"

#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QStringList>

// Icons the current theme lacks (e.g. Adwaita on GNOME) come from Breeze. Breeze's glyphs are
// drawn for one background, so use Breeze Dark when the palette has light text on a dark window.
// Call it before (re)loading icons. It only changes anything when the palette has changed.
inline void syncFallbackIconTheme()
{
    const QPalette palette = QGuiApplication::palette();
    const bool dark = palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
    const QString name = dark ? QStringLiteral("breeze-dark") : QStringLiteral("breeze");
    if (QIcon::fallbackThemeName() != name) {
        QIcon::setFallbackThemeName(name);
    }
}

// True for changes after which icons need loading again: the colour scheme (palette), the icon
// theme or the style (icon sizes). Widgets that keep pixmaps in labels redraw them.
inline bool affectsIcons(const QEvent *event)
{
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
    case QEvent::StyleChange:
        return true;
    default:
        return false;
    }
}

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

// The installed icon (from the .desktop file's theme lookup), or the one bundled in the app.
inline QIcon appIcon()
{
    return QIcon::fromTheme(AppInfo::id(), QIcon(QStringLiteral(":/icons/%1.svg").arg(AppInfo::id())));
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
