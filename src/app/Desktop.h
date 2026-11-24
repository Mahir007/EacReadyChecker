// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QApplication>
#include <QStringList>
#include <QStyle>
#include <QWidget>

inline bool isDesktop(QStringView name)
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':').contains(name, Qt::CaseInsensitive);
}

inline bool isGnome()
{
    return isDesktop(u"GNOME");
}

inline bool isKde()
{
    return isDesktop(u"KDE");
}

// True if `widget` is drawn with the style called `name`, such as "fusion".
inline bool hasStyle(const QWidget *widget, QStringView name)
{
    return widget->style()->name().compare(name, Qt::CaseInsensitive) == 0;
}

// True if the app's style is Breeze, as on Plasma with KDE's platform theme.
inline bool isBreezeStyle()
{
    const QStyle *style = QApplication::style();
    return style && style->name().compare(u"breeze", Qt::CaseInsensitive) == 0;
}

// Plasma with Fusion instead of Breeze, as in the AppImage, which has neither KDE's platform theme
// nor the Breeze style. The app draws some of Breeze's details itself there.
inline bool isFusionOnPlasma(const QWidget *widget)
{
    return isKde() && hasStyle(widget, u"fusion");
}

// The margin of a layout on `widget`. Under Fusion on Plasma it's Breeze's: 10 px around a window's
// contents and 6 px inside it, where Fusion has 9 for both.
inline int layoutMargin(const QWidget *widget)
{
    if (isFusionOnPlasma(widget)) {
        return widget->isWindow() ? 10 : 6;
    }
    return widget->style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, widget);
}
