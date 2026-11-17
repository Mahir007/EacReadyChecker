// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckRunner.h"

#include <QWidget>

class CheckRow;
class MessageBar;
class Revealer;
class QPushButton;
class QScrollArea;
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
    void updateColours();
    void layOutList();
    void updateIcons();
    void updateMargins();
    void updateScrollBar();
    void onStarted();
    void onFinished(const Report &report);
    void updateMessage(const Report &report);
    void copyReport(bool json);
    void showAbout();

    CheckRunner *m_runner;
    SummaryBanner *m_banner;
    QList<CheckRow *> m_rows;
    MessageBar *m_message;
    QWidget *m_messageBox;
    Revealer *m_messageReveal;
    QPushButton *m_runFull; // the message's button
    QPushButton *m_copy;
    QPushButton *m_about;
    QWidget *m_list;
    QWidget *m_footer;
    QScrollArea *m_scroll;
    QTimer *m_copiedReset;
    Report m_report;
    bool m_elevating = false;
};
