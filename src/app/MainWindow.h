// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckRunner.h"

#include <QWidget>

class CheckRow;
class QPushButton;
class SummaryBanner;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(const HostAccess &host, QWidget *parent = nullptr);

    // Starts a run. With `elevate`, also asks for admin access (one polkit prompt).
    void runChecks(bool elevate);

private:
    void onStarted();
    void onFinished(const Report &report);
    void copyReport(bool json);
    void showAbout();

    CheckRunner *m_runner;
    SummaryBanner *m_banner;
    QList<CheckRow *> m_rows;
    QPushButton *m_runFull;
    QPushButton *m_refresh;
    QPushButton *m_copy;
    Report m_report;
    bool m_elevating = false;
};
