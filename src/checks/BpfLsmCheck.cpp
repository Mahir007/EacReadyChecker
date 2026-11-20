// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"

using namespace Qt::StringLiterals;

CheckResult BpfLsmCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    const HostEntry lsm = snapshot.file(HostPaths::Lsm);

    if (lsm.status == HostEntry::Status::Missing) {
        result.status = CheckStatus::Unknown;
        result.detail = u"securityfs isn't mounted"_s;
        return result;
    }
    if (!lsm.ok()) {
        result.status = CheckStatus::Unknown;
        result.detail = CheckHelpers::unreadable(lsm);
        return result;
    }

    if (CheckHelpers::commaList(lsm).contains(u"bpf"_s)) {
        result.status = CheckStatus::Pass;
    } else {
        result.status = CheckStatus::Fail;
        result.detail = u"BPF isn't an active LSM"_s;
    }
    return result;
}
