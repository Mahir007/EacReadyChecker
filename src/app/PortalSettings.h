// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QDBusMessage>
#include <QMap>
#include <QString>
#include <QVariantMap>

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
