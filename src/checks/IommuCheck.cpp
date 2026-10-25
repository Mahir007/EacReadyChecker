// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "parsers/KernelCmdline.h"

using namespace Qt::StringLiterals;

namespace
{
// SMBIOS chassis types exempt from the IOMMU requirement.
constexpr int kChassisHandHeld = 11;
constexpr int kChassisTablet = 30;

QStringList names(const QList<HostEntry> &entries)
{
    QStringList out;
    for (const HostEntry &entry : entries) {
        if (entry.status == HostEntry::Status::Present) {
            out.append(entry.path.section(u'/', -1));
        }
    }
    return out;
}

SubCheckResult enabled(const HostSnapshot &snapshot)
{
    const QString title = u"IOMMU active"_s;
    const QStringList units = names(snapshot.matches(HostRequest::list(HostPaths::IommuDevices)));
    if (units.isEmpty()) {
        return {title, CheckStatus::Fail, u"No IOMMU found (off in the firmware or the kernel)"_s};
    }
    return {title, CheckStatus::Pass, units.join(u", "_s)};
}

SubCheckResult groups(const HostSnapshot &snapshot)
{
    const QString title = u"IOMMU groups exist"_s;
    const qsizetype count = names(snapshot.matches(HostRequest::list(HostPaths::IommuGroups))).size();
    if (count == 0) {
        return {title, CheckStatus::Fail, u"No IOMMU groups"_s};
    }
    return {title, CheckStatus::Pass, count == 1 ? u"1 group"_s : u"%1 groups"_s.arg(count)};
}

SubCheckResult groupTypes(const HostSnapshot &snapshot)
{
    const QString title = u"No passthrough (group types)"_s;
    const qsizetype groupCount = names(snapshot.matches(HostRequest::list(HostPaths::IommuGroups))).size();
    const QList<HostEntry> types = snapshot.matches(HostPaths::IommuGroupTypes);
    if (groupCount == 0) {
        return {title, CheckStatus::Fail, u"No IOMMU groups"_s};
    }
    if (types.size() < groupCount) {
        // The type attribute appeared in Linux 5.11.
        return {title, CheckStatus::Unknown, u"This kernel doesn't report IOMMU group types"_s};
    }

    QStringList identity;
    QStringList other;
    for (const HostEntry &entry : types) {
        if (!entry.ok()) {
            return {title, CheckStatus::Unknown, CheckHelpers::unreadable(entry)};
        }
        const QString group = entry.path.section(u'/', -2, -2);
        const QByteArray type = entry.trimmed();
        if (type == "identity") {
            identity.append(group);
        } else if (type != "DMA" && type != "DMA-FQ") {
            other.append(u"%1 (%2)"_s.arg(group, QString::fromUtf8(type)));
        }
    }
    // Only system-wide passthrough fails. The kernel puts single devices in
    // identity mode for its own reasons, e.g. the AMD IOMMU driver does it for every APU's
    // integrated GPU, so some identity groups alongside translated ones are fine.
    if (identity.size() == types.size()) {
        return {title, CheckStatus::Fail, u"All %1 groups use passthrough (identity)"_s.arg(types.size())};
    }
    // Only DMA and DMA-FQ are known to translate DMA, so anything else doesn't count as passing.
    if (!other.isEmpty()) {
        return {title, CheckStatus::Fail, u"Unexpected group types: %1"_s.arg(other.join(u", "_s))};
    }
    if (!identity.isEmpty()) {
        const QString which = identity.size() == 1 ? u"group %1 uses"_s.arg(identity[0]) : u"groups %1 use"_s.arg(identity.join(u", "_s));
        return {title,
                CheckStatus::Pass,
                u"%1 of %2 groups translate DMA; %3 identity (a single device, not system-wide)"_s.arg(types.size() - identity.size()).arg(types.size()).arg(which)};
    }
    return {title, CheckStatus::Pass, u"All %1 groups translate DMA"_s.arg(types.size())};
}

SubCheckResult cmdlinePassthrough(const HostSnapshot &snapshot)
{
    const QString title = u"No passthrough (kernel command line)"_s;
    const HostEntry cmdline = snapshot.file(HostPaths::Cmdline);
    if (!cmdline.ok()) {
        return {title, CheckStatus::Unknown, CheckHelpers::unreadable(cmdline)};
    }

    // Later parameters override earlier ones, as in the kernel.
    bool passthrough = false;
    QString why;
    const KernelCmdline parsed = KernelCmdline::parse(cmdline.data);
    for (const KernelCmdline::Param &param : parsed.params()) {
        if (!param.value) {
            continue;
        }
        if (param.name == u"iommu") {
            const QStringList options = param.value->split(u',');
            for (const QString &option : options) {
                if (option == u"pt") {
                    passthrough = true;
                    why = u"iommu=%1"_s.arg(*param.value);
                } else if (option == u"nopt") {
                    passthrough = false;
                }
            }
        } else if (param.name == u"iommu.passthrough") {
            if (const auto on = KernelCmdline::parseBool(*param.value)) {
                passthrough = *on;
                why = u"iommu.passthrough=%1"_s.arg(*param.value);
            }
        }
    }
    if (passthrough) {
        return {title, CheckStatus::Fail, u"Passthrough requested (%1)"_s.arg(why)};
    }
    return {title, CheckStatus::Pass, u"Not requested"_s};
}
} // namespace

QList<HostRequest> IommuCheck::requests() const
{
    return {
        HostRequest::read(HostPaths::ChassisType),
        HostRequest::list(HostPaths::IommuDevices),
        HostRequest::list(HostPaths::IommuGroups),
        HostRequest::read(HostPaths::IommuGroupTypes),
        HostRequest::read(HostPaths::Cmdline),
    };
}

CheckResult IommuCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;

    // Handhelds and tablets are exempt. Missing DMI data means no exemption.
    const HostEntry chassis = snapshot.file(HostPaths::ChassisType);
    bool isNumber = false;
    const int chassisType = chassis.ok() ? chassis.trimmed().toInt(&isNumber) : 0;
    if (isNumber && (chassisType == kChassisHandHeld || chassisType == kChassisTablet)) {
        const QString kind = chassisType == kChassisHandHeld ? u"Handheld"_s : u"Tablet"_s;
        result.status = CheckStatus::NotRequired;
        result.detail = u"Not required on this device (%1, chassis type %2)"_s.arg(kind).arg(chassisType);
        return result;
    }

    result.subChecks = {enabled(snapshot), groups(snapshot), groupTypes(snapshot), cmdlinePassthrough(snapshot)};
    CheckHelpers::finishFromSubChecks(result, u"Enabled, no passthrough"_s);
    return result;
}
