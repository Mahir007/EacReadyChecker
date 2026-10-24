// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"

using namespace Qt::StringLiterals;

namespace
{
// Documentation/admin-guide/tainted-kernels.rst, bit 0 upwards.
constexpr char kTaintLetters[] = "PFSRMBUDAWCIOELKXTN";
constexpr int kTaintUnsignedModule = 13; // 'E'

QString taintLetters(quint64 taint)
{
    QStringList letters;
    for (int bit = 0; bit < 64; ++bit) {
        if (!(taint & (quint64(1) << bit))) {
            continue;
        }
        letters.append(bit < int(sizeof(kTaintLetters)) - 1 ? QString(QLatin1Char(kTaintLetters[bit])) : u"bit %1"_s.arg(bit));
    }
    return letters.join(u", "_s);
}

SubCheckResult unsignedModules(const HostSnapshot &snapshot)
{
    const QString title = u"No unsigned modules loaded"_s;
    const HostEntry tainted = snapshot.file(HostPaths::Tainted);
    if (!tainted.ok()) {
        return {title, CheckStatus::Unknown, CheckHelpers::unreadable(tainted)};
    }
    bool ok = false;
    const quint64 taint = tainted.trimmed().toULongLong(&ok);
    if (!ok) {
        return {title, CheckStatus::Unknown, u"Unexpected taint value “%1”"_s.arg(QString::fromUtf8(tainted.trimmed()))};
    }
    if (taint & (quint64(1) << kTaintUnsignedModule)) {
        return {title, CheckStatus::Fail, u"An unsigned module is loaded (taint flags: %1)"_s.arg(taintLetters(taint))};
    }
    // Out-of-tree modules (O) alone are fine as long as they're signed.
    return {title, CheckStatus::Pass, taint == 0 ? u"Kernel isn't tainted"_s : u"No unsigned modules (taint flags: %1)"_s.arg(taintLetters(taint))};
}
} // namespace

QList<HostRequest> KernelSigningCheck::requests() const
{
    return {
        HostRequest::read(HostPaths::Tainted),
        HostRequest::read(HostPaths::MokListRt),
    };
}

CheckResult KernelSigningCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    result.subChecks = {
        CheckHelpers::notImplemented(u"Kernel signed by a well-known key"_s, snapshot, true), // Phase 4
        unsignedModules(snapshot),
        CheckHelpers::notImplemented(u"Module-signing keys (MOKs) are well-known"_s, snapshot, false), // Phase 3
    };
    CheckHelpers::finishFromSubChecks(result, u"Kernel and modules signed by well-known keys"_s);
    return result;
}
