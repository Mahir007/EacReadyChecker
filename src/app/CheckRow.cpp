// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckRow.h"

#include "Icons.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
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
    , m_icon(new QLabel(this))
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

    m_subLayout->setContentsMargins(0, 4, 0, 0);
    m_subLayout->setSpacing(4);
    m_subChecks->hide();

    auto *layout = new QGridLayout(this);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(2);
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
    m_detail->setText(result.detail.isEmpty() ? statusLabel(result.status) : result.detail);

    clearSubChecks();
    for (const SubCheckResult &sub : result.subChecks) {
        auto *row = new QWidget(m_subChecks);
        auto *icon = new QLabel(row);
        m_subIcons.append({icon, sub.status});
        icon->setToolTip(statusLabel(sub.status));
        auto *text = secondaryLabel(row);
        text->setText(sub.detail.isEmpty() ? sub.title : u"%1 — %2"_s.arg(sub.title, sub.detail));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(icon, 0, Qt::AlignTop);
        rowLayout->addWidget(text, 1);
        m_subLayout->addWidget(row);
    }
    updateIcons();
    m_expander->setVisible(!result.subChecks.isEmpty());
    setExpanded(m_expanded && !result.subChecks.isEmpty());
}

void CheckRow::changeEvent(QEvent *event)
{
    if (affectsIcons(event)) {
        syncFallbackIconTheme();
        updateIcons();
    }
    QWidget::changeEvent(event);
}

void CheckRow::updateIcons()
{
    const int size = iconSize(this);
    const QIcon icon = m_status ? statusIcon(*m_status) : themeIcon({u"view-refresh"_s});
    m_icon->setFixedSize(size, size);
    m_icon->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatio()));
    const int subSize = size * 2 / 3;
    for (const auto &[label, status] : std::as_const(m_subIcons)) {
        label->setPixmap(statusIcon(status).pixmap(QSize(subSize, subSize), devicePixelRatio()));
    }
}

void CheckRow::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_expander->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_expander->setAccessibleName(expanded ? u"Hide details"_s : u"Show details"_s);
    m_subChecks->setVisible(expanded && m_subLayout->count() > 0);
}
