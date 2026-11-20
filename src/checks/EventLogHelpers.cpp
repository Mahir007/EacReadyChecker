// SPDX-License-Identifier: GPL-3.0-or-later
#include "EventLogHelpers.h"

#include "CheckHelpers.h"
#include "HostPaths.h"
#include "core/Certificate.h"
#include "core/WellKnownKeys.h"

#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace
{
// Authority events hold an EFI_SIGNATURE_DATA: the owner GUID, then the certificate. Accept a
// bare certificate too, in case some firmware leaves the GUID out.
std::optional<QByteArray> authorityCertificate(const QByteArray &data)
{
    if (data.size() > 16 && Certificate::isDer(data.mid(16))) {
        return data.mid(16);
    }
    if (Certificate::isDer(data)) {
        return data;
    }
    return std::nullopt;
}

// Firmware logs an authority just before it measures the image it verified. If the next image
// is a driver (e.g. a GPU's option ROM), the key verified firmware, not the boot chain.
bool verifiedDriver(const QList<TcgEventLog::Event> &events, qsizetype authority)
{
    for (qsizetype i = authority + 1; i < events.size(); ++i) {
        switch (events[i].type) {
        case TcgEventLog::EfiBootServicesDriver:
        case TcgEventLog::EfiRuntimeServicesDriver:
            return true;
        case TcgEventLog::EfiBootServicesApplication:
            return false;
        default:
            break;
        }
    }
    return false;
}
} // namespace

namespace EventLogHelpers
{
Loaded load(const HostSnapshot &snapshot)
{
    if (snapshot.privilegedState != PrivilegedState::Granted) {
        return {std::nullopt, CheckStatus::NeedsAdmin, u"Needs admin access"_s};
    }
    const HostEntry entry = snapshot.file(HostPaths::EventLog);
    if (entry.status == HostEntry::Status::Missing) {
        return {std::nullopt, CheckStatus::Fail, u"No TPM event log"_s};
    }
    if (!entry.ok()) {
        return {std::nullopt, CheckStatus::Unknown, CheckHelpers::unreadable(entry)};
    }
    TcgEventLog::ParseResult parsed = TcgEventLog::parse(entry.data);
    switch (parsed.error) {
    case TcgEventLog::ParseResult::Error::None:
        return {std::move(parsed.log), CheckStatus::Pass, {}};
    case TcgEventLog::ParseResult::Error::NotCryptoAgile:
        return {std::nullopt, CheckStatus::Fail, u"TPM event log is in the old SHA-1-only format"_s};
    case TcgEventLog::ParseResult::Error::Malformed:
        break;
    }
    return {std::nullopt, CheckStatus::Unknown, u"Couldn't parse the TPM event log"_s};
}

SubCheckResult authoritiesWellKnown(const HostSnapshot &snapshot, const QString &title, QJsonArray &raw)
{
    const Loaded loaded = load(snapshot);
    if (!loaded.log) {
        // Without a usable log there's no evidence either way.
        return {title, loaded.status == CheckStatus::Fail ? CheckStatus::Unknown : loaded.status, loaded.detail};
    }

    const QList<TcgEventLog::Event> &events = loaded.log->events;
    QStringList vendors;
    QStringList unknown;
    QStringList unknownDrivers;
    int certificates = 0;
    for (qsizetype i = 0; i < events.size(); ++i) {
        const TcgEventLog::Event &event = events[i];
        if (event.pcr != 7 || event.type != TcgEventLog::EfiVariableAuthority) {
            continue;
        }
        const auto variable = TcgEventLog::variableData(event);
        if (!variable) {
            return {title, CheckStatus::Unknown, u"Couldn't parse the TPM event log"_s};
        }
        QJsonObject entry{{u"variable"_s, variable->name}};
        const auto der = authorityCertificate(variable->data);
        if (!der) {
            entry.insert(u"size"_s, variable->data.size());
            raw.append(entry);
            continue;
        }
        const QByteArray sha256 = Certificate::sha256(*der);
        const QString subject = Certificate::subject(*der);
        const QString name = subject.isEmpty() ? u"certificate %1"_s.arg(Certificate::fingerprint(sha256)) : subject;
        const WellKnownKey *known = WellKnownKeys::builtIn().findFingerprint(sha256);
        const bool driver = verifiedDriver(events, i);
        entry.insert(u"sha256"_s, Certificate::fingerprint(sha256));
        entry.insert(u"subject"_s, subject);
        entry.insert(u"verified"_s, driver ? u"driver"_s : u"boot"_s);
        if (known) {
            entry.insert(u"vendor"_s, known->vendor);
        }
        raw.append(entry);

        if (driver) {
            if (!known) {
                unknownDrivers.append(name);
            }
            continue;
        }
        ++certificates;
        if (!known) {
            unknown.append(name);
        } else if (!vendors.contains(known->vendor)) {
            vendors.append(known->vendor);
        }
    }

    if (!unknown.isEmpty()) {
        return {title, CheckStatus::Fail, CheckHelpers::unrecognisedKeys(unknown)};
    }
    if (certificates == 0) {
        // Nothing verified the boot chain, e.g. because Secure Boot is off.
        return {title, CheckStatus::Fail, u"TPM event log shows no Secure Boot verification"_s};
    }
    QString detail = vendors.join(u", "_s);
    if (!unknownDrivers.isEmpty()) {
        detail += u" · driver key not counted: %1"_s.arg(unknownDrivers.join(u", "_s));
    }
    return {title, CheckStatus::Pass, detail};
}
} // namespace EventLogHelpers
