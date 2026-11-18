// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColorScheme.h"

#include "Icons.h"

#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QPalette>
#include <QStyleHints>

#include <optional>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto PortalService = "org.freedesktop.portal.Desktop"_L1;
constexpr auto PortalPath = "/org/freedesktop/portal/desktop"_L1;
constexpr auto PortalInterface = "org.freedesktop.portal.Settings"_L1;
constexpr auto AppearanceNamespace = "org.freedesktop.appearance"_L1;
constexpr auto ColorSchemeKey = "color-scheme"_L1;
constexpr int PortalTimeoutMs = 1000;

bool s_darkPaletteSet = false;

bool isGnome()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':').contains(u"GNOME"_s, Qt::CaseInsensitive);
}

bool isKde()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':').contains(u"KDE"_s, Qt::CaseInsensitive);
}

// Qt's GTK plugin takes Window and Base from the same GTK colour, so a list has the window's colour.
bool hasOneColour(const QPalette &palette)
{
    return palette.color(QPalette::Base) == palette.color(QPalette::Window);
}

// The portal's values: 0 no preference, 1 prefer dark, 2 prefer light.
Qt::ColorScheme schemeFromPortal(uint value)
{
    return value == 1 ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
}

// Blocks, so the first frame already has the right colours.
std::optional<Qt::ColorScheme> readPortalColorScheme()
{
    QDBusMessage message = QDBusMessage::createMethodCall(PortalService, PortalPath, PortalInterface, u"ReadAll"_s);
    message << QStringList{AppearanceNamespace};
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, PortalTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return std::nullopt;
    }
    const auto settings = qdbus_cast<QMap<QString, QVariantMap>>(reply.arguments().constFirst());
    const QVariant value = settings.value(AppearanceNamespace).value(ColorSchemeKey);
    if (!value.isValid()) {
        return std::nullopt;
    }
    return schemeFromPortal(value.toUInt());
}

class PortalListener : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public Q_SLOTS:
    void settingChanged(const QString &group, const QString &key, const QDBusVariant &value)
    {
        if (group == AppearanceNamespace && key == ColorSchemeKey) {
            applyColorScheme(schemeFromPortal(value.variant().toUInt()));
        }
    }
};

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
        // A dark GTK theme still gives lists the window's colour.
        const QPalette palette = QGuiApplication::palette();
        if (!isDarkPalette(palette) || hasOneColour(palette)) {
            QApplication::setPalette(darkPalette());
            s_darkPaletteSet = true;
        }
    } else if (s_darkPaletteSet) {
        QApplication::setPalette(QPalette());
        s_darkPaletteSet = false;
    }
}

QPalette windowColours(const QPalette &appPalette)
{
    QPalette p;
    if (isKde() && !isBreezeStyle()) {
        // Qt's own KDE theme, as in the AppImage, gives an unfocused window the focused window's
        // highlight. Plasma tints the window colour with it, in its own colour space, and 29% of
        // the highlight comes within a few shades of that.
        const QColor highlight = appPalette.color(QPalette::Active, QPalette::Highlight);
        if (appPalette.color(QPalette::Inactive, QPalette::Highlight) == highlight) {
            const QColor window = appPalette.color(QPalette::Inactive, QPalette::Window);
            const auto mix = [](int a, int b) { return qRound(a + (b - a) * 0.29); };
            p.setColor(QPalette::Inactive,
                       QPalette::Highlight,
                       QColor(mix(window.red(), highlight.red()), mix(window.green(), highlight.green()), mix(window.blue(), highlight.blue())));
            p.setColor(QPalette::Inactive, QPalette::HighlightedText, appPalette.color(QPalette::Inactive, QPalette::WindowText));
        }
        return p;
    }
    if (!isGnome() || isDarkPalette(appPalette) || !hasOneColour(appPalette)) {
        return p;
    }
    // libadwaita's sidebar colour for the window and its view colour for lists, the pair GNOME's
    // Files uses. Its window colour, #fafafb, is too close to white to tell from a list or the title bar.
    p.setColor(QPalette::Window, QColor(0xeb, 0xeb, 0xed));
    p.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Inactive, role, appPalette.color(QPalette::Active, role));
    }
    return p;
}

void followDesktopColorScheme()
{
    if (!isGnome()) {
        return;
    }
    if (const auto scheme = readPortalColorScheme()) {
        applyColorScheme(*scheme);
        QDBusConnection::sessionBus().connect(PortalService,
                                              PortalPath,
                                              PortalInterface,
                                              u"SettingChanged"_s,
                                              new PortalListener(qApp),
                                              SLOT(settingChanged(QString, QString, QDBusVariant)));
        return;
    }
    applyColorScheme(QGuiApplication::styleHints()->colorScheme());
    // Queued: the platform theme may update the palette after it reports the new scheme.
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, &applyColorScheme, Qt::QueuedConnection);
}

#include "ColorScheme.moc"
