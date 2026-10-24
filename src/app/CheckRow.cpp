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
    m_expander->setAutoRaise(true);
    m_expander->setCheckable(true);
    m_expander->setArrowType(Qt::RightArrow);
    m_expander->setAccessibleName(u"Show details"_s);
    // Keep the arrow's column when hidden so titles line up across rows.
    QSizePolicy policy = m_expander->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    m_expander->setSizePolicy(policy);
    connect(m_expander, &QToolButton::toggled, this, &CheckRow::setExpanded);

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    const int size = iconSize(this);
    m_icon->setFixedSize(size, size);

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
    m_icon->setPixmap(themeIcon({u"view-refresh"_s}).pixmap(iconSize(this)));
    m_detail->setText(u"Checking…"_s);
}

void CheckRow::setResult(const CheckResult &result)
{
    m_icon->setPixmap(statusIcon(result.status).pixmap(iconSize(this)));
    m_icon->setToolTip(statusLabel(result.status));
    m_icon->setAccessibleName(statusLabel(result.status));
    m_detail->setText(result.detail.isEmpty() ? statusLabel(result.status) : result.detail);

    while (QLayoutItem *item = m_subLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const int size = iconSize(this) * 2 / 3;
    for (const SubCheckResult &sub : result.subChecks) {
        auto *row = new QWidget(m_subChecks);
        auto *icon = new QLabel(row);
        icon->setPixmap(statusIcon(sub.status).pixmap(size));
        icon->setToolTip(statusLabel(sub.status));
        auto *text = secondaryLabel(row);
        text->setText(sub.detail.isEmpty() ? sub.title : u"%1 — %2"_s.arg(sub.title, sub.detail));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(icon, 0, Qt::AlignTop);
        rowLayout->addWidget(text, 1);
        m_subLayout->addWidget(row);
    }
    m_expander->setVisible(!result.subChecks.isEmpty());
    if (result.subChecks.isEmpty()) {
        m_expander->setChecked(false);
    }
    setExpanded(m_expander->isChecked());
}

void CheckRow::setExpanded(bool expanded)
{
    m_expander->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_expander->setAccessibleName(expanded ? u"Hide details"_s : u"Show details"_s);
    m_subChecks->setVisible(expanded && m_subLayout->count() > 0);
}
