// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Check.h"

// Stand-in until the real check lands (Phases 2–4). It already requests the host paths the real
// check will read, so the read pipeline and the raw values in --json can be checked end to end.
class PlaceholderCheck : public Check
{
public:
    PlaceholderCheck(QString id, QString title, QList<HostRequest> requests, QList<HostRequest> privilegedRequests = {});

    QString id() const override { return m_id; }
    QString title() const override { return m_title; }
    QList<HostRequest> requests() const override { return m_requests; }
    QList<HostRequest> privilegedRequests() const override { return m_privilegedRequests; }
    CheckResult evaluate(const HostSnapshot &snapshot) const override;

private:
    QString m_id;
    QString m_title;
    QList<HostRequest> m_requests;
    QList<HostRequest> m_privilegedRequests;
};
