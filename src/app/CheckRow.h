// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"

#include <QWidget>

class QLabel;
class QToolButton;
class QVBoxLayout;

// One requirement: status icon, title, one-line detail. Rows with sub-checks can be expanded.
class CheckRow : public QWidget
{
    Q_OBJECT

public:
    explicit CheckRow(const QString &title, QWidget *parent = nullptr);

    void setPending();
    void setResult(const CheckResult &result);

private:
    void setExpanded(bool expanded);

    QToolButton *m_expander;
    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_detail;
    QWidget *m_subChecks;
    QVBoxLayout *m_subLayout;
};
