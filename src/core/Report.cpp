// SPDX-License-Identifier: GPL-3.0-or-later
#include "Report.h"

#include "eacheck_version.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringDecoder>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
constexpr qsizetype kMaxInlineText = 4096;
constexpr qsizetype kMaxInlineHex = 256;

bool isPrintableText(const QByteArray &data)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(data);
    if (decoder.hasError()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](QChar c) {
        return c.isPrint() || c == u'\n' || c == u'\t';
    });
}
} // namespace

QString requirementsAsOf()
{
    // Update when the checks are re-verified against Epic's article.
    return u"October 2026"_s;
}

QJsonValue rawValue(const HostEntry &entry)
{
    switch (entry.status) {
    case HostEntry::Status::Ok:
        if (entry.data.size() <= kMaxInlineText && isPrintableText(entry.data)) {
            return QString::fromUtf8(entry.data.trimmed());
        }
        if (entry.data.size() <= kMaxInlineHex) {
            return QJsonObject{{u"hex"_s, QString::fromLatin1(entry.data.toHex())}};
        }
        return QJsonObject{
            {u"size"_s, entry.data.size()},
            {u"sha256"_s, QString::fromLatin1(QCryptographicHash::hash(entry.data, QCryptographicHash::Sha256).toHex())},
        };
    case HostEntry::Status::Present:
        return u"<present>"_s;
    case HostEntry::Status::Directory:
        return u"<directory>"_s;
    case HostEntry::Status::Missing:
        return u"<missing>"_s;
    case HostEntry::Status::Error:
        return u"<unreadable>"_s;
    case HostEntry::Status::NotRead:
        break;
    }
    return u"<not read>"_s;
}

QJsonObject rawValues(const HostSnapshot &snapshot, const QList<HostRequest> &requests)
{
    QJsonObject raw;
    for (const HostRequest &request : requests) {
        const QList<HostEntry> entries = snapshot.matches(request);
        if (request.kind == HostRequest::Kind::List) {
            QJsonArray paths;
            for (const HostEntry &entry : entries) {
                if (entry.status != HostEntry::Status::Missing) {
                    paths.append(entry.path);
                }
            }
            raw.insert(request.pattern, paths);
        } else if (entries.isEmpty()) {
            raw.insert(request.pattern, request.isGlob() ? u"<no matches>"_s : rawValue(snapshot.file(request.pattern)).toString());
        } else {
            for (const HostEntry &entry : entries) {
                raw.insert(entry.path, rawValue(entry));
            }
        }
    }
    return raw;
}

QJsonObject Report::toJson() const
{
    QJsonArray checks;
    for (const CheckResult &r : results) {
        QJsonArray subChecks;
        for (const SubCheckResult &s : r.subChecks) {
            subChecks.append(QJsonObject{
                {u"title"_s, s.title},
                {u"status"_s, statusKey(s.status)},
                {u"detail"_s, s.detail},
            });
        }
        checks.append(QJsonObject{
            {u"id"_s, r.id},
            {u"title"_s, r.title},
            {u"status"_s, statusKey(r.status)},
            {u"detail"_s, r.detail},
            {u"subChecks"_s, subChecks},
            {u"raw"_s, r.raw},
        });
    }

    QJsonObject host{
        {u"transport"_s, transport},
        {u"elevationRequested"_s, elevationRequested},
        {u"privilegedRead"_s, privilegedStateKey(privilegedState)},
    };
    if (!sysroot.isEmpty()) {
        host.insert(u"sysroot"_s, sysroot);
    }
    if (!hostReadError.isEmpty()) {
        host.insert(u"readError"_s, hostReadError);
    }
    if (!privilegedError.isEmpty()) {
        host.insert(u"privilegedError"_s, privilegedError);
    }

    return QJsonObject{
        {u"app"_s, QJsonObject{{u"id"_s, QStringLiteral(EACHECK_APP_ID)}, {u"version"_s, QStringLiteral(EACHECK_VERSION_STRING)}}},
        {u"requirementsAsOf"_s, requirementsAsOf()},
        {u"generated"_s, generated.toUTC().toString(Qt::ISODate)},
        {u"host"_s, host},
        {u"verdict"_s, verdictKey(verdict())},
        {u"checks"_s, checks},
    };
}

QString Report::toText() const
{
    QString text;
    text += u"EACheck %1 — %2\n"_s.arg(QStringLiteral(EACHECK_VERSION_STRING), verdictLabel(verdict()));
    text += u"Generated %1 · requirements as of %2\n\n"_s.arg(generated.toUTC().toString(Qt::ISODate), requirementsAsOf());
    for (const CheckResult &r : results) {
        text += u"[%1] %2"_s.arg(statusLabel(r.status), r.title);
        if (!r.detail.isEmpty()) {
            text += u" — "_s + r.detail;
        }
        text += u'\n';
        for (const SubCheckResult &s : r.subChecks) {
            text += u"    [%1] %2"_s.arg(statusLabel(s.status), s.title);
            if (!s.detail.isEmpty()) {
                text += u" — "_s + s.detail;
            }
            text += u'\n';
        }
    }
    return text;
}
