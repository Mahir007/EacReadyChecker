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
        result.detail = u"securityfs isn't mounted at /sys/kernel/security"_s;
        return result;
    }
    if (!lsm.ok()) {
        result.status = CheckStatus::Unknown;
        result.detail = CheckHelpers::unreadable(lsm);
        return result;
    }

    const QStringList active = CheckHelpers::commaList(lsm);
    if (active.contains(u"bpf"_s)) {
        result.status = CheckStatus::Pass;
        result.detail = u"Active LSMs: %1"_s.arg(active.join(u", "_s));
    } else {
        result.status = CheckStatus::Fail;
        result.detail = u"BPF isn't an active LSM (active: %1)"_s.arg(active.isEmpty() ? u"none"_s : active.join(u", "_s));
    }
    return result;
}
