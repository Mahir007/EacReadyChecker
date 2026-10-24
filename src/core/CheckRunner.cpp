// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckRunner.h"

#include <QSet>
#include <QtConcurrentRun>

#include <algorithm>

namespace
{
QList<HostRequest> uniqueRequests(const CheckList &checks, QList<HostRequest> (Check::*which)() const)
{
    QList<HostRequest> requests;
    QSet<QString> seen;
    for (const auto &check : checks) {
        const QList<HostRequest> wanted = ((*check).*which)();
        for (const HostRequest &request : wanted) {
            if (!seen.contains(request.key())) {
                seen.insert(request.key());
                requests.append(request);
            }
        }
    }
    return requests;
}
} // namespace

CheckRunner::CheckRunner(HostAccess host, CheckList checks, QObject *parent)
    : QObject(parent)
    , m_host(std::move(host))
    , m_checks(std::move(checks))
{
    connect(&m_watcher, &QFutureWatcher<Report>::finished, this, [this] {
        Q_EMIT finished(m_watcher.result());
    });
}

CheckRunner::~CheckRunner()
{
    // Don't leave a worker blocked on a password dialog nobody will answer.
    cancel();
    m_watcher.waitForFinished();
}

Report CheckRunner::run(const HostAccess &host, const CheckList &checks, bool elevate, const std::atomic_bool *cancelled)
{
    Report report;
    report.transport = host.transportName();
    report.sysroot = host.sysroot();
    report.elevationRequested = elevate;

    HostSnapshot snapshot;
    const HostAccess::Result userRead = host.read(uniqueRequests(checks, &Check::requests), HostAccess::Privilege::User, snapshot, cancelled);
    report.hostReadError = userRead.error;

    if (elevate) {
        const QList<HostRequest> privileged = uniqueRequests(checks, &Check::privilegedRequests);
        if (!privileged.isEmpty()) {
            const HostAccess::Result adminRead = host.read(privileged, HostAccess::Privilege::Admin, snapshot, cancelled);
            snapshot.privilegedState = adminRead.privilegedState;
            snapshot.privilegedError = adminRead.error;
        }
    }
    report.privilegedState = snapshot.privilegedState;
    report.privilegedError = snapshot.privilegedError;

    for (const auto &check : checks) {
        CheckResult result = check->evaluate(snapshot);
        // Record what the check saw. Values the check set itself take precedence.
        const QJsonObject raw = rawValues(snapshot, check->requests() + check->privilegedRequests());
        for (auto it = raw.begin(); it != raw.end(); ++it) {
            if (!result.raw.contains(it.key())) {
                result.raw.insert(it.key(), it.value());
            }
        }
        result.id = check->id();
        result.title = check->title();
        report.results.append(std::move(result));
    }
    report.generated = QDateTime::currentDateTimeUtc();
    return report;
}

void CheckRunner::start(bool elevate)
{
    if (isRunning()) {
        return;
    }
    m_cancelled = false;
    Q_EMIT started();
    m_watcher.setFuture(QtConcurrent::run([this, elevate] {
        return run(m_host, m_checks, elevate, &m_cancelled);
    }));
}

bool CheckRunner::hasPrivilegedChecks() const
{
    return std::any_of(m_checks.begin(), m_checks.end(), [](const auto &check) {
        return !check->privilegedRequests().isEmpty();
    });
}
