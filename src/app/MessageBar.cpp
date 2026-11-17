// SPDX-License-Identifier: GPL-3.0-or-later
#include "MessageBar.h"

#include "IconWidget.h"
#include "Icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStyle>

using namespace Qt::StringLiterals;

MessageBar::MessageBar(QWidget *parent)
    : QWidget(parent)
    , m_icon(new IconWidget(this))
    , m_text(new QLabel(this))
    , m_button(new QPushButton(this))
{
    m_text->setWordWrap(true);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 6, 6);
    layout->setSpacing(8);
    layout->addWidget(m_icon, 0, Qt::AlignVCenter);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_button, 0, Qt::AlignVCenter);
    updateIcon();
}

void MessageBar::setMessage(Type type, const QString &text, const QString &action)
{
    m_type = type;
    m_text->setText(text);
    m_button->setText(action);
    m_button->setVisible(!action.isEmpty());
    updateIcon();
    update();
}

QColor MessageBar::colour() const
{
    // KMessageWidget's colours: the highlight for information, Breeze's neutral and negative
    // colours for warnings and errors.
    switch (m_type) {
    case Type::Information:
        return palette().color(QPalette::Highlight);
    case Type::Warning:
        return {0xf6, 0x74, 0x00};
    case Type::Error:
        return {0xda, 0x44, 0x53};
    }
    return {};
}

void MessageBar::updateIcon()
{
    const int size = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    m_icon->setFixedSize(size, size);
    switch (m_type) {
    case Type::Information:
        m_icon->setIcon(themeIcon({u"dialog-information"_s}));
        break;
    case Type::Warning:
        m_icon->setIcon(themeIcon({u"dialog-warning"_s}));
        break;
    case Type::Error:
        m_icon->setIcon(themeIcon({u"dialog-error"_s}));
        break;
    }
}

void MessageBar::changeEvent(QEvent *event)
{
    if (affectsIcons(event)) {
        updateIcon();
    }
    QWidget::changeEvent(event);
}

void MessageBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor border = colour();
    QColor fill = border;
    fill.setAlphaF(0.2);
    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
}
