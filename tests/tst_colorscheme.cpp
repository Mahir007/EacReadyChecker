// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ColorScheme.h"
#include "app/Icons.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QPointer>
#include <QStyleHints>
#include <QTest>
#include <QThread>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
using Settings = QMap<QString, QVariantMap>;

const QString Service = u"org.freedesktop.portal.Desktop"_s;
const QString Path = u"/org/freedesktop/portal/desktop"_s;
const QString Interface = u"org.freedesktop.portal.Settings"_s;
const QString Appearance = u"org.freedesktop.appearance"_s;
const QString SchemeKey = u"color-scheme"_s;
constexpr uint PreferDark = 1;
constexpr uint PreferLight = 2;

// Stand-in for the settings portal.
class FakePortal : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Settings")

public:
    using QObject::QObject;
    uint scheme = PreferDark;

public Q_SLOTS:
    Settings ReadAll(const QStringList &)
    {
        Settings settings;
        settings[Appearance][SchemeKey] = QVariant::fromValue(scheme);
        return settings;
    }
};
} // namespace

class TestColorScheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        if (!QDBusConnection::sessionBus().isConnected()) {
            QSKIP("No session bus");
        }
        qDBusRegisterMetaType<Settings>();

        // On its own connection and thread, because the app blocks on its call to the portal.
        m_thread.start();
        m_portal = new FakePortal;
        m_portal->moveToThread(&m_thread);
        m_bus =std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fake-portal"_s));
        // Fails on a desktop whose real portal holds the name. CMake runs this test on a private bus.
        if (!m_bus->registerService(Service)) {
            QSKIP("The portal's name is taken");
        }
        QMetaObject::invokeMethod(
            m_portal,
            [this] {
                m_bus->registerObject(Path, m_portal, QDBusConnection::ExportAllSlots);
            },
            Qt::BlockingQueuedConnection);
    }

    void cleanupTestCase()
    {
        if (m_portal) {
            QMetaObject::invokeMethod(m_portal, &QObject::deleteLater);
        }
        m_thread.quit();
        m_thread.wait();
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
    void setScheme(uint scheme)
    {
        QMetaObject::invokeMethod(
            m_portal,
            [this, scheme] {
                m_portal->scheme = scheme;
            },
            Qt::BlockingQueuedConnection);
    }

    void changeScheme(uint scheme)
    {
        setScheme(scheme);
        emitSettingChanged(Appearance, SchemeKey, scheme);
    }

    void emitSettingChanged(const QString &group, const QString &key, uint value)
    {
        QDBusMessage signal = QDBusMessage::createSignal(Path, Interface, u"SettingChanged"_s);
        signal << group << key << QVariant::fromValue(QDBusVariant(QVariant::fromValue(value)));
        QVERIFY(m_bus->send(signal));
    }

    QThread m_thread;
    QPointer<FakePortal> m_portal;
    std::unique_ptr<QDBusConnection> m_bus;
};

QTEST_MAIN(TestColorScheme)
#include "tst_colorscheme.moc"
