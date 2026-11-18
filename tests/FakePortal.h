// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QVariantMap>

#include <memory>
#include <utility>

using PortalSettings = QMap<QString, QVariantMap>;

// The portal's accent-color: red, green and blue from 0 to 1, as a D-Bus structure (ddd).
struct PortalColour {
    double r;
    double g;
    double b;
};
Q_DECLARE_METATYPE(PortalColour)

inline QDBusArgument &operator<<(QDBusArgument &argument, const PortalColour &colour)
{
    argument.beginStructure();
    argument << colour.r << colour.g << colour.b;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, PortalColour &colour)
{
    argument.beginStructure();
    argument >> colour.r >> colour.g >> colour.b;
    argument.endStructure();
    return argument;
}

// ReadAll's reply as xdg-desktop-portal 1.18 builds it: each backend's namespaces in turn, so the
// same namespace can come more than once.
struct PortalReply {
    QList<std::pair<QString, QVariantMap>> groups;
};
Q_DECLARE_METATYPE(PortalReply)

inline QDBusArgument &operator<<(QDBusArgument &argument, const PortalReply &reply)
{
    argument.beginMap(QMetaType::fromType<QString>(), QMetaType::fromType<QVariantMap>());
    for (const auto &[group, values] : reply.groups) {
        argument.beginMapEntry();
        argument << group << values;
        argument.endMapEntry();
    }
    argument.endMap();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, PortalReply &reply)
{
    argument.beginMap();
    while (!argument.atEnd()) {
        std::pair<QString, QVariantMap> group;
        argument.beginMapEntry();
        argument >> group.first >> group.second;
        argument.endMapEntry();
        reply.groups.append(group);
    }
    argument.endMap();
    return argument;
}

// Stand-in for the settings portal.
class FakePortal : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Settings")

public:
    using QObject::QObject;
    PortalSettings settings;
    // A second backend's settings, listed after the first's, as GTK's after COSMIC's on Pop!_OS.
    PortalSettings otherBackend;

public Q_SLOTS:
    PortalReply ReadAll(const QStringList &namespaces)
    {
        PortalReply result;
        for (const PortalSettings *backend : {&settings, &otherBackend}) {
            for (const QString &name : namespaces) {
                if (backend->contains(name)) {
                    result.groups.append({name, backend->value(name)});
                }
            }
        }
        return result;
    }
};

// Runs a FakePortal on its own connection and thread, because the app may block on its call to it.
class FakePortalService
{
public:
    static inline const QString Service = QStringLiteral("org.freedesktop.portal.Desktop");
    static inline const QString Path = QStringLiteral("/org/freedesktop/portal/desktop");
    static inline const QString Interface = QStringLiteral("org.freedesktop.portal.Settings");

    // Empty if it's running, otherwise why it isn't.
    QString start()
    {
        if (!QDBusConnection::sessionBus().isConnected()) {
            return QStringLiteral("No session bus");
        }
        qDBusRegisterMetaType<PortalSettings>();
        qDBusRegisterMetaType<PortalColour>();
        qDBusRegisterMetaType<PortalReply>();
        m_thread.start();
        m_portal = new FakePortal;
        m_portal->moveToThread(&m_thread);
        m_bus = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-portal")));
        // Fails on a desktop whose real portal holds the name. CMake runs these tests on a private bus.
        if (!m_bus->registerService(Service)) {
            return QStringLiteral("The portal's name is taken");
        }
        QMetaObject::invokeMethod(
            m_portal,
            [this] {
                m_bus->registerObject(Path, m_portal, QDBusConnection::ExportAllSlots);
            },
            Qt::BlockingQueuedConnection);
        return {};
    }

    void stop()
    {
        if (m_portal) {
            QMetaObject::invokeMethod(m_portal, &QObject::deleteLater);
        }
        m_thread.quit();
        m_thread.wait();
    }

    // What ReadAll returns from now on.
    void set(const QString &group, const QString &key, const QVariant &value)
    {
        QMetaObject::invokeMethod(
            m_portal,
            [this, group, key, value] {
                m_portal->settings[group][key] = value;
            },
            Qt::BlockingQueuedConnection);
    }

    // What the second backend's ReadAll returns from now on.
    void setOtherBackend(const PortalSettings &settings)
    {
        QMetaObject::invokeMethod(
            m_portal,
            [this, settings] {
                m_portal->otherBackend = settings;
            },
            Qt::BlockingQueuedConnection);
    }

    // Sends SettingChanged, without changing what ReadAll returns.
    bool emitChanged(const QString &group, const QString &key, const QVariant &value)
    {
        QDBusMessage signal = QDBusMessage::createSignal(Path, Interface, QStringLiteral("SettingChanged"));
        signal << group << key << QVariant::fromValue(QDBusVariant(value));
        return m_bus->send(signal);
    }

    // set() and emitChanged(), as when the user changes the setting.
    bool change(const QString &group, const QString &key, const QVariant &value)
    {
        set(group, key, value);
        return emitChanged(group, key, value);
    }

private:
    QThread m_thread;
    QPointer<FakePortal> m_portal;
    std::unique_ptr<QDBusConnection> m_bus;
};
