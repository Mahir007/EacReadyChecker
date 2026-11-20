// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"

using namespace Qt::StringLiterals;

CheckResult KexecCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    const HostEntry disabled = snapshot.file(HostPaths::KexecLoadDisabled);

    if (disabled.ok()) {
        const QByteArray value = disabled.trimmed();
        if (value == "1") {
            result.status = CheckStatus::Pass;
        } else if (value == "0") {
            result.status = CheckStatus::Fail;
            result.detail = u"Kexec is allowed"_s;
        } else {
            result.status = CheckStatus::Unknown;
            result.detail = u"Unexpected kexec_load_disabled value “%1”"_s.arg(QString::fromUtf8(value));
        }
    } else if (disabled.status == HostEntry::Status::Missing) {
        // The sysctl only exists when the kernel is built with kexec (CONFIG_KEXEC_CORE). Without
        // it, kexec can't be used at all, which is what the requirement asks for.
        result.status = CheckStatus::Pass;
        result.detail = u"Kexec isn't built into this kernel"_s;
    } else {
        result.status = CheckStatus::Unknown;
        result.detail = CheckHelpers::unreadable(disabled);
    }
    return result;
}
