// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaceholderCheck.h"

using namespace Qt::StringLiterals;

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
    if (!m_privilegedRequests.isEmpty() && snapshot.privilegedState != PrivilegedState::Granted) {
        result.status = CheckStatus::NeedsAdmin;
        result.detail = u"Not implemented yet · needs admin access"_s;
    } else {
        result.status = CheckStatus::Unknown;
        result.detail = u"Not implemented yet"_s;
    }
    return result;
}
