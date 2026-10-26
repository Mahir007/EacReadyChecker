// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "EventLogHelpers.h"

#include <QJsonArray>
#include <QSet>

using namespace Qt::StringLiterals;

namespace
{
// PCRs the firmware log can reproduce. Userspace extends some PCRs after boot without adding to
// this log: IMA uses 10, systemd-pcrphase 11, and systemd's other measurements 15. 16 is for
// debugging, 17–22 for DRTM and 23 for applications.
bool replayable(quint32 pcr)
{
    return pcr <= 14 && pcr != 10 && pcr != 11;
}

// "0–9, 14"
QString pcrRanges(const QList<quint32> &pcrs)
{
    QStringList parts;
    for (qsizetype i = 0; i < pcrs.size();) {
        qsizetype end = i;
        while (end + 1 < pcrs.size() && pcrs[end + 1] == pcrs[end] + 1) {
            ++end;
        }
        parts.append(end == i ? QString::number(pcrs[i]) : u"%1–%2"_s.arg(pcrs[i]).arg(pcrs[end]));
        i = end + 1;
    }
    return parts.join(u", "_s);
}

// Replaying the SHA-256 bank must give the PCR values the TPM reports now.
SubCheckResult logMatchesTpm(const HostSnapshot &snapshot, const EventLogHelpers::Loaded &loaded, QJsonObject &raw)
{
    const QString title = u"Event log matches the TPM"_s;
    if (!loaded.log) {
        return {title, loaded.status, loaded.detail};
    }
    QJsonArray banks;
    for (quint16 algorithm : loaded.log->algorithms) {
        banks.append(TcgEventLog::algorithmName(algorithm));
    }
    raw.insert(u"eventLog"_s, QJsonObject{{u"banks"_s, banks}, {u"events"_s, loaded.log->events.size()}});

    const auto replayed = TcgEventLog::replay(*loaded.log, TcgEventLog::Sha256);
    if (!replayed) {
        return {title, CheckStatus::Unknown, u"The TPM event log has no SHA-256 digests"_s};
    }

    QList<quint32> pcrs;
    for (auto it = replayed->begin(); it != replayed->end(); ++it) {
        if (replayable(it.key())) {
            pcrs.append(it.key());
        }
    }
    if (pcrs.isEmpty()) {
        return {title, CheckStatus::Fail, u"The TPM event log has no boot measurements"_s};
    }

    QHash<QString, HostEntry> tpmPcrs;
    const QList<HostEntry> entries = snapshot.matches(HostPaths::TpmPcrsSha256);
    for (const HostEntry &entry : entries) {
        tpmPcrs.insert(entry.path.section(u'/', -1), entry);
    }
    if (tpmPcrs.isEmpty()) {
        // /sys/class/tpm/tpm0/pcr-sha256 appeared in Linux 5.12.
        return {title, CheckStatus::Unknown, u"This kernel doesn't report the TPM's PCR values"_s};
    }

    QList<quint32> mismatched;
    QJsonObject replay;
    for (quint32 pcr : std::as_const(pcrs)) {
        const QByteArray value = replayed->value(pcr);
        const HostEntry entry = tpmPcrs.value(QString::number(pcr));
        if (!entry.ok()) {
            return {title, CheckStatus::Unknown, u"Couldn't read PCR %1"_s.arg(pcr)};
        }
        const bool match = QByteArray::fromHex(entry.trimmed()) == value;
        replay.insert(QString::number(pcr), QJsonObject{{u"log"_s, QString::fromLatin1(value.toHex())}, {u"match"_s, match}});
        if (!match) {
            mismatched.append(pcr);
        }
    }
    raw.insert(u"pcrReplay"_s, replay);

    if (mismatched.size() == 1) {
        return {title, CheckStatus::Fail, u"PCR %1 doesn't match the TPM event log"_s.arg(mismatched.first())};
    }
    if (!mismatched.isEmpty()) {
        return {title, CheckStatus::Fail, u"PCRs %1 don't match the TPM event log"_s.arg(pcrRanges(mismatched))};
    }
    return {title, CheckStatus::Pass, u"PCRs %1 match the TPM event log"_s.arg(pcrRanges(pcrs))};
}

// The boot manager measured the kernel.
SubCheckResult kernelMeasured(const EventLogHelpers::Loaded &loaded, QJsonObject &raw)
{
    const QString title = u"Boot manager measured the kernel"_s;
    if (!loaded.log) {
        return {title, loaded.status, loaded.detail};
    }
    const QList<TcgEventLog::Event> &events = loaded.log->events;

    // GRUB: the kernel image into PCR 9 (event text is its path), and "kernel_cmdline: <path> …"
    // into PCR 8.
    QSet<QString> grubFiles;
    for (const TcgEventLog::Event &event : events) {
        if (event.pcr == 9 && event.type == TcgEventLog::Ipl) {
            grubFiles.insert(TcgEventLog::text(event));
        }
    }
    const QString cmdlinePrefix = u"kernel_cmdline: "_s;
    for (const TcgEventLog::Event &event : events) {
        if (event.pcr != 8 || event.type != TcgEventLog::Ipl) {
            continue;
        }
        const QString text = TcgEventLog::text(event);
        const QString kernel = text.startsWith(cmdlinePrefix) ? text.mid(cmdlinePrefix.size()).section(u' ', 0, 0) : QString();
        if (!kernel.isEmpty() && grubFiles.contains(kernel)) {
            raw.insert(u"kernelMeasuredBy"_s, u"grub"_s);
            raw.insert(u"measuredKernel"_s, kernel);
            return {title, CheckStatus::Pass, u"Kernel measured by GRUB"_s};
        }
    }

    int applications = 0;
    bool ukiSections = false;
    for (const TcgEventLog::Event &event : events) {
        applications += event.pcr == 4 && event.type == TcgEventLog::EfiBootServicesApplication;
        ukiSections = ukiSections || (event.pcr == 11 && event.type == TcgEventLog::Ipl && TcgEventLog::text(event) == u".linux");
    }
    // systemd-stub: the UKI is an EFI application in PCR 4, and the stub measures its sections,
    // including the kernel (".linux"), into PCR 11.
    if (ukiSections && applications > 0) {
        raw.insert(u"kernelMeasuredBy"_s, u"systemd-stub"_s);
        return {title, CheckStatus::Pass, u"Kernel measured by systemd-stub (UKI)"_s};
    }
    // Other boot managers load the kernel as an EFI application, which the firmware (or shim)
    // measures into PCR 4 after the boot manager's own entry.
    if (applications >= 2) {
        raw.insert(u"kernelMeasuredBy"_s, u"efi-application"_s);
        return {title, CheckStatus::Pass, u"Kernel measured as an EFI application"_s};
    }
    return {title, CheckStatus::Fail, u"No kernel measurement in the TPM event log"_s};
}
} // namespace

QList<HostRequest> MeasuredBootCheck::requests() const
{
    return {HostRequest::read(HostPaths::TpmPcrsSha256)};
}

CheckResult MeasuredBootCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    const EventLogHelpers::Loaded loaded = EventLogHelpers::load(snapshot);
    result.subChecks = {
        logMatchesTpm(snapshot, loaded, result.raw),
        kernelMeasured(loaded, result.raw),
    };
    CheckHelpers::finishFromSubChecks(result, result.subChecks[1].detail + u", event log matches the TPM"_s);
    return result;
}
