// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "parsers/KernelCmdline.h"

using namespace Qt::StringLiterals;

namespace
{
// SMBIOS chassis types exempt from the IOMMU requirement.
constexpr int ChassisHandHeld = 11;
constexpr int ChassisTablet = 30;

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
    // Only system-wide passthrough fails. The kernel puts single devices in identity mode for its own
    // reasons: the AMD IOMMU driver does it for every APU's integrated GPU. So some identity groups
    // alongside translated ones are fine.
    if (identity.size() == types.size()) {
        return {title, CheckStatus::Fail, u"All %1 groups use passthrough (identity)"_s.arg(types.size())};
    }
    // Only DMA and DMA-FQ are known to translate DMA, so anything else doesn't count as passing.
    if (!other.isEmpty()) {
        return {title, CheckStatus::Fail, u"Unexpected group types: %1"_s.arg(other.join(u", "_s))};
    }
    if (!identity.isEmpty()) {
        return {title,
                CheckStatus::Pass,
                identity.size() == 1 ? u"Only group %1 uses passthrough"_s.arg(identity[0]) : u"Only groups %1 use passthrough"_s.arg(identity.join(u", "_s))};
    }
    return {title, CheckStatus::Pass, {}};
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
    return {title, CheckStatus::Pass, {}};
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
    if (isNumber && (chassisType == ChassisHandHeld || chassisType == ChassisTablet)) {
        result.status = CheckStatus::NotRequired;
        result.detail = chassisType == ChassisHandHeld ? u"Not required on handhelds"_s : u"Not required on tablets"_s;
        return result;
    }

    // Groups need an IOMMU, and group types need groups. A sub-check whose precondition failed is
    // left out rather than failed again for the same reason. The command line is checked anyway:
    // passthrough there is a separate problem.
    result.subChecks = {enabled(snapshot)};
    if (result.subChecks.last().status == CheckStatus::Pass) {
        result.subChecks.append(groups(snapshot));
        if (result.subChecks.last().status == CheckStatus::Pass) {
            result.subChecks.append(groupTypes(snapshot));
        }
    }
    result.subChecks.append(cmdlinePassthrough(snapshot));
    CheckHelpers::finishFromSubChecks(result);
    return result;
}
