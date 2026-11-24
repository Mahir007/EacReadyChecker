// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ColorScheme.h"
#include "app/Icons.h"

#include "FakePortal.h"

#include <QApplication>
#include <QFont>
#include <QStyleHints>
#include <QTest>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace
{
const QString Appearance = u"org.freedesktop.appearance"_s;
const QString SchemeKey = u"color-scheme"_s;
const QString AccentKey = u"accent-color"_s;
constexpr uint PreferDark = 1;
constexpr uint PreferLight = 2;
} // namespace

class TestColorScheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        const QString problem = m_portal.start();
        if (!problem.isEmpty()) {
            QSKIP(qPrintable(problem));
        }
    }

    void cleanupTestCase()
    {
        m_portal.stop();
        qunsetenv("XDG_CURRENT_DESKTOP");
        qunsetenv("QT_QPA_PLATFORMTHEME");
    }

    void otherDesktopsAreLeftAlone_data()
    {
        QTest::addColumn<QByteArray>("desktop");
        QTest::addColumn<QByteArray>("platformTheme");
        // Qt's KDE theme or Plasma's follows the colour scheme.
        QTest::newRow("KDE") << QByteArray("KDE") << QByteArray();
        // The user picked their colours.
        QTest::newRow("qt6ct") << QByteArray("COSMIC") << QByteArray("qt6ct");
    }

    void otherDesktopsAreLeftAlone()
    {
        QFETCH(QByteArray, desktop);
        QFETCH(QByteArray, platformTheme);
        setScheme(PreferDark);
        m_portal.set(Appearance, AccentKey, QVariant::fromValue(PortalColour{0.57, 0.38, 0.77}));
        qputenv("XDG_CURRENT_DESKTOP", desktop);
        qputenv("QT_QPA_PLATFORMTHEME", platformTheme);
        followDesktopColorScheme();
        QVERIFY(!isDarkPalette(QApplication::palette()));
        QVERIFY(!windowColours(QApplication::palette()).isBrushSet(QPalette::Active, QPalette::Highlight));
        qunsetenv("QT_QPA_PLATFORMTHEME");
        m_portal.set(Appearance, AccentKey, QVariant::fromValue(PortalColour{-1, -1, -1}));
    }

    void followsThePortalOnGnome()
    {
        // Qt reports nothing here, as it can report light at startup on GNOME. The palette has to be
        // dark before the window is first drawn, so no event loop runs before this check.
        QCOMPARE(QGuiApplication::styleHints()->colorScheme(), Qt::ColorScheme::Unknown);
        setScheme(PreferDark);
        qputenv("XDG_CURRENT_DESKTOP", "ubuntu:GNOME");
        followDesktopColorScheme();
        QVERIFY(isDarkPalette(QApplication::palette()));

        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));

        changeScheme(PreferDark);
        QTRY_VERIFY(isDarkPalette(QApplication::palette()));

        // Another setting changing isn't a reason to switch.
        emitSettingChanged(u"org.gnome.desktop.interface"_s, u"color-scheme"_s, PreferLight);
        QTest::qWait(100);
        QVERIFY(isDarkPalette(QApplication::palette()));

        constexpr uint noPreference = 0;
        changeScheme(noPreference);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
    }

    void followsTheAccentOnGnome()
    {
        // The accent GNOME 47 reports: in the app's dark palette, and in light over GTK's palette in
        // the windows' own colours.
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        const QColor purple = QColor::fromRgbF(0.57f, 0.38f, 0.77f);
        const QColor green = QColor::fromRgbF(0.15f, 0.64f, 0.41f);
        m_portal.set(Appearance, AccentKey, QVariant::fromValue(PortalColour{0.57, 0.38, 0.77}));
        setScheme(PreferDark);
        followDesktopColorScheme();
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), purple);

        // Picked while the app runs.
        QVERIFY(m_portal.change(Appearance, AccentKey, QVariant::fromValue(PortalColour{0.15, 0.64, 0.41})));
        QTRY_COMPARE(QApplication::palette().color(QPalette::Highlight), green);

        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
        QCOMPARE(windowColours(QApplication::palette()).color(QPalette::Highlight), green);

        // Out of range: no accent, so GTK's highlight in light and libadwaita's blue in dark.
        QVERIFY(m_portal.change(Appearance, AccentKey, QVariant::fromValue(PortalColour{-1, -1, -1})));
        QTRY_VERIFY(!windowColours(QApplication::palette()).isBrushSet(QPalette::Active, QPalette::Highlight));
        changeScheme(PreferDark);
        QTRY_COMPARE(QApplication::palette().color(QPalette::Highlight), QColor(0x35, 0x84, 0xe4));
        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
    }

    void followsThePortalWhereQtHasNoTheme()
    {
        // As on COSMIC, Hyprland or Sway: Qt's generic theme only has a light palette.
        qputenv("XDG_CURRENT_DESKTOP", "COSMIC");
        const QColor cyan = QColor::fromRgbF(0.39f, 0.82f, 0.87f);
        m_portal.set(Appearance, AccentKey, QVariant::fromValue(PortalColour{0.39, 0.82, 0.87}));
        setScheme(PreferDark);
        // Pop!_OS's portal also lists GTK's backend, after COSMIC's, with no accent. Each setting
        // comes from the first backend that has it.
        m_portal.setOtherBackend({{Appearance, {{SchemeKey, QVariant::fromValue(PreferLight)}}}});
        followDesktopColorScheme();
        QVERIFY(isDarkPalette(QApplication::palette()));
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), cyan);
        m_portal.setOtherBackend({});

        // In light the accent is in the windows' own colours, and nothing else is.
        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
        const QPalette colours = windowColours(QApplication::palette());
        QCOMPARE(colours.color(QPalette::Highlight), cyan);
        QVERIFY(!colours.isBrushSet(QPalette::Active, QPalette::Window));
        QVERIFY(!colours.isBrushSet(QPalette::Active, QPalette::Base));

        m_portal.set(Appearance, AccentKey, QVariant::fromValue(PortalColour{-1, -1, -1}));
    }

    void replacesTheSandboxPalette()
    {
        // In the Flatpak, Qt's portal theme gives Fusion's dark palette when the app starts in dark,
        // and keeps it after a switch to light.
        qputenv("XDG_CURRENT_DESKTOP", "COSMIC");
        QPalette fusionDark(QColor(0x32, 0x32, 0x32), QColor(0x32, 0x32, 0x32));
        fusionDark.setColor(QPalette::WindowText, QColor(0xf0, 0xf0, 0xf0));
        fusionDark.setColor(QPalette::Text, QColor(0xf0, 0xf0, 0xf0));
        fusionDark.setColor(QPalette::Base, QColor(0x24, 0x24, 0x24));
        QApplication::setPalette(fusionDark);
        setScheme(PreferDark);
        followDesktopColorScheme();
        QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(0x24, 0x24, 0x24));

        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
        changeScheme(PreferDark);
        QTRY_VERIFY(isDarkPalette(QApplication::palette()));
        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
    }

    void linksFollowTheAccent()
    {
        // libadwaita's blue, and the colours its documentation gives for blue text and links.
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        m_portal.set(Appearance, AccentKey, QVariant::fromValue(PortalColour{0x35 / 255.0, 0x84 / 255.0, 0xe4 / 255.0}));
        setScheme(PreferDark);
        followDesktopColorScheme();
        QVERIFY(isDarkPalette(QApplication::palette()));
        QVERIFY(near(QApplication::palette().color(QPalette::Link), QColor(0x81, 0xd0, 0xff)));

        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
        const QPalette colours = windowColours(QApplication::palette());
        QVERIFY(near(colours.color(QPalette::Link), QColor(0x04, 0x61, 0xbe)));
        QVERIFY(colours.isBrushSet(QPalette::Active, QPalette::LinkVisited));

        // Without an accent, links keep their colours.
        QVERIFY(m_portal.change(Appearance, AccentKey, QVariant::fromValue(PortalColour{-1, -1, -1})));
        QTRY_VERIFY(!windowColours(QApplication::palette()).isBrushSet(QPalette::Active, QPalette::Link));
        changeScheme(PreferDark);
        QTRY_COMPARE(QApplication::palette().color(QPalette::Link), QColor(0x78, 0xae, 0xed));
        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
    }

    void followsThePortalOnGtkDesktops_data()
    {
        QTest::addColumn<QByteArray>("desktop");
        QTest::addColumn<bool>("listed");
        QTest::addColumn<bool>("hasReadOne");
        QTest::newRow("Cinnamon") << QByteArray("X-Cinnamon") << true << true;
        QTest::newRow("XFCE") << QByteArray("XFCE") << true << true;
        // A backend that lists nothing in ReadAll, as elementary OS 8's does, before GTK's, which has
        // the colour scheme but no accent.
        QTest::newRow("unlisted") << QByteArray("Pantheon") << false << true;
        // The same with xdg-desktop-portal before 1.15.
        QTest::newRow("unlisted, no ReadOne") << QByteArray("Pantheon") << false << false;
    }

    void followsThePortalOnGtkDesktops()
    {
        // A dark style that leaves the GTK theme light, so Qt's GTK plugin keeps a light palette.
        QFETCH(QByteArray, desktop);
        QFETCH(bool, listed);
        QFETCH(bool, hasReadOne);
        qputenv("XDG_CURRENT_DESKTOP", desktop);
        const QColor grape(0xa5, 0x6d, 0xe2);
        const QColor mint(0x28, 0xbc, 0xa3);
        const PortalSettings appearance{{Appearance, {{SchemeKey, QVariant::fromValue(PreferDark)}, {AccentKey, portalColour(grape)}}}};
        m_portal.setSettings(listed ? appearance : PortalSettings{});
        m_portal.setUnlisted(listed ? PortalSettings{} : appearance);
        m_portal.setOtherBackend(listed ? PortalSettings{} : PortalSettings{{Appearance, {{SchemeKey, QVariant::fromValue(PreferDark)}}}});
        m_portal.setHasReadOne(hasReadOne);
        followDesktopColorScheme();
        QVERIFY(isDarkPalette(QApplication::palette()));
        QVERIFY(near(QApplication::palette().color(QPalette::Highlight), grape));

        // In light, the accent is in the windows' own colours, over GTK's palette.
        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
        QVERIFY(near(windowColours(QApplication::palette()).color(QPalette::Highlight), grape));

        // Picked while the app runs.
        QVERIFY(m_portal.emitChanged(Appearance, AccentKey, portalColour(mint)));
        QTRY_VERIFY(near(windowColours(QApplication::palette()).color(QPalette::Highlight), mint));
        changeScheme(PreferDark);
        QTRY_VERIFY(isDarkPalette(QApplication::palette()));
        QVERIFY(near(QApplication::palette().color(QPalette::Highlight), mint));

        m_portal.setUnlisted({});
        m_portal.setOtherBackend({});
        m_portal.setHasReadOne(true);
        changeScheme(PreferLight);
        QTRY_VERIFY(!isDarkPalette(QApplication::palette()));
    }

    void keepsADarkGtkTheme()
    {
        // A GTK desktop whose dark style is a dark GTK theme: Qt's GTK plugin gives its colours, with
        // the list in the window's colour, and they stay.
        const QColor window(0x2f, 0x2f, 0x2f);
        QPalette gtkDark = QApplication::palette();
        for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Base, QPalette::Button}) {
            gtkDark.setColor(role, window);
        }
        for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
            gtkDark.setColor(role, QColor(0xda, 0xda, 0xda));
        }
        QApplication::setPalette(gtkDark);
        qputenv("XDG_CURRENT_DESKTOP", "X-Cinnamon");
        applyColorScheme(Qt::ColorScheme::Dark);
        QCOMPARE(QApplication::palette().color(QPalette::Window), window);

        // On GNOME, libadwaita's colours replace them.
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        applyColorScheme(Qt::ColorScheme::Dark);
        QCOMPARE(QApplication::palette().color(QPalette::Window), QColor(0x24, 0x24, 0x24));
        applyColorScheme(Qt::ColorScheme::Light);
        QVERIFY(!isDarkPalette(QApplication::palette()));
        qunsetenv("XDG_CURRENT_DESKTOP");
    }

    void replacesAnInvalidDimmedColour()
    {
        // Qt's GTK plugin on a desktop set to dark with a light GTK theme: dark grey text less 100
        // shades is out of range. Also where the app leaves the colours to Qt.
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        QPalette palette = QApplication::palette();
        palette.setColor(QPalette::PlaceholderText, QColor(0x33 - 100, 0x33 - 100, 0x33 - 100));
        QVERIFY(!palette.color(QPalette::PlaceholderText).isValid());
        const QPalette colours = windowColours(palette);
        QVERIFY(colours.isBrushSet(QPalette::Active, QPalette::PlaceholderText));
        const QColor dimmed = colours.color(QPalette::PlaceholderText);
        QCOMPARE(dimmed.rgb(), palette.color(QPalette::WindowText).rgb());
        QVERIFY(dimmed.alpha() < 255);

        // A valid one is left alone.
        QVERIFY(!windowColours(QApplication::palette()).isBrushSet(QPalette::Active, QPalette::PlaceholderText));
        qunsetenv("XDG_CURRENT_DESKTOP");
    }

    void followsPlasmaFont()
    {
        // Qt's own KDE theme, as in the AppImage, reads Plasma's font only at startup. KDE's portal
        // sends a change as the text in kdeglobals.
        const QString general = u"org.kde.kdeglobals.General"_s;
        const QString serif = u"Noto Serif,11,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"_s;
        const QString style = QApplication::style()->name();
        const QFont font = QApplication::font();
        QApplication::setStyle(u"Fusion"_s);
        QWidget widget;

        // followPlasmaFont() does nothing on other desktops.
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        followPlasmaFont();
        QVERIFY(m_portal.emitChanged(general, u"font"_s, serif));
        QTest::qWait(100);
        QCOMPARE(QApplication::font(), font);

        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        followPlasmaFont();
        QVERIFY(m_portal.emitChanged(general, u"font"_s, serif));
        QTRY_COMPARE(QApplication::font().family(), u"Noto Serif"_s);
        QCOMPARE(QApplication::font().pointSize(), 11);
        // Open widgets get it too.
        QCOMPARE(widget.font().family(), u"Noto Serif"_s);

        // Back to Plasma's default, which kdeglobals doesn't store: the font stays.
        QVERIFY(m_portal.emitChanged(general, u"font"_s, QString()));
        QTest::qWait(100);
        QCOMPARE(QApplication::font().family(), u"Noto Serif"_s);

        QApplication::setFont(font);
        QApplication::setStyle(style);
        qunsetenv("XDG_CURRENT_DESKTOP");
    }

private:
    // Within a shade per channel: libadwaita's values are rounded from OKLab.
    static bool near(const QColor &a, const QColor &b)
    {
        return std::abs(a.red() - b.red()) <= 1 && std::abs(a.green() - b.green()) <= 1 && std::abs(a.blue() - b.blue()) <= 1;
    }

    static QVariant portalColour(const QColor &colour) { return QVariant::fromValue(PortalColour{colour.redF(), colour.greenF(), colour.blueF()}); }

    void setScheme(uint scheme) { m_portal.set(Appearance, SchemeKey, QVariant::fromValue(scheme)); }

    void changeScheme(uint scheme) { QVERIFY(m_portal.change(Appearance, SchemeKey, QVariant::fromValue(scheme))); }

    void emitSettingChanged(const QString &group, const QString &key, uint value) { QVERIFY(m_portal.emitChanged(group, key, QVariant::fromValue(value))); }

    FakePortalService m_portal;
};

QTEST_MAIN(TestColorScheme)
#include "tst_colorscheme.moc"
