// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "core/Certificate.h"
#include "core/WellKnownKeys.h"
#include "parsers/EfiSignatureList.h"
#include "parsers/EfiVariable.h"

#include <QJsonArray>

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

// "MokListRT", "MokListRT1", … from an efivars path.
QString variableName(const QString &path)
{
    return path.section(u'/', -1).section(u'-', 0, 0);
}

// Every MOK must be a well-known certificate. shim also copies its built-in vendor certificates
// into MokListRT, so a stock Fedora or Ubuntu lists its own CA here. Any other entry, including a
// hash, isn't a well-known key and fails.
SubCheckResult moksWellKnown(const HostSnapshot &snapshot, QJsonArray &rawEntries)
{
    const QString title = u"Module-signing keys (MOKs) are well-known"_s;
    const QList<HostEntry> variables = snapshot.matches(HostPaths::MokListRt);
    for (const HostEntry &variable : std::as_const(variables)) {
        if (!variable.ok()) {
            return {title, CheckStatus::Unknown, CheckHelpers::unreadable(variable)};
        }
    }

    QStringList vendors;
    QStringList unknown;
    for (const HostEntry &variable : std::as_const(variables)) {
        const auto data = EfiVariable::data(variable.data);
        const auto entries = data ? EfiSignatureList::parse(*data) : std::nullopt;
        if (!entries) {
            return {title, CheckStatus::Unknown, u"Couldn't parse %1"_s.arg(variableName(variable.path))};
        }
        for (const EfiSignatureList::Entry &entry : *entries) {
            QJsonObject raw{{u"variable"_s, variableName(variable.path)}, {u"type"_s, EfiSignatureList::typeName(entry.type)}};
            if (entry.type != EfiSignatureList::CertX509) {
                raw.insert(u"data"_s, QString::fromLatin1(entry.data.toHex()));
                rawEntries.append(raw);
                unknown.append(entry.type == EfiSignatureList::CertSha256 ? u"SHA-256 hash %1"_s.arg(QString::fromLatin1(entry.data.toHex().left(16)))
                                                                           : u"%1 entry"_s.arg(EfiSignatureList::typeName(entry.type)));
                continue;
            }
            const QByteArray sha256 = Certificate::sha256(entry.data);
            const QString subject = Certificate::subject(entry.data);
            raw.insert(u"sha256"_s, Certificate::fingerprint(sha256));
            raw.insert(u"subject"_s, subject);
            const WellKnownKey *known = WellKnownKeys::builtIn().findFingerprint(sha256);
            if (known) {
                raw.insert(u"vendor"_s, known->vendor);
                if (!vendors.contains(known->vendor)) {
                    vendors.append(known->vendor);
                }
            } else {
                unknown.append(subject.isEmpty() ? u"certificate %1"_s.arg(Certificate::fingerprint(sha256)) : subject);
            }
            rawEntries.append(raw);
        }
    }

    if (!unknown.isEmpty()) {
        QString detail = (unknown.size() == 1 ? u"Unknown key: %1"_s : u"Unknown keys: %1"_s).arg(unknown.first());
        if (unknown.size() > 1) {
            detail += u" (+%1 more)"_s.arg(unknown.size() - 1);
        }
        return {title, CheckStatus::Fail, detail};
    }
    if (vendors.isEmpty()) {
        return {title, CheckStatus::Pass, u"No MOKs enrolled"_s};
    }
    return {title, CheckStatus::Pass, u"Well-known: %1"_s.arg(vendors.join(u", "_s))};
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
    QJsonArray moks;
    result.subChecks = {
        CheckHelpers::notImplemented(u"Kernel signed by a well-known key"_s, snapshot, true), // Phase 4
        unsignedModules(snapshot),
        moksWellKnown(snapshot, moks),
    };
    result.raw.insert(u"mokList"_s, moks);
    CheckHelpers::finishFromSubChecks(result, u"Kernel and modules signed by well-known keys"_s);
    return result;
}
