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

// The palette the app has set over the platform's, if any.
enum class OwnPalette {
    None,
    Dark,
    Light,
};
OwnPalette s_ownPalette = OwnPalette::None;

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

// libadwaita's sidebar colour for the window and its view colour for the list, the pair GNOME's
// Files uses. Its window colour, #fafafb, is too close to white to tell from the list or the title
// bar. The other colours stay the platform's.
QPalette lightViewColours()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0xeb, 0xeb, 0xed));
    p.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    return p;
}
} // namespace

void applyColorScheme(Qt::ColorScheme scheme)
{
    const OwnPalette wanted = scheme == Qt::ColorScheme::Dark ? OwnPalette::Dark : OwnPalette::Light;
    if (s_ownPalette == wanted) {
        return;
    }
    if (s_ownPalette != OwnPalette::None) {
        QApplication::setPalette(QPalette()); // back to the platform's
        s_ownPalette = OwnPalette::None;
    }
    const QPalette platform = QGuiApplication::palette();
    // Qt's GTK plugin gives Base, the colour for lists, the window's colour.
    const bool oneColour = platform.color(QPalette::Base) == platform.color(QPalette::Window);
    if (wanted == OwnPalette::Dark && (oneColour || !isDarkPalette(platform))) {
        QApplication::setPalette(darkPalette());
        s_ownPalette = wanted;
    } else if (wanted == OwnPalette::Light && oneColour) {
        QApplication::setPalette(lightViewColours());
        s_ownPalette = wanted;
    }
}

void followDesktopColorScheme()
{
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':');
    if (!desktops.contains(u"GNOME"_s, Qt::CaseInsensitive)) {
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
