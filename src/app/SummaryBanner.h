// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Report.h"

#include <QWidget>

#include <optional>

class QLabel;

// Header: overall verdict with its icon and a one-line summary.
class SummaryBanner : public QWidget
{
    Q_OBJECT

public:
    explicit SummaryBanner(QWidget *parent = nullptr);

    void setRunning(bool elevated);
    void setReport(const Report &report);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateIcon();

    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_subtitle;
    std::optional<Verdict> m_verdict; // empty while running
};
