// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"

using namespace Qt::StringLiterals;

QList<HostRequest> TpmCheck::requests() const
{
    return {HostRequest::read(HostPaths::TpmVersionMajor), HostRequest::list(HostPaths::TpmDevice)};
}

CheckResult TpmCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    const HostEntry version = snapshot.file(HostPaths::TpmVersionMajor);
    const QList<HostEntry> device = snapshot.matches(HostRequest::list(HostPaths::TpmDevice));
    const HostEntry::Status deviceStatus = device.isEmpty() ? HostEntry::Status::NotRead : device.first().status;

    if (version.ok()) {
        const QByteArray major = version.trimmed();
        if (major == "2") {
            result.status = CheckStatus::Pass;
        } else if (major == "1") {
            result.status = CheckStatus::Fail;
            result.detail = u"TPM 1.2 found, 2.0 required"_s;
        } else {
            result.status = CheckStatus::Unknown;
            result.detail = u"Unexpected TPM version “%1”"_s.arg(QString::fromUtf8(major));
        }
    } else if (version.status == HostEntry::Status::Missing && deviceStatus == HostEntry::Status::Missing) {
        result.status = CheckStatus::Fail;
        result.detail = u"No TPM found"_s;
    } else if (version.status == HostEntry::Status::Missing && deviceStatus == HostEntry::Status::Present) {
        // tpm_version_major appeared in Linux 5.6.
        result.status = CheckStatus::Unknown;
        result.detail = u"Found a TPM, but this kernel doesn't report its version"_s;
    } else {
        result.status = CheckStatus::Unknown;
        result.detail = CheckHelpers::unreadable(version);
    }
    return result;
}
