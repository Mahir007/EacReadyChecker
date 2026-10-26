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
} // namespace

namespace EventLogHelpers
{
Loaded load(const HostSnapshot &snapshot)
{
    if (snapshot.privilegedState != PrivilegedState::Granted) {
        return {std::nullopt, CheckStatus::NeedsAdmin, u"Needs admin access to read the TPM event log"_s};
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
        return {std::nullopt, CheckStatus::Fail, u"The TPM event log isn't in the TCG2 crypto-agile format"_s};
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

    QStringList vendors;
    QStringList unknown;
    int certificates = 0;
    for (const TcgEventLog::Event &event : loaded.log->events) {
        if (event.pcr != 7 || event.type != TcgEventLog::EfiVariableAuthority) {
            continue;
        }
        const auto variable = TcgEventLog::variableData(event);
        if (!variable) {
            return {title, CheckStatus::Unknown, u"Couldn't parse a Secure Boot authority in the TPM event log"_s};
        }
        QJsonObject entry{{u"variable"_s, variable->name}};
        const auto der = authorityCertificate(variable->data);
        if (!der) {
            entry.insert(u"size"_s, variable->data.size());
            raw.append(entry);
            continue;
        }
        ++certificates;
        const QByteArray sha256 = Certificate::sha256(*der);
        const QString subject = Certificate::subject(*der);
        entry.insert(u"sha256"_s, Certificate::fingerprint(sha256));
        entry.insert(u"subject"_s, subject);
        if (const WellKnownKey *known = WellKnownKeys::builtIn().findFingerprint(sha256)) {
            entry.insert(u"vendor"_s, known->vendor);
            if (!vendors.contains(known->vendor)) {
                vendors.append(known->vendor);
            }
        } else {
            unknown.append(subject.isEmpty() ? u"certificate %1"_s.arg(Certificate::fingerprint(sha256)) : subject);
        }
        raw.append(entry);
    }

    if (!unknown.isEmpty()) {
        return {title, CheckStatus::Fail, CheckHelpers::unknownKeys(unknown)};
    }
    if (certificates == 0) {
        // Nothing verified the boot chain, e.g. because Secure Boot is off.
        return {title, CheckStatus::Fail, u"The TPM event log shows no Secure Boot verification"_s};
    }
    return {title, CheckStatus::Pass, u"Well-known: %1"_s.arg(vendors.join(u", "_s))};
}
} // namespace EventLogHelpers
