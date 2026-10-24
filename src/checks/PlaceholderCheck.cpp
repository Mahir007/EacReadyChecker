// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaceholderCheck.h"

#include "core/Report.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace
{
void addRaw(QJsonObject &raw, const HostSnapshot &snapshot, const HostRequest &request)
{
    const QList<HostEntry> entries = snapshot.matches(request);
    if (request.kind == HostRequest::Kind::List) {
        QJsonArray paths;
        for (const HostEntry &entry : entries) {
            if (entry.status != HostEntry::Status::Missing) {
                paths.append(entry.path);
            }
        }
        raw.insert(request.pattern, paths);
        return;
    }
    if (entries.isEmpty()) {
        raw.insert(request.pattern, request.isGlob() ? u"<no matches>"_s : rawValue(snapshot.file(request.pattern)).toString());
        return;
    }
    for (const HostEntry &entry : entries) {
        raw.insert(entry.path, rawValue(entry));
    }
}
} // namespace

PlaceholderCheck::PlaceholderCheck(QString id, QString title, QList<HostRequest> requests, QList<HostRequest> privilegedRequests)
    : m_id(std::move(id))
    , m_title(std::move(title))
    , m_requests(std::move(requests))
    , m_privilegedRequests(std::move(privilegedRequests))
{
}

CheckResult PlaceholderCheck::evaluate(const HostSnapshot &snapshot) const
{
    CheckResult result;
    for (const HostRequest &request : m_requests) {
        addRaw(result.raw, snapshot, request);
    }
    for (const HostRequest &request : m_privilegedRequests) {
        addRaw(result.raw, snapshot, request);
    }

    if (!m_privilegedRequests.isEmpty() && snapshot.privilegedState != PrivilegedState::Granted) {
        result.status = CheckStatus::NeedsAdmin;
        result.detail = u"Not implemented yet · needs admin access"_s;
    } else {
        result.status = CheckStatus::Unknown;
        result.detail = u"Not implemented yet"_s;
    }
    return result;
}
