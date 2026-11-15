// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "EventLogHelpers.h"
#include "parsers/EfiVariable.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace
{
bool bootedWithUefi(const HostSnapshot &snapshot)
{
    return snapshot.matches(HostRequest::list(HostPaths::EfiDir)).value(0).status == HostEntry::Status::Present;
}

// Reads a one-byte EFI variable. Returns the sub-check result for the cases that aren't 0 or 1.
std::optional<SubCheckResult> readFlag(const HostSnapshot &snapshot, const QString &title, const QString &path, std::optional<quint8> &value)
{
    const HostEntry entry = snapshot.file(path);
    if (entry.status == HostEntry::Status::Missing) {
        if (!bootedWithUefi(snapshot)) {
            return SubCheckResult{title, CheckStatus::Fail, u"Not booted in UEFI mode"_s};
        }
        return SubCheckResult{title, CheckStatus::Unknown, u"%1 not found"_s.arg(path.section(u'/', -1).section(u'-', 0, 0))};
    }
    if (!entry.ok()) {
        return SubCheckResult{title, CheckStatus::Unknown, CheckHelpers::unreadable(entry)};
    }
    value = EfiVariable::byteValue(entry.data);
    if (!value || *value > 1) {
        return SubCheckResult{title, CheckStatus::Unknown, u"Unexpected contents in %1"_s.arg(path)};
    }
    return std::nullopt;
}

SubCheckResult enabled(const HostSnapshot &snapshot)
{
    const QString title = u"Enabled"_s;
    std::optional<quint8> value;
    if (auto problem = readFlag(snapshot, title, HostPaths::SecureBoot, value)) {
        return *problem;
    }
    return *value == 1 ? SubCheckResult{title, CheckStatus::Pass, {}} : SubCheckResult{title, CheckStatus::Fail, u"Secure Boot is off"_s};
}

SubCheckResult notInSetupMode(const HostSnapshot &snapshot)
{
    const QString title = u"Not in Setup Mode"_s;
    std::optional<quint8> value;
    if (auto problem = readFlag(snapshot, title, HostPaths::SetupMode, value)) {
        return *problem;
    }
    return *value == 0 ? SubCheckResult{title, CheckStatus::Pass, {}} : SubCheckResult{title, CheckStatus::Fail, u"Firmware is in Setup Mode"_s};
}
} // namespace

QList<HostRequest> SecureBootCheck::requests() const
{
    return {
        HostRequest::list(HostPaths::EfiDir),
        HostRequest::read(HostPaths::SecureBoot),
        HostRequest::read(HostPaths::SetupMode),
    };
}

CheckResult SecureBootCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    QJsonArray authorities;
    result.subChecks = {
        enabled(snapshot),
        notInSetupMode(snapshot),
        // Only the keys used for this boot count. db may hold others.
        EventLogHelpers::authoritiesWellKnown(snapshot, u"Only well-known keys used to boot"_s, authorities),
    };
    result.raw.insert(u"bootAuthorities"_s, authorities);
    CheckHelpers::finishFromSubChecks(result);
    return result;
}
