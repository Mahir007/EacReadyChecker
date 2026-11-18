// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ColorScheme.h"
#include "app/Icons.h"

#include "FakePortal.h"

#include <QApplication>
#include <QStyleHints>
#include <QTest>

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
        // Qt's KDE theme or Plasma's follows the colour scheme, and Qt's GTK plugin gives the GTK
        // theme's own dark palette.
        QTest::newRow("KDE") << QByteArray("KDE") << QByteArray();
        QTest::newRow("Cinnamon") << QByteArray("X-Cinnamon") << QByteArray();
        QTest::newRow("XFCE") << QByteArray("XFCE") << QByteArray();
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

private:
    void setScheme(uint scheme) { m_portal.set(Appearance, SchemeKey, QVariant::fromValue(scheme)); }

    void changeScheme(uint scheme) { QVERIFY(m_portal.change(Appearance, SchemeKey, QVariant::fromValue(scheme))); }

    void emitSettingChanged(const QString &group, const QString &key, uint value) { QVERIFY(m_portal.emitChanged(group, key, QVariant::fromValue(value))); }

    FakePortalService m_portal;
};

QTEST_MAIN(TestColorScheme)
#include "tst_colorscheme.moc"
