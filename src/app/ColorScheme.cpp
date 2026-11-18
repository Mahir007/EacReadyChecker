// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColorScheme.h"

#include "Icons.h"
#include "PortalSettings.h"

#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto PortalService = "org.freedesktop.portal.Desktop"_L1;
constexpr auto PortalPath = "/org/freedesktop/portal/desktop"_L1;
constexpr auto PortalInterface = "org.freedesktop.portal.Settings"_L1;
constexpr auto AppearanceNamespace = "org.freedesktop.appearance"_L1;
constexpr auto ColorSchemeKey = "color-scheme"_L1;
constexpr auto AccentColorKey = "accent-color"_L1;
constexpr int PortalTimeoutMs = 1000;

bool s_darkPaletteSet = false;
// The desktop's accent, if the portal reports one (GNOME 47 and later).
std::optional<QColor> s_accent;

bool isGnome()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':').contains(u"GNOME"_s, Qt::CaseInsensitive);
}

bool isKde()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':').contains(u"KDE"_s, Qt::CaseInsensitive);
}

// Desktops Qt loads a theme for: KDE, and the GTK desktops in Qt 6.11's qgenericunixtheme.cpp. On
// any other, such as COSMIC, Hyprland or Sway, Qt uses its generic theme, which only has a light
// palette and doesn't ask the portal. Unless the user picked a theme, such as qt6ct.
bool hasQtTheme()
{
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME")) {
        return true;
    }
    static const QStringList qtDesktops{u"KDE"_s, u"GNOME"_s, u"X-CINNAMON"_s, u"PANTHEON"_s, u"UNITY"_s, u"MATE"_s, u"XFCE"_s, u"LXDE"_s};
    const QStringList names = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(u':');
    return std::ranges::any_of(names, [](const QString &name) { return qtDesktops.contains(name, Qt::CaseInsensitive); });
}

// GNOME, and desktops Qt has no theme for. Elsewhere Qt's theme follows the desktop's colours.
bool followsPortal()
{
    return isGnome() || !hasQtTheme();
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

// The portal's accent: red, green and blue from 0 to 1. A value out of range means there's none.
std::optional<QColor> accentFromPortal(const QVariant &value)
{
    if (value.userType() != qMetaTypeId<QDBusArgument>()) {
        return std::nullopt;
    }
    const auto argument = value.value<QDBusArgument>();
    double r = -1;
    double g = -1;
    double b = -1;
    argument.beginStructure();
    argument >> r >> g >> b;
    argument.endStructure();
    for (const double c : {r, g, b}) {
        if (c < 0 || c > 1) {
            return std::nullopt;
        }
    }
    return QColor::fromRgbF(float(r), float(g), float(b));
}

// Reads the accent into s_accent and returns the colour scheme. Blocks, so the first frame already
// has the right colours.
std::optional<Qt::ColorScheme> readPortalAppearance()
{
    QDBusMessage message = QDBusMessage::createMethodCall(PortalService, PortalPath, PortalInterface, u"ReadAll"_s);
    message << QStringList{AppearanceNamespace};
    const QVariantMap appearance = portalSettings(QDBusConnection::sessionBus().call(message, QDBus::Block, PortalTimeoutMs)).value(AppearanceNamespace);
    s_accent = accentFromPortal(appearance.value(AccentColorKey));
    const QVariant value = appearance.value(ColorSchemeKey);
    if (!value.isValid()) {
        return std::nullopt;
    }
    return schemeFromPortal(value.toUInt());
}

// The accent as libadwaita uses it for text and links: the same hue, with OKLab lightness at most
// 0.5 in light and at least 0.85 in dark, so it reads on either background. Clipped to sRGB, as
// libadwaita does.
QColor accentText(const QColor &accent, bool dark)
{
    const auto toLinear = [](float c) { return c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F); };
    const auto toSrgb = [](float c) {
        c = std::clamp(c, 0.0F, 1.0F);
        return c <= 0.0031308F ? c * 12.92F : 1.055F * std::pow(c, 1 / 2.4F) - 0.055F;
    };
    const float r = toLinear(accent.redF());
    const float g = toLinear(accent.greenF());
    const float b = toLinear(accent.blueF());
    // Björn Ottosson's sRGB to OKLab.
    const float l = std::cbrt(0.4122214708F * r + 0.5363325363F * g + 0.0514459929F * b);
    const float m = std::cbrt(0.2119034982F * r + 0.6806995451F * g + 0.1073969566F * b);
    const float s = std::cbrt(0.0883024619F * r + 0.2817188376F * g + 0.6299787005F * b);
    float lightness = 0.2104542553F * l + 0.7936177850F * m - 0.0040720468F * s;
    const float a = 1.9779984951F * l - 2.4285922050F * m + 0.4505937099F * s;
    const float bb = 0.0259040371F * l + 0.7827717662F * m - 0.8086757660F * s;
    lightness = dark ? std::max(lightness, 0.85F) : std::min(lightness, 0.5F);
    const auto cube = [](float v) { return v * v * v; };
    const float l2 = cube(lightness + 0.3963377774F * a + 0.2158037573F * bb);
    const float m2 = cube(lightness - 0.1055613458F * a - 0.0638541728F * bb);
    const float s2 = cube(lightness - 0.0894841775F * a - 1.2914855480F * bb);
    return QColor::fromRgbF(toSrgb(4.0767416621F * l2 - 3.3077115913F * m2 + 0.2309699292F * s2),
                            toSrgb(-1.2684380046F * l2 + 2.6097574011F * m2 - 0.3413193965F * s2),
                            toSrgb(-0.0041960863F * l2 - 0.7034186147F * m2 + 1.7076147010F * s2));
}

// Links in the accent, and visited ones 80% of that over the text colour, as in libadwaita.
void setAccentLinks(QPalette &palette, const QColor &accent, bool dark)
{
    const QColor link = accentText(accent, dark);
    const QColor text = palette.color(QPalette::Text);
    const auto mix = [](float a, float b) { return a * 0.8F + b * 0.2F; };
    palette.setColor(QPalette::Link, link);
    palette.setColor(QPalette::LinkVisited,
                     QColor::fromRgbF(mix(link.redF(), text.redF()), mix(link.greenF(), text.greenF()), mix(link.blueF(), text.blueF())));
}

QPalette darkPalette();

class PortalListener : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public Q_SLOTS:
    void settingChanged(const QString &group, const QString &key, const QDBusVariant &value)
    {
        if (group != AppearanceNamespace) {
            return;
        }
        if (key == ColorSchemeKey) {
            applyColorScheme(schemeFromPortal(value.variant().toUInt()));
        } else if (key == AccentColorKey) {
            s_accent = accentFromPortal(value.variant());
            if (s_darkPaletteSet) {
                QApplication::setPalette(darkPalette());
            }
            // In light, the accent is in the windows' own colours, which they set again on a palette
            // change (windowColours).
            const QWidgetList windows = QApplication::topLevelWidgets();
            for (QWidget *window : windows) {
                QCoreApplication::postEvent(window, new QEvent(QEvent::PaletteChange));
            }
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
    // The desktop's accent, or libadwaita's default blue.
    p.setColor(QPalette::Highlight, s_accent.value_or(QColor(0x35, 0x84, 0xe4)));
    p.setColor(QPalette::HighlightedText, text);
    p.setColor(QPalette::Link, QColor(0x78, 0xae, 0xed));
    p.setColor(QPalette::LinkVisited, QColor(0xc0, 0x61, 0xcb));
    if (s_accent) {
        setAccentLinks(p, *s_accent, true);
    }
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

// Fusion's light colours, the ones Qt's generic theme gives (qt_fusionPalette in Qt 6.11).
QPalette lightPalette()
{
    const QColor window(0xef, 0xef, 0xef);
    const QColor dark = window.darker(150);
    const QColor mid = window.darker(130);
    const QColor shadow = dark.darker(135);
    const QColor highlight(0x30, 0x8c, 0xc6);
    const QColor disabled(0xbe, 0xbe, 0xbe);

    QPalette p(Qt::black, window, window.lighter(150), dark, mid, Qt::black, Qt::white);
    p.setColor(QPalette::Midlight, mid.lighter(110));
    p.setColor(QPalette::Button, window);
    p.setColor(QPalette::Shadow, shadow);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::Accent, highlight);
    p.setColor(QPalette::PlaceholderText, QColor(0, 0, 0, 128));
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, disabled);
    }
    p.setColor(QPalette::Disabled, QPalette::Base, window);
    p.setColor(QPalette::Disabled, QPalette::Dark, QColor(0xd1, 0xd1, 0xd1).darker(110));
    p.setColor(QPalette::Disabled, QPalette::Shadow, shadow.lighter(150));
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x91, 0x91, 0x91));
    p.setColor(QPalette::Disabled, QPalette::Accent, QColor(0x91, 0x91, 0x91));
    return p;
}
} // namespace

void applyColorScheme(Qt::ColorScheme scheme)
{
    // Without a Qt theme, both palettes are set here. In the Flatpak, Qt's portal theme reports the
    // scheme, but its palette is Fusion's in the scheme the app started in, and stays so.
    if (scheme == Qt::ColorScheme::Dark) {
        // A dark GTK theme still gives lists the window's colour.
        const QPalette palette = QGuiApplication::palette();
        if (!hasQtTheme() || !isDarkPalette(palette) || hasOneColour(palette)) {
            QApplication::setPalette(darkPalette());
            s_darkPaletteSet = true;
        }
    } else if (!hasQtTheme()) {
        QApplication::setPalette(lightPalette());
        s_darkPaletteSet = false;
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
    if (!followsPortal()) {
        return p;
    }
    // The desktop's accent, which GTK 3's palette may not have, also for links.
    // The app's dark palette has it already.
    if (s_accent) {
        if (appPalette.color(QPalette::Highlight) != *s_accent) {
            p.setColor(QPalette::Highlight, *s_accent);
        }
        QPalette links = appPalette;
        setAccentLinks(links, *s_accent, isDarkPalette(appPalette));
        if (links.color(QPalette::Link) != appPalette.color(QPalette::Link)) {
            p.setColor(QPalette::Link, links.color(QPalette::Link));
            p.setColor(QPalette::LinkVisited, links.color(QPalette::LinkVisited));
        }
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
    if (!followsPortal()) {
        return;
    }
    if (const auto scheme = readPortalAppearance()) {
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
