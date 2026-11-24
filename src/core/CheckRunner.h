// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Check.h"
#include "HostAccess.h"
#include "Report.h"

#include <QFutureWatcher>
#include <QObject>

#include <atomic>
#include <memory>
#include <vector>

using CheckList = std::vector<std::unique_ptr<Check>>;

// Runs the checks: one batched unprivileged read, then (if asked) one batched pkexec read, then
// evaluates every check against the combined snapshot.
class CheckRunner : public QObject
{
    Q_OBJECT

public:
    CheckRunner(HostAccess host, CheckList checks, QObject *parent = nullptr);
    ~CheckRunner() override;

    // Blocking. Used by the CLI and tests, and by start() on a worker thread.
    static Report run(const HostAccess &host, const CheckList &checks, bool elevate, const std::atomic_bool *cancelled = nullptr);

    // Runs off the UI thread and emits finished(). Ignored while a run is in progress.
    void start(bool elevate);
    // Aborts a pending host read, such as an unanswered password dialog. The run still finishes.
    void cancel() { m_cancelled = true; }
    bool isRunning() const { return m_watcher.isRunning(); }

    const CheckList &checks() const { return m_checks; }
    bool hasPrivilegedChecks() const;

Q_SIGNALS:
    void started();
    void finished(const Report &report);

private:
    HostAccess m_host;
    CheckList m_checks;
    QFutureWatcher<Report> m_watcher;
    std::atomic_bool m_cancelled = false;
};
