// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "parsers/KernelCmdline.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

QList<HostRequest> LockdownCheck::requests() const
{
    return {
        HostRequest::read(HostPaths::Lockdown),
        HostRequest::read(HostPaths::Lsm),
        HostRequest::read(HostPaths::Cmdline),
    };
}

CheckResult LockdownCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    const HostEntry lockdown = snapshot.file(HostPaths::Lockdown);

    if (lockdown.status == HostEntry::Status::Missing) {
        // The lsm file shows securityfs is there, so the lockdown LSM isn't running: either it
        // isn't built in, or an lsm= on the command line leaves it out.
        if (snapshot.file(HostPaths::Lsm).ok()) {
            result.status = CheckStatus::Fail;
            result.detail = u"Lockdown LSM isn't active"_s;
        } else {
            result.status = CheckStatus::Unknown;
            result.detail = u"securityfs isn't mounted"_s;
        }
        return result;
    }
    if (!lockdown.ok()) {
        result.status = CheckStatus::Unknown;
        result.detail = CheckHelpers::unreadable(lockdown);
        return result;
    }

    // e.g. "none [integrity] confidentiality"
    static const QRegularExpression active(u"\\[(\\w+)\\]"_s);
    const QRegularExpressionMatch match = active.match(QString::fromUtf8(lockdown.trimmed()));
    if (!match.hasMatch()) {
        result.status = CheckStatus::Unknown;
        result.detail = u"Unexpected lockdown state “%1”"_s.arg(QString::fromUtf8(lockdown.trimmed()));
        return result;
    }

    const QString mode = match.captured(1);
    if (mode == u"integrity") {
        result.status = CheckStatus::Pass;
        // How it was enabled doesn't matter, but say so when we can tell.
        const HostEntry cmdline = snapshot.file(HostPaths::Cmdline);
        if (cmdline.ok() && KernelCmdline::parse(cmdline.data).values(u"lockdown").contains(u"integrity"_s)) {
            result.detail = u"Set on the kernel command line"_s;
        }
    } else if (mode == u"none") {
        result.status = CheckStatus::Fail;
        result.detail = u"Lockdown is off"_s;
    } else {
        result.status = CheckStatus::Fail;
        result.detail = u"Lockdown is in %1 mode"_s.arg(mode);
    }
    return result;
}
