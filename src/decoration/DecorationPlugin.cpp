// SPDX-License-Identifier: GPL-3.0-or-later
// The GNOME title bar plugin: QAdwaitaDecorations, unchanged, with a shadow for
// unfocused windows too. Upstream draws its shadow only for the focused window. This takes the place
// of upstream's plugin class, which only creates the decoration.
#include "qadwaitadecorations.h"

#include <QtGui/QPainter>
#include <QtWaylandClient/private/qwaylanddecorationplugin_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

Q_DECL_IMPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

namespace
{
// Upstream's ceCornerRadius, the title bar's corners.
constexpr qreal CornerRadius = 12;
// Upstream's blur. At this opacity the shadow comes closest to upstream's focused one scaled by how much
// lighter libadwaita's unfocused shadow is at each distance from the window, without being darker than it
// anywhere.
constexpr qreal BlurRadius = 12;
constexpr qreal Opacity = 0.4;

class ShadowedDecoration : public QAdwaitaDecorations
{
protected:
    void paint(QPaintDevice *device) override
    {
        QAdwaitaDecorations::paint(device);

        // Upstream's test for its shadow, for the unfocused window instead
        const QWaylandWindow *window = waylandWindow();
        if (window->windowStates() & (Qt::WindowActive | Qt::WindowMaximized) || window->toplevelWindowTilingStates() != QWaylandWindow::WindowNoState) {
            return;
        }
        const QRect frame = window->windowContentGeometry();
        const QRect surface = frame + margins(ShadowsOnly);
        const QSize size = surface.size();
        if (m_shadow.size() != size) {
            QImage shape(size, QImage::Format_ARGB32_Premultiplied);
            shape.fill(Qt::transparent);
            QPainter p(&shape);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::black);
            // Rounded at the top only, like the frame
            p.drawRoundedRect(frame, CornerRadius, CornerRadius);
            p.drawRect(frame.adjusted(0, frame.height() / 2, 0, 0));
            p.end();
            m_shadow = QImage(size, QImage::Format_ARGB32_Premultiplied);
            m_shadow.fill(Qt::transparent);
            p.begin(&m_shadow);
            qt_blurImage(&p, shape, BlurRadius, false, false);
        }

        // Behind what upstream drew, and only in the margins, the part of this image Qt uses
        QPainter p(device);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
        p.setClipRegion(QRegion(surface) - surface.marginsRemoved(margins()));
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setOpacity(Opacity);
        p.drawImage(QPoint(), m_shadow);
    }

private:
    QImage m_shadow;
};

class DecorationPlugin : public QWaylandDecorationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QWaylandDecorationFactoryInterface_iid FILE "DecorationPlugin.json")
public:
    QWaylandAbstractDecoration *create(const QString &, const QStringList &) override { return new ShadowedDecoration; }
};
} // namespace

#include "DecorationPlugin.moc"
