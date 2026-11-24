// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckResult.h"

#include <algorithm>

using namespace Qt::StringLiterals;

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
        return u"pass"_s;
    case CheckStatus::Fail:
        return u"fail"_s;
    case CheckStatus::NotRequired:
        return u"not-required"_s;
    case CheckStatus::NeedsAdmin:
        return u"needs-admin"_s;
    case CheckStatus::Unknown:
        break;
    }
    return u"unknown"_s;
}

QString statusLabel(CheckStatus status)
{
    switch (status) {
    case CheckStatus::Pass:
        return u"Pass"_s;
    case CheckStatus::Fail:
        return u"Fail"_s;
    case CheckStatus::NotRequired:
        return u"Not required"_s;
    case CheckStatus::NeedsAdmin:
        return u"Needs admin"_s;
    case CheckStatus::Unknown:
        break;
    }
    return u"Couldn't check"_s;
}

QString verdictKey(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ready:
        return u"ready"_s;
    case Verdict::NotReady:
        return u"not-ready"_s;
    case Verdict::Incomplete:
        break;
    }
    return u"incomplete"_s;
}

QString verdictLabel(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ready:
        return u"Ready for EAC Linux v2"_s;
    case Verdict::NotReady:
        return u"Not ready"_s;
    case Verdict::Incomplete:
        break;
    }
    return u"Incomplete"_s;
}
