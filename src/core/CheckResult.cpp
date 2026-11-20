// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckResult.h"

#include <algorithm>

Verdict overallVerdict(const QList<CheckResult> &results)
{
    const auto has = [&results](CheckStatus s) {
        return std::any_of(results.begin(), results.end(), [s](const CheckResult &r) {
            return r.status == s;
        });
    };
    if (has(CheckStatus::Fail)) {
        return Verdict::NotReady;
    }
    if (has(CheckStatus::NeedsAdmin) || has(CheckStatus::Unknown)) {
        return Verdict::Incomplete;
    }
    return Verdict::Ready;
}

CheckStatus combineStatus(const QList<SubCheckResult> &subChecks)
{
    const auto has = [&subChecks](CheckStatus s) {
        return std::any_of(subChecks.begin(), subChecks.end(), [s](const SubCheckResult &r) {
            return r.status == s;
        });
    };
    for (CheckStatus s : {CheckStatus::Fail, CheckStatus::NeedsAdmin, CheckStatus::Unknown, CheckStatus::Pass}) {
        if (has(s)) {
            return s;
        }
    }
    return subChecks.isEmpty() ? CheckStatus::Unknown : CheckStatus::NotRequired;
}

QString statusKey(CheckStatus status)
{
    switch (status) {
    case CheckStatus::Pass:
        return QStringLiteral("pass");
    case CheckStatus::Fail:
        return QStringLiteral("fail");
    case CheckStatus::NotRequired:
        return QStringLiteral("not-required");
    case CheckStatus::NeedsAdmin:
        return QStringLiteral("needs-admin");
    case CheckStatus::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

QString statusLabel(CheckStatus status)
{
    switch (status) {
    case CheckStatus::Pass:
        return QStringLiteral("Pass");
    case CheckStatus::Fail:
        return QStringLiteral("Fail");
    case CheckStatus::NotRequired:
        return QStringLiteral("Not required");
    case CheckStatus::NeedsAdmin:
        return QStringLiteral("Needs admin");
    case CheckStatus::Unknown:
        break;
    }
    return QStringLiteral("Couldn't check");
}

QString verdictKey(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ready:
        return QStringLiteral("ready");
    case Verdict::NotReady:
        return QStringLiteral("not-ready");
    case Verdict::Incomplete:
        break;
    }
    return QStringLiteral("incomplete");
}

QString verdictLabel(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ready:
        return QStringLiteral("Ready for EAC Linux v2");
    case Verdict::NotReady:
        return QStringLiteral("Not ready");
    case Verdict::Incomplete:
        break;
    }
    return QStringLiteral("Incomplete");
}
