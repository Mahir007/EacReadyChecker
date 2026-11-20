// SPDX-License-Identifier: GPL-3.0-or-later
#include "CheckRow.h"

#include "Desktop.h"
#include "IconWidget.h"
#include "Icons.h"
#include "Motion.h"
#include "Revealer.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <cmath>
#include <functional>

using namespace Qt::StringLiterals;

// The part of a row above its sub-checks. While the row can expand, a click anywhere on it
// toggles them, and it shows a hover like an item view's. The hover fades in and out.
class CheckRowHeader : public QWidget
{
public:
    explicit CheckRowHeader(QWidget *parent)
        : QWidget(parent)
        , m_hover(new QVariantAnimation(this))
    {
        connect(m_hover, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_level = value.toReal();
            update();
        });
    }

    std::function<void()> toggle; // empty while the row can't expand

protected:
    void enterEvent(QEnterEvent *) override { fadeHover(1); }
    void leaveEvent(QEvent *) override { fadeHover(0); }

    void mousePressEvent(QMouseEvent *event) override
    {
        // Labels pass clicks on to here. Accepting the press keeps the release here too.
        event->setAccepted(toggle && event->button() == Qt::LeftButton);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (toggle && event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            toggle();
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        if (!toggle || m_level == 0) {
            return;
        }
        QPainter painter(this);
        painter.setOpacity(m_level);
        if (isFusionOnPlasma(this)) {
            // Fusion draws no hover in item views. On Plasma, draw Breeze's: 30% of the highlight
            // with a 1 px line of it at 80%, inside a margin of 2 px at the sides and 1 px above
            // and below.
            QColor fill = palette().color(QPalette::Highlight);
            QColor line = fill;
            fill.setAlphaF(0.3);
            line.setAlphaF(0.8);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(line);
            painter.setBrush(fill);
            painter.drawRoundedRect(QRectF(rect()).adjusted(2.5, 1.5, -2.5, -1.5), 4.5, 4.5);
            return;
        }
        if (style()->name().compare(u"fusion"_s, Qt::CaseInsensitive) == 0) {
            // Elsewhere, shade the row like GNOME's lists do.
            QColor colour = palette().color(foregroundRole());
            colour.setAlphaF(0.06);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colour);
            painter.drawRoundedRect(rect(), 6, 6);
            return;
        }
        QStyleOptionViewItem option;
        option.initFrom(this);
        option.state |= QStyle::State_MouseOver;
        // An item on its own. Without a position, Breeze draws it square, as for a table cell.
        option.viewItemPosition = QStyleOptionViewItem::OnlyOne;
        style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, &painter, this);
    }

private:
    // From where it is, so a quick pass over the row doesn't flash.
    void fadeHover(qreal target)
    {
        m_hover->stop();
        const int ms = Motion::duration(int(std::lround(Motion::Short * std::abs(target - m_level))));
        if (ms == 0) {
            m_level = target;
            update();
            return;
        }
        m_hover->setDuration(ms);
        m_hover->setStartValue(m_level);
        m_hover->setEndValue(target);
        m_hover->start();
    }

    QVariantAnimation *m_hover;
    qreal m_level = 0; // 0 no hover, 1 full
};

namespace
{
constexpr int kSpacing = 8;

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

// `icon` turned clockwise, for the arrow while it turns.
QIcon turned(const QIcon &icon, int size, qreal scale, qreal degrees)
{
    QPixmap pixmap(QSize(size, size) * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QPointF centre(size / 2.0, size / 2.0);
    painter.translate(centre);
    painter.rotate(degrees);
    painter.translate(-centre);
    painter.drawPixmap(0, 0, icon.pixmap(QSize(size, size), scale));
    return QIcon(pixmap);
}
} // namespace

CheckRow::CheckRow(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_header(new CheckRowHeader(this))
    , m_expander(new QToolButton(m_header))
    , m_icon(new IconWidget(m_header))
    , m_title(new QLabel(title, m_header))
    , m_detail(secondaryLabel(m_header))
    , m_subChecks(new QWidget(this))
    , m_subLayout(new QVBoxLayout(m_subChecks))
    , m_reveal(new Revealer(m_subChecks, this))
{
    // Not checkable: Breeze draws a checked tool button as pressed, which boxes the arrow.
    m_expander->setAutoRaise(true);
    m_expander->setAccessibleName(u"Show details"_s);
    // Keep the arrow's column when hidden so titles line up across rows.
    QSizePolicy policy = m_expander->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    m_expander->setSizePolicy(policy);
    connect(m_expander, &QToolButton::clicked, this, [this] {
        setExpanded(!m_expanded, true);
    });

    m_detail->setObjectName(u"detail"_s);

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    m_subLayout->setSpacing(kSpacing);
    // Qt would lay out the row, then the list, painting in between: for a frame, a collapsed row
    // keeps its height with the title and detail stretched into it. So lay out both at once, on
    // each step while the sub-checks slide.
    connect(m_reveal, &Revealer::progressChanged, this, [this] {
        updateExpander();
        layout()->activate();
        updateGeometry();
        Q_EMIT heightChanged();
    });

    auto *header = new QGridLayout(m_header);
    header->setHorizontalSpacing(kSpacing);
    header->setVerticalSpacing(0);
    header->setColumnStretch(2, 1);
    header->addWidget(m_expander, 0, 0, Qt::AlignTop);
    header->addWidget(m_icon, 0, 1, Qt::AlignTop);
    header->addWidget(m_title, 0, 2);
    header->addWidget(m_detail, 1, 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_reveal);

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
    setExpandable(false);
    m_reveal->setRevealed(false, false);
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
        itemLayout->setSpacing(kSpacing);
        itemLayout->addWidget(icon, 0, Qt::AlignTop);
        itemLayout->addLayout(text, 1);
        m_subLayout->addWidget(item);
    }
    updateIcons(IconChange::Fade);
    setExpandable(!result.subChecks.isEmpty());
    // Only a click slides the sub-checks. Here they're back as they were before the run.
    setExpanded(m_expanded && !result.subChecks.isEmpty(), false);
}

void CheckRow::changeEvent(QEvent *event)
{
    if (affectsIcons(event)) {
        syncIconThemes();
        updateIcons();
    }
    QWidget::changeEvent(event);
}

void CheckRow::updateIcons(IconChange change)
{
    const int size = iconSize(this);
    const QIcon icon = m_status ? statusIcon(*m_status) : themeIcon({u"view-refresh"_s});
    updateExpander();
    m_icon->setFixedSize(size, size);
    if (change == IconChange::Fade) {
        m_icon->fadeTo(icon);
    } else {
        m_icon->setIcon(icon);
    }
    const int subSize = size * 2 / 3;
    for (const auto &[subIcon, status] : std::as_const(m_subIcons)) {
        subIcon->setFixedSize(subSize, subSize);
        subIcon->setIcon(statusIcon(status));
    }

    // Under Fusion on Plasma, Breeze's margin and arrow size, which set a row's height and where
    // its title starts. Breeze's tool buttons have 6 px around their icon.
    const int margin = layoutMargin(m_header);
    m_header->layout()->setContentsMargins(margin, margin, margin, margin);
    if (isFusionOnPlasma(this)) {
        const int arrow = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this) + 12;
        m_expander->setFixedSize(arrow, arrow);
    } else {
        m_expander->setMinimumSize(0, 0);
        m_expander->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }
    // Sub-checks line up with the row's title. Both sizes depend on the style.
    const QMargins margins = m_header->layout()->contentsMargins();
    const int expander = m_expander->sizeHint().expandedTo(m_expander->minimumSize()).width();
    const int indent = margins.left() + expander + kSpacing + size + kSpacing;
    m_subLayout->setContentsMargins(indent, 0, margins.right(), margins.bottom());
}

void CheckRow::updateExpander()
{
    // Breeze's chevrons, which Breeze also draws on KDE, so the arrows look the same everywhere.
    // Fusion's own are small triangles. Adwaita's and Yaru's symbolic icons would be dark on dark,
    // so with those (Breeze isn't installed) it's the style's arrow.
    // While the sub-checks slide, the arrow turns with them.
    const qreal progress = m_reveal->progress();
    const QString name = progress == 1 ? u"arrow-down"_s : u"arrow-right"_s;
    if (!isGnomeIconTheme(QIcon::themeName()) && QIcon::hasThemeIcon(name)) {
        const int size = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
        m_expander->setArrowType(Qt::NoArrow);
        m_expander->setIconSize(QSize(size, size));
        m_expander->setIcon(progress == 0 || progress == 1 ? QIcon::fromTheme(name) : turned(QIcon::fromTheme(name), size, devicePixelRatio(), progress * 90));
    } else {
        m_expander->setIcon(QIcon());
        m_expander->setArrowType(progress >= 0.5 ? Qt::DownArrow : Qt::RightArrow);
    }
}

void CheckRow::setExpandable(bool expandable)
{
    m_expander->setVisible(expandable);
    // Clicking the row expands it, so its detail can't also be a place to select text.
    m_detail->setTextInteractionFlags(expandable ? Qt::NoTextInteraction : Qt::TextSelectableByMouse);
    m_header->toggle = nullptr;
    if (expandable) {
        m_header->toggle = [this] {
            setExpanded(!m_expanded, true);
        };
    }
    m_header->update();
}

void CheckRow::setExpanded(bool expanded, bool animate)
{
    m_expanded = expanded;
    m_expander->setAccessibleName(expanded ? u"Hide details"_s : u"Show details"_s);
    m_reveal->setRevealed(expanded && m_subLayout->count() > 0, animate);
    updateExpander();
}
