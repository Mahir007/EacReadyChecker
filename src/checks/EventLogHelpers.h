// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"
#include "core/HostSnapshot.h"
#include "parsers/TcgEventLog.h"

#include <QJsonArray>

#include <optional>

// The TPM event log as the checks that need admin access see it.
namespace EventLogHelpers
{
struct Loaded {
    std::optional<TcgEventLog::Log> log;
    // Why there's no log: Needs admin before access is granted, Fail when the log is missing or
    // isn't crypto-agile, Unknown when it can't be read or parsed.
    CheckStatus status = CheckStatus::Unknown;
    QString detail;
};

Loaded load(const HostSnapshot &snapshot);

// Every X.509 certificate in a PCR 7 EV_EFI_VARIABLE_AUTHORITY event must be a well-known key.
// Those events record which db, shim or MOK certificate verified each stage of the boot chain.
// Other authority data (e.g. shim's SbatLevel) isn't a key and isn't judged, and neither are
// the keys that verified firmware drivers such as option ROMs. Each
// authority is appended to `raw`.
SubCheckResult authoritiesWellKnown(const HostSnapshot &snapshot, const QString &title, QJsonArray &raw);
} // namespace EventLogHelpers
