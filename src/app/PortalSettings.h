// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QLatin1StringView>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>

inline constexpr auto PortalService = QLatin1StringView("org.freedesktop.portal.Desktop");
inline constexpr auto PortalPath = QLatin1StringView("/org/freedesktop/portal/desktop");
inline constexpr auto PortalInterface = QLatin1StringView("org.freedesktop.portal.Settings");

// Calls the receiver's settingChanged(QString, QString, QDBusVariant) slot when a setting changes.
// Qt 6.5.0 looks the slot's argument types up by name, and finds QDBusVariant only once it's
// registered, which a reply that holds one also does. Before that, the connection fails.
inline bool connectSettingChanged(QObject *receiver)
{
    qRegisterMetaType<QDBusVariant>();
    return QDBusConnection::sessionBus().connect(PortalService,
                                                 PortalPath,
                                                 PortalInterface,
                                                 QStringLiteral("SettingChanged"),
                                                 receiver,
                                                 SLOT(settingChanged(QString, QString, QDBusVariant)));
}

// The settings portal's reply to ReadAll, by namespace. xdg-desktop-portal 1.18 asks every backend
// that has settings and lists each one's namespaces in turn, so a namespace can come twice: on
// Pop!_OS, from COSMIC's backend and then GTK's, which has no accent. Qt's own conversion keeps the
// last. Here each key comes from the first backend that has it, as with ReadOne.
inline QMap<QString, QVariantMap> portalSettings(const QDBusMessage &reply)
{
    QMap<QString, QVariantMap> settings;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()
        || reply.arguments().constFirst().metaType() != QMetaType::fromType<QDBusArgument>()) {
        return settings;
    }
    const auto argument = qvariant_cast<QDBusArgument>(reply.arguments().constFirst());
    if (argument.currentType() != QDBusArgument::MapType) {
        return settings;
    }
    argument.beginMap();
    while (!argument.atEnd()) {
        QString group;
        QVariantMap values;
        argument.beginMapEntry();
        argument >> group >> values;
        argument.endMapEntry();
        QVariantMap &merged = settings[group];
        for (auto entry = values.cbegin(); entry != values.cend(); ++entry) {
            if (!merged.contains(entry.key())) {
                merged.insert(entry.key(), entry.value());
            }
        }
    }
    argument.endMap();
    return settings;
}
