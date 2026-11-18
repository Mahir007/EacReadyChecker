// SPDX-License-Identifier: GPL-3.0-or-later
#include "MessageBar.h"

#include "IconWidget.h"
#include "Icons.h"

#include <QHBoxLayout>
#include <QIconEngine>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStyle>

using namespace Qt::StringLiterals;

namespace
{
// An information icon in the shape of Breeze's: a rounded square in `colour` with a white "i", on a
// 16-unit grid scaled to the size asked for.
class InfoIconEngine : public QIconEngine
{
public:
    explicit InfoIconEngine(const QColor &colour)
        : m_colour(colour)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode, QIcon::State) override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.topLeft());
        painter->scale(rect.width() / 16.0, rect.height() / 16.0);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_colour);
        painter->drawRoundedRect(QRectF(2, 2, 12, 12), 2, 2);
        painter->setBrush(Qt::white);
        painter->drawRect(QRectF(7, 4, 2, 2));
        painter->drawRect(QRectF(7, 7, 2, 5));
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return pixmap;
    }

    QIconEngine *clone() const override { return new InfoIconEngine(m_colour); }

private:
    QColor m_colour;
};

// Breeze draws its information icon in the colour scheme's highlight, and on Plasma KDE's platform
// theme recolours it to match, with Breeze as the style. Without it, as in the AppImage, or with
// Breeze replacing GNOME's icons, it stays Breeze's blue whatever the accent.
bool needsOwnInfoIcon()
{
    return QIcon::themeName().startsWith(u"breeze"_s) && !isBreezeStyle();
}
} // namespace

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
        // The accent, as Plasma shows it, also while the window isn't focused.
        m_icon->setIcon(needsOwnInfoIcon() ? QIcon(new InfoIconEngine(palette().color(QPalette::Active, QPalette::Highlight)))
                                           : themeIcon({u"dialog-information"_s}));
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
