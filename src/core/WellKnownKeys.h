// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

struct WellKnownKey {
    QString vendor;  // name as Epic publishes it, e.g. "Fedora Secure Boot CA"
    QString subject; // full certificate subject, for display only
    QByteArray sha256;
};

// Epic's well-known keys (src/data/well-known-keys.json). Certificates are matched by
// the SHA-256 of their DER bytes, never by name.
class WellKnownKeys
{
public:
    // The list built into the app.
    static const WellKnownKeys &builtIn();

    // nullopt (and `error` set) if the JSON or any entry in it is malformed.
    static std::optional<WellKnownKeys> fromJson(const QByteArray &json, QString *error = nullptr);

    const WellKnownKey *findCertificate(const QByteArray &der) const;
    const WellKnownKey *findFingerprint(const QByteArray &sha256) const;

    const QList<WellKnownKey> &keys() const { return m_keys; }

private:
    QList<WellKnownKey> m_keys;
};
