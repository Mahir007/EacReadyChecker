// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckResult.h"

#include <algorithm>

namespace
{
template<typename Results>
bool hasStatus(const Results &results, CheckStatus status)
{
    return std::any_of(results.begin(), results.end(), [status](const auto &result) {
        return result.status == status;
    });
}
} // namespace

Verdict overallVerdict(const QList<CheckResult> &results)
{
    if (hasStatus(results, CheckStatus::Fail)) {
        return Verdict::NotReady;
    }
    if (hasStatus(results, CheckStatus::NeedsAdmin) || hasStatus(results, CheckStatus::Unknown)) {
        return Verdict::Incomplete;
    }
    return Verdict::Ready;
}

CheckStatus combineStatus(const QList<SubCheckResult> &subChecks)
{
    for (CheckStatus s : {CheckStatus::Fail, CheckStatus::NeedsAdmin, CheckStatus::Unknown, CheckStatus::Pass}) {
        if (hasStatus(subChecks, s)) {
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
