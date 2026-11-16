// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/CheckResult.h"

#include <QWidget>

#include <optional>
#include <utility>

class CheckRowHeader;
class IconWidget;
class QLabel;
class QToolButton;
class QVBoxLayout;
class Revealer;

// One requirement: status icon, title and, when it adds something, a detail line. Rows with
// sub-checks expand to show them, by clicking the row or its arrow.
class CheckRow : public QWidget
{
    Q_OBJECT

public:
    explicit CheckRow(const QString &title, QWidget *parent = nullptr);

    void setPending();
    void setResult(const CheckResult &result);

Q_SIGNALS:
    // The row's sub-checks were shown or hidden, so the list around it needs laying out again.
    void heightChanged();

protected:
    void changeEvent(QEvent *event) override;

private:
    enum class IconChange { AtOnce, Fade };
    void updateIcons(IconChange change = IconChange::AtOnce);
    void updateExpander();
    void setExpandable(bool expandable);
    void setExpanded(bool expanded, bool animate);
    void clearSubChecks();

    CheckRowHeader *m_header;
    QToolButton *m_expander;
    IconWidget *m_icon;
    QLabel *m_title;
    QLabel *m_detail;
    QWidget *m_subChecks;
    QVBoxLayout *m_subLayout;
    Revealer *m_reveal;
    bool m_expanded = false;
    std::optional<CheckStatus> m_status; // empty while checking
    QList<std::pair<IconWidget *, CheckStatus>> m_subIcons;
};
