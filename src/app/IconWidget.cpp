// SPDX-License-Identifier: GPL-3.0-or-later
#include "IconWidget.h"

#include "Motion.h"

#include <QPainter>
#include <QVariantAnimation>

IconWidget::IconWidget(QWidget *parent)
    : QWidget(parent)
    , m_fade(new QVariantAnimation(this))
{
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);
    connect(m_fade, &QVariantAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(m_fade, &QVariantAnimation::finished, this, [this] {
        m_previous = QIcon();
        update();
    });
}

void IconWidget::setIcon(const QIcon &icon)
{
    m_fade->stop();
    m_previous = QIcon();
    m_icon = icon;
    update();
}

void IconWidget::fadeTo(const QIcon &icon)
{
    const int ms = Motion::duration(Motion::Long);
    if (ms == 0 || !isVisible()) {
        setIcon(icon);
        return;
    }
    m_previous = m_icon;
    m_icon = icon;
    m_fade->stop();
    m_fade->setDuration(ms);
    m_fade->start();
    update();
}

void IconWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (m_fade->state() == QAbstractAnimation::Running) {
        const qreal progress = m_fade->currentValue().toReal();
        painter.setOpacity(1 - progress);
        painter.drawPixmap(0, 0, m_previous.pixmap(size(), devicePixelRatio()));
        painter.setOpacity(progress);
    }
    painter.drawPixmap(0, 0, m_icon.pixmap(size(), devicePixelRatio()));
}
