// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckHelpers.h"
#include "Checks.h"
#include "EventLogHelpers.h"
#include "core/Certificate.h"
#include "core/WellKnownKeys.h"
#include "parsers/EfiSignatureList.h"
#include "parsers/EfiVariable.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace
{
// Documentation/admin-guide/tainted-kernels.rst
constexpr int TaintUnsignedModule = 13; // 'E'

// "name (flags)" for each loaded module that tainted the kernel. /proc/modules ends such a line
// with its taint letters in parentheses, plus '+' or '-' while it loads or unloads.
QStringList taintedModules(const QByteArray &modules)
{
    QStringList tainted;
    const QList<QByteArray> lines = modules.split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> fields = line.simplified().split(' ');
        const QByteArray &last = fields.last();
        if (fields.size() < 2 || !last.startsWith('(') || !last.endsWith(')')) {
            continue;
        }
        QByteArray flags = last.mid(1, last.size() - 2);
        flags.replace('+', "").replace('-', "");
        if (!flags.isEmpty()) {
            tainted.append(u"%1 (%2)"_s.arg(QString::fromUtf8(fields.first()), QString::fromLatin1(flags)));
        }
    }
    return tainted;
}

// The kernel's E flag stays set after the module that set it is unloaded. Each module's own flags
// in /proc/modules tell whether it's still loaded.
SubCheckResult unsignedModules(const HostSnapshot &snapshot, QJsonArray &rawModules)
{
    const QString title = u"No unsigned modules loaded"_s;
    const HostEntry modules = snapshot.file(HostPaths::Modules);
    QStringList unsignedNames;
    QStringList outOfTree;
    if (modules.ok()) {
        const QStringList tainted = taintedModules(modules.data);
        for (const QString &module : tainted) {
            rawModules.append(module);
            const QString flags = module.section(u'(', 1);
            const QString name = module.section(u' ', 0, 0);
            if (flags.contains(u'E')) {
                unsignedNames.append(name);
            }
            if (flags.contains(u'O')) {
                outOfTree.append(name);
            }
        }
    }

    const HostEntry tainted = snapshot.file(HostPaths::Tainted);
    if (!tainted.ok()) {
        return {title, CheckStatus::Unknown, CheckHelpers::unreadable(tainted)};
    }
    bool ok = false;
    const quint64 taint = tainted.trimmed().toULongLong(&ok);
    if (!ok) {
        return {title, CheckStatus::Unknown, u"Unexpected taint value “%1”"_s.arg(QString::fromUtf8(tainted.trimmed()))};
    }
    if (!(taint & (quint64(1) << TaintUnsignedModule))) {
        // Out-of-tree modules (O) are fine as long as they're signed. Name them, since they're the
        // ones whose keys the MOK sub-check is about. Other taint flags don't concern signing.
        return {title, CheckStatus::Pass, outOfTree.isEmpty() ? QString() : u"Out-of-tree: %1"_s.arg(outOfTree.join(u", "_s))};
    }
    if (!unsignedNames.isEmpty()) {
        return {title,
                CheckStatus::Fail,
                u"%1: %2"_s.arg(unsignedNames.size() == 1 ? u"Unsigned module loaded"_s : u"Unsigned modules loaded"_s, unsignedNames.join(u", "_s))};
    }
    if (modules.ok()) {
        // The flag can't be cleared, so this still fails until a reboot.
        return {title, CheckStatus::Fail, u"An unsigned module has been loaded and unloaded since boot"_s};
    }
    return {title, CheckStatus::Fail, u"An unsigned module has been loaded since boot"_s};
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
            return {title, CheckStatus::Unknown, u"Couldn't parse %1"_s.arg(EfiVariable::nameFromPath(variable.path))};
        }
        for (const EfiSignatureList::Entry &entry : *entries) {
            QJsonObject raw{{u"variable"_s, EfiVariable::nameFromPath(variable.path)}, {u"type"_s, EfiSignatureList::typeName(entry.type)}};
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
        return {title, CheckStatus::Fail, CheckHelpers::unrecognisedKeys(unknown)};
    }
    if (vendors.isEmpty()) {
        return {title, CheckStatus::Pass, u"No MOKs enrolled"_s};
    }
    return {title, CheckStatus::Pass, vendors.join(u", "_s)};
}
} // namespace

QList<HostRequest> KernelSigningCheck::requests() const
{
    return {
        HostRequest::read(HostPaths::Tainted),
        HostRequest::read(HostPaths::Modules),
        HostRequest::read(HostPaths::MokListRt),
    };
}

CheckResult KernelSigningCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    QJsonArray authorities;
    QJsonArray moks;
    QJsonArray modules;
    result.subChecks = {
        // shim logs each certificate once, so the authorities that verified GRUB and the kernel
        // can't be told apart. Every authority of the boot chain is judged.
        EventLogHelpers::authoritiesWellKnown(snapshot, u"Kernel signed by a well-known key"_s, authorities),
        unsignedModules(snapshot, modules),
        moksWellKnown(snapshot, moks),
    };
    result.raw.insert(u"bootAuthorities"_s, authorities);
    result.raw.insert(u"mokList"_s, moks);
    // Only the modules that tainted the kernel, not the whole list.
    if (snapshot.file(HostPaths::Modules).ok()) {
        result.raw.insert(HostPaths::Modules, modules);
    }
    CheckHelpers::finishFromSubChecks(result);
    return result;
}
