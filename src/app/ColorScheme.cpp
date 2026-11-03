// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColorScheme.h"

#include "Icons.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

using namespace Qt::StringLiterals;

namespace
{
bool s_darkPaletteSet = false;

// libadwaita's dark colours, so the window matches its title bar.
QPalette darkPalette()
{
    const QColor text(0xff, 0xff, 0xff);
    const QColor dimmed(0xa8, 0xa8, 0xa8);
    const QColor disabled(0x80, 0x80, 0x80);

    QPalette p;
    p.setColor(QPalette::Window, QColor(0x24, 0x24, 0x24));
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
    p.setColor(QPalette::AlternateBase, QColor(0x2a, 0x2a, 0x2a));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x36, 0x36, 0x36));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, text);
    p.setColor(QPalette::PlaceholderText, dimmed);
    p.setColor(QPalette::ToolTipBase, QColor(0x38, 0x38, 0x38));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, QColor(0x35, 0x84, 0xe4));
    p.setColor(QPalette::HighlightedText, text);
    p.setColor(QPalette::Link, QColor(0x78, 0xae, 0xed));
    p.setColor(QPalette::LinkVisited, QColor(0xc0, 0x61, 0xcb));
    p.setColor(QPalette::Light, QColor(0x50, 0x50, 0x50));
    p.setColor(QPalette::Midlight, QColor(0x42, 0x42, 0x42));
    p.setColor(QPalette::Mid, QColor(0x2e, 0x2e, 0x2e));
    p.setColor(QPalette::Dark, QColor(0x1a, 0x1a, 0x1a));
    p.setColor(QPalette::Shadow, QColor(0x00, 0x00, 0x00));
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::PlaceholderText}) {
        p.setColor(QPalette::Disabled, role, disabled);
    }
    return p;
}
} // namespace

void applyColorScheme(Qt::ColorScheme scheme)
{
    if (scheme == Qt::ColorScheme::Dark) {
        if (!isDarkPalette(QGuiApplication::palette())) {
            QApplication::setPalette(darkPalette());
            s_darkPaletteSet = true;
        }
    } else if (s_darkPaletteSet) {
        QApplication::setPalette(QPalette());
        s_darkPaletteSet = false;
    }
}

void followDesktopColorScheme()
{
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':');
    if (!desktops.contains(u"GNOME"_s, Qt::CaseInsensitive)) {
        return;
    }
    applyColorScheme(QGuiApplication::styleHints()->colorScheme());
    // Queued: the platform theme may update the palette after it reports the new scheme.
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, &applyColorScheme, Qt::QueuedConnection);
}
