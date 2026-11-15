// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckRow.h"

#include "IconWidget.h"
#include "Icons.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSet>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
int iconSize(const QWidget *w)
{
    return w->style()->pixelMetric(QStyle::PM_ToolBarIconSize, nullptr, w);
}

QLabel *secondaryLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
} // namespace

CheckRow::CheckRow(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_expander(new QToolButton(this))
    , m_icon(new IconWidget(this))
    , m_title(new QLabel(title, this))
    , m_detail(secondaryLabel(this))
    , m_subChecks(new QWidget(this))
    , m_subLayout(new QVBoxLayout(m_subChecks))
{
    // Not checkable: Breeze draws a checked tool button as pressed, which boxes the arrow.
    m_expander->setAutoRaise(true);
    m_expander->setArrowType(Qt::RightArrow);
    m_expander->setAccessibleName(u"Show details"_s);
    // Keep the arrow's column when hidden so titles line up across rows.
    QSizePolicy policy = m_expander->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    m_expander->setSizePolicy(policy);
    connect(m_expander, &QToolButton::clicked, this, [this] {
        setExpanded(!m_expanded);
    });

    m_detail->setObjectName(u"detail"_s);

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    m_subLayout->setContentsMargins(0, 8, 0, 0);
    m_subLayout->setSpacing(8);
    m_subChecks->hide();

    auto *layout = new QGridLayout(this);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(0);
    layout->setColumnStretch(2, 1);
    layout->addWidget(m_expander, 0, 0, Qt::AlignTop);
    layout->addWidget(m_icon, 0, 1, Qt::AlignTop);
    layout->addWidget(m_title, 0, 2);
    layout->addWidget(m_detail, 1, 2);
    layout->addWidget(m_subChecks, 2, 2);

    setPending();
}

void CheckRow::setPending()
{
    m_status.reset();
    m_detail->setText(u"Checking…"_s);
    m_detail->show();
    // Drop the old sub-checks rather than show stale results under "Checking…". The row stays
    // expanded (or not) for when the result arrives.
    clearSubChecks();
    m_expander->hide();
    m_subChecks->hide();
    updateIcons();
}

void CheckRow::clearSubChecks()
{
    while (QLayoutItem *item = m_subLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_subIcons.clear();
}

void CheckRow::setResult(const CheckResult &result)
{
    m_status = result.status;
    m_icon->setToolTip(statusLabel(result.status));
    m_icon->setAccessibleName(statusLabel(result.status));
    // A pass needs no words. Anything else says why, even if the check gave no detail.
    const QString detail = result.detail.isEmpty() && result.status != CheckStatus::Pass ? statusLabel(result.status) : result.detail;
    m_detail->setText(detail);
    m_detail->setVisible(!detail.isEmpty());

    clearSubChecks();
    // A detail is shown once per row. With no event log, for example, every sub-check that needs
    // it would say "No TPM event log" again.
    QSet<QString> shown{detail};
    for (const SubCheckResult &sub : result.subChecks) {
        auto *item = new QWidget(m_subChecks);
        auto *icon = new IconWidget(item);
        m_subIcons.append({icon, sub.status});
        icon->setToolTip(statusLabel(sub.status));
        icon->setAccessibleName(statusLabel(sub.status));
        auto *title = new QLabel(sub.title, item);
        title->setWordWrap(true);
        auto *text = new QVBoxLayout;
        text->setSpacing(0);
        text->addWidget(title);
        if (!sub.detail.isEmpty() && !shown.contains(sub.detail)) {
            shown.insert(sub.detail);
            auto *subDetail = secondaryLabel(item);
            subDetail->setText(sub.detail);
            text->addWidget(subDetail);
        }
        auto *itemLayout = new QHBoxLayout(item);
        itemLayout->setContentsMargins({});
        itemLayout->setSpacing(8);
        itemLayout->addWidget(icon, 0, Qt::AlignTop);
        itemLayout->addLayout(text, 1);
        m_subLayout->addWidget(item);
    }
    updateIcons();
    m_expander->setVisible(!result.subChecks.isEmpty());
    setExpanded(m_expanded && !result.subChecks.isEmpty());
}

void CheckRow::changeEvent(QEvent *event)
{
    if (affectsIcons(event)) {
        syncIconThemes();
        updateIcons();
    }
    QWidget::changeEvent(event);
}

void CheckRow::updateIcons()
{
    const int size = iconSize(this);
    const QIcon icon = m_status ? statusIcon(*m_status) : themeIcon({u"view-refresh"_s});
    m_icon->setFixedSize(size, size);
    m_icon->setIcon(icon);
    const int subSize = size * 2 / 3;
    for (const auto &[subIcon, status] : std::as_const(m_subIcons)) {
        subIcon->setFixedSize(subSize, subSize);
        subIcon->setIcon(statusIcon(status));
    }
}

void CheckRow::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_expander->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_expander->setAccessibleName(expanded ? u"Hide details"_s : u"Show details"_s);
    m_subChecks->setVisible(expanded && m_subLayout->count() > 0);
}
