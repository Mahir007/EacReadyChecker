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
    }

    void otherDesktopsAreLeftAlone()
    {
        setScheme(PreferDark);
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        followDesktopColorScheme();
        QVERIFY(!isDarkPalette(QApplication::palette()));
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

private:
    void setScheme(uint scheme) { m_portal.set(Appearance, SchemeKey, QVariant::fromValue(scheme)); }

    void changeScheme(uint scheme) { QVERIFY(m_portal.change(Appearance, SchemeKey, QVariant::fromValue(scheme))); }

    void emitSettingChanged(const QString &group, const QString &key, uint value) { QVERIFY(m_portal.emitChanged(group, key, QVariant::fromValue(value))); }

    FakePortalService m_portal;
};

QTEST_MAIN(TestColorScheme)
#include "tst_colorscheme.moc"
