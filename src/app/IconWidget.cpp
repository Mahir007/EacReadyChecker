// SPDX-License-Identifier: GPL-3.0-or-later
#include "IconWidget.h"

#include <QPainter>

IconWidget::IconWidget(QWidget *parent)
    : QWidget(parent)
{
}

void IconWidget::setIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

void IconWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_icon.pixmap(size(), devicePixelRatio()));
}
