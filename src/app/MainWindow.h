// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckRunner.h"

#include <QWidget>

class CheckRow;
class QLabel;
class QPushButton;
class QTimer;
class SummaryBanner;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(const HostAccess &host, QWidget *parent = nullptr);

    // Starts a run. With `elevate`, also asks for admin access (one polkit prompt).
    void runChecks(bool elevate);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateIcons();
    void updateNote();
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
    QPushButton *m_about;
    QLabel *m_note;
    QTimer *m_copiedReset;
    Report m_report;
    bool m_elevating = false;
};
