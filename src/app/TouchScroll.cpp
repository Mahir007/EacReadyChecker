// SPDX-License-Identifier: GPL-3.0-or-later
#include "TouchScroll.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QScroller>

namespace
{
// QScroller's touch gesture only reads touch events. The widgets get mouse events that Qt makes
// from the same touches, so without this a drag that starts on a button clicks it when the finger
// lifts. QScroller handles this for the mouse gesture, the same way: a release far outside the
// pressed widget, then nothing more until the next press.
class TouchScroll : public QObject
{
public:
    explicit TouchScroll(QAbstractScrollArea *area)
        : QObject(area)
        , m_viewport(area->viewport())
    {
        QScroller::grabGesture(m_viewport, QScroller::TouchGesture);
        m_scroller = QScroller::scroller(m_viewport);
        // No bounce at the ends: GNOME and Plasma don't.
        QScrollerProperties properties = m_scroller->scrollerProperties();
        const QVariant off = QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff);
        properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, off);
        properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, off);
        m_scroller->setScrollerProperties(properties);
        connect(m_scroller, &QScroller::stateChanged, this, [this](QScroller::State state) {
            if (state == QScroller::Dragging) {
                cancelPress();
            }
        });
        qApp->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::MouseButtonRelease:
            break;
        default:
            return false;
        }
        const auto *mouse = static_cast<QMouseEvent *>(event);
        auto *widget = qobject_cast<QWidget *>(watched);
        if (mouse->pointingDevice()->type() != QInputDevice::DeviceType::TouchScreen || !widget
            || (widget != m_viewport && !m_viewport->isAncestorOf(widget))) {
            return false;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            // A tap on contents that are still moving only stops them. Otherwise remember the
            // widget, the last one the press goes to as it passes up from the one under the finger.
            m_scrolling = m_scroller->state() == QScroller::Scrolling || m_scroller->state() == QScroller::Dragging;
            m_pressed = m_scrolling ? nullptr : widget;
        }
        const bool scrolling = m_scrolling;
        if (event->type() == QEvent::MouseButtonRelease) {
            m_scrolling = false;
            m_pressed = nullptr;
        }
        return scrolling;
    }

private:
    void cancelPress()
    {
        m_scrolling = true;
        if (m_pressed) {
            const QPointF away(-QWIDGETSIZE_MAX, -QWIDGETSIZE_MAX);
            QMouseEvent release(QEvent::MouseButtonRelease, away, away, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(m_pressed, &release);
            m_pressed = nullptr;
        }
    }

    QWidget *m_viewport;
    QScroller *m_scroller;
    QPointer<QWidget> m_pressed;
    bool m_scrolling = false;
};
} // namespace

void enableTouchScrolling(QAbstractScrollArea *area)
{
    new TouchScroll(area);
}
