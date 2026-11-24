// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Desktop.h"
#include "core/AppInfo.h"
#include "core/CheckResult.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QStringList>
#include <QStyle>

inline bool isDarkPalette(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
}

// GNOME's Adwaita and Ubuntu's Yaru, in any of its accent colours (Yaru-blue, Yaru-blue-dark...).
// Both have old full-colour icons for the buttons, which look out of place.
inline bool isGnomeIconTheme(const QString &theme)
{
    return theme.compare(QStringLiteral("Adwaita"), Qt::CaseInsensitive) == 0 || theme.compare(QStringLiteral("Yaru"), Qt::CaseInsensitive) == 0
           || theme.startsWith(QStringLiteral("Yaru-"), Qt::CaseInsensitive);
}

inline bool isBreezeInstalled()
{
    const QStringList paths = QIcon::themeSearchPaths();
    for (const QString &path : paths) {
        if (QFile::exists(path + QStringLiteral("/breeze/index.theme"))) {
            return true;
        }
    }
    return false;
}

// Icons the current theme lacks come from Breeze. Breeze's glyphs are drawn for one background, so
// use Breeze Dark when the palette has light text on a dark window. On Plasma, KDE's platform theme
// recolours them instead, and the style is Breeze. Without it, as in the AppImage, the style is
// Fusion and Breeze is switched here too. Adwaita and Yaru are replaced outright: Adwaita inherits
// AdwaitaLegacy's old full-colour icons, and Yaru has its own.
// Call it before (re)loading icons. It only changes anything when the palette has changed.
inline void syncIconThemes()
{
    const QString name = isDarkPalette(QGuiApplication::palette()) ? QStringLiteral("breeze-dark") : QStringLiteral("breeze");
    if (QIcon::fallbackThemeName() != name) {
        QIcon::setFallbackThemeName(name);
    }

    static bool replaced = false;
    const QString theme = QIcon::themeName();
    if (isGnomeIconTheme(theme)) {
        if (!isBreezeInstalled()) {
            return;
        }
        replaced = true;
    }
    const bool breeze = theme.startsWith(QStringLiteral("breeze")) && (replaced || !isBreezeStyle());
    if (theme != name && (isGnomeIconTheme(theme) || breeze)) {
        QIcon::setThemeName(name);
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
// or the theme is still Adwaita or Yaru because Breeze isn't installed, the bundled icon
// `:/icons/<bundled>.svg` (when given).
inline QIcon themeIcon(const QStringList &names, const QString &bundled = {})
{
    const QIcon bundledIcon = bundled.isEmpty() ? QIcon() : QIcon(QStringLiteral(":/icons/%1.svg").arg(bundled));
    if (!bundledIcon.isNull() && isGnomeIconTheme(QIcon::themeName())) {
        return bundledIcon;
    }
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    return bundledIcon.isNull() ? QIcon::fromTheme(names.value(0)) : bundledIcon;
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
