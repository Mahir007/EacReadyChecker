// SPDX-License-Identifier: GPL-3.0-or-later
#include "Revealer.h"

#include "Motion.h"

#include <QEasingCurve>
#include <QVariantAnimation>

#include <cmath>

Revealer::Revealer(QWidget *content, QWidget *parent)
    : QWidget(parent)
    , m_content(content)
    , m_animation(new QVariantAnimation(this))
{
    m_content->setParent(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setProgress(value.toReal());
    });
    hide();
}

void Revealer::setRevealed(bool revealed, bool animate)
{
    const qreal target = revealed ? 1 : 0;
    // From where it is, so a click while it moves turns it around.
    const int ms = animate ? Motion::duration(int(std::lround(Motion::Long * std::abs(target - m_progress)))) : 0;
    m_animation->stop();
    if (ms == 0 || !window()->isVisible()) {
        setProgress(target);
        return;
    }
    m_animation->setDuration(ms);
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    m_animation->start();
}

void Revealer::setProgress(qreal progress)
{
    if (progress == m_progress && isVisibleTo(parentWidget()) == (progress > 0)) {
        return;
    }
    m_progress = progress;
    setVisible(progress > 0);
    updateGeometry();
    placeContent();
    Q_EMIT progressChanged();
}

int Revealer::contentHeight(int width) const
{
    return m_content->hasHeightForWidth() ? m_content->heightForWidth(width) : m_content->sizeHint().height();
}

QSize Revealer::sizeHint() const
{
    const QSize content = m_content->sizeHint();
    return {content.width(), heightForWidth(content.width())};
}

QSize Revealer::minimumSizeHint() const
{
    const QSize content = m_content->minimumSizeHint();
    return {content.width(), int(std::lround(content.height() * m_progress))};
}

int Revealer::heightForWidth(int width) const
{
    return int(std::lround(contentHeight(width) * m_progress));
}

void Revealer::resizeEvent(QResizeEvent *event)
{
    placeContent();
    QWidget::resizeEvent(event);
}

void Revealer::placeContent()
{
    // The content's bottom edge shows first, then the rest slides down into view.
    const int full = contentHeight(width());
    m_content->setGeometry(0, height() - full, width(), full);
}
