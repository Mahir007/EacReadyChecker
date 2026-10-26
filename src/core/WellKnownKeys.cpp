// SPDX-License-Identifier: GPL-3.0-or-later
#include "WellKnownKeys.h"

#include "Certificate.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

const WellKnownKeys &WellKnownKeys::builtIn()
{
    static const WellKnownKeys keys = [] {
        QFile file(u":/data/well-known-keys.json"_s);
        QString error = u"can't open the resource"_s;
        if (file.open(QIODevice::ReadOnly)) {
            if (auto parsed = fromJson(file.readAll(), &error)) {
                return *parsed;
            }
        }
        // Tests keep the built-in list valid, so this only happens with a broken build.
        qWarning("well-known-keys.json: %s", qUtf8Printable(error));
        return WellKnownKeys();
    }();
    return keys;
}

std::optional<WellKnownKeys> WellKnownKeys::fromJson(const QByteArray &json, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(parseError.errorString());
    }
    const QJsonValue list = document.object().value(u"keys"_s);
    if (!list.isArray()) {
        return fail(u"no \"keys\" array"_s);
    }

    WellKnownKeys result;
    const QJsonArray entries = list.toArray();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        WellKnownKey key{entry.value(u"vendor"_s).toString(), entry.value(u"subject"_s).toString(),
                         QByteArray::fromHex(entry.value(u"sha256"_s).toString().remove(u':').toLatin1())};
        if (key.vendor.isEmpty() || key.subject.isEmpty()) {
            return fail(u"entry %1 lacks a vendor or subject"_s.arg(result.m_keys.size()));
        }
        if (key.sha256.size() != 32 || Certificate::fingerprint(key.sha256) != entry.value(u"sha256"_s).toString().toUpper()) {
            return fail(u"entry %1 (%2) has a malformed sha256"_s.arg(result.m_keys.size()).arg(key.subject));
        }
        if (result.findFingerprint(key.sha256)) {
            return fail(u"%1 is listed twice"_s.arg(key.subject));
        }
        result.m_keys.append(std::move(key));
    }
    return result;
}

const WellKnownKey *WellKnownKeys::findCertificate(const QByteArray &der) const
{
    return findFingerprint(Certificate::sha256(der));
}

const WellKnownKey *WellKnownKeys::findFingerprint(const QByteArray &sha256) const
{
    for (const WellKnownKey &key : m_keys) {
        if (key.sha256 == sha256) {
            return &key;
        }
    }
    return nullptr;
}
