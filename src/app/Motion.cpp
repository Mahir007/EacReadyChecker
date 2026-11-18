// SPDX-License-Identifier: GPL-3.0-or-later
#include "Motion.h"

#include "PortalSettings.h"

#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QVariantAnimation>
#include <QWidget>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto PortalService = "org.freedesktop.portal.Desktop"_L1;
constexpr auto PortalPath = "/org/freedesktop/portal/desktop"_L1;
constexpr auto PortalInterface = "org.freedesktop.portal.Settings"_L1;
constexpr int PortalTimeoutMs = 1000;

// xdg-desktop-portal 1.21 and later: 1 means reduce motion.
constexpr auto Appearance = "org.freedesktop.appearance"_L1;
constexpr auto ReducedMotion = "reduced-motion"_L1;
// GNOME's Animations switch, in Accessibility.
constexpr auto GnomeInterface = "org.gnome.desktop.interface"_L1;
constexpr auto EnableAnimations = "enable-animations"_L1;
// KDE's animation speed slider. KDE's portal sends it as text.
constexpr auto KdeGroup = "org.kde.kdeglobals.KDE"_L1;
constexpr auto AnimationDurationFactor = "AnimationDurationFactor"_L1;

// Breeze's own duration at normal speed. It reports KDE's speed as a multiple of this.
constexpr int BreezeDurationMs = 100;

constexpr auto CrossFadeName = "eacheck-cross-fade"_L1;

struct DesktopSetting
{
    bool reducedMotion = false;
    bool gnomeAnimations = true;
    std::optional<qreal> kdeFactor;
};

DesktopSetting s_desktop;
std::optional<qreal> s_override;

// A value inside a variant arrives wrapped once more.
QVariant unwrap(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
        value = qvariant_cast<QDBusVariant>(value).variant();
    }
    return value;
}

void applySetting(const QString &group, const QString &key, const QVariant &wrapped)
{
    const QVariant value = unwrap(wrapped);
    if (group == Appearance && key == ReducedMotion) {
        s_desktop.reducedMotion = value.toUInt() == 1;
    } else if (group == GnomeInterface && key == EnableAnimations) {
        s_desktop.gnomeAnimations = value.toBool();
    } else if (group == KdeGroup && key == AnimationDurationFactor) {
        bool ok = false;
        const qreal factor = value.toDouble(&ok);
        s_desktop.kdeFactor = ok ? std::optional(std::max(0.0, factor)) : std::nullopt;
    }
}

class PortalListener : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public Q_SLOTS:
    void settingChanged(const QString &group, const QString &key, const QDBusVariant &value)
    {
        applySetting(group, key, value.variant());
    }
};

// The widget as it was, drawn over it as it is now and fading out.
class CrossFadeOverlay : public QWidget
{
public:
    CrossFadeOverlay(const QPixmap &before, QWidget *parent)
        : QWidget(parent)
        , m_before(before)
    {
        setObjectName(CrossFadeName);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setGeometry(QRect(QPoint(), before.deviceIndependentSize().toSize()));
    }

    void setOpacity(qreal opacity)
    {
        m_opacity = opacity;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setOpacity(m_opacity);
        painter.drawPixmap(0, 0, m_before);
    }

private:
    QPixmap m_before;
    qreal m_opacity = 1;
};
} // namespace

namespace Motion
{
qreal factor()
{
    if (s_override) {
        return *s_override;
    }
    if (s_desktop.reducedMotion || !s_desktop.gnomeAnimations) {
        return 0;
    }
    if (s_desktop.kdeFactor) {
        return *s_desktop.kdeFactor;
    }
    // Breeze reads KDE's setting itself, also where there's no portal.
    const QStyle *style = QApplication::style();
    if (style && style->name().compare(u"breeze"_s, Qt::CaseInsensitive) == 0) {
        return qreal(style->styleHint(QStyle::SH_Widget_Animation_Duration)) / BreezeDurationMs;
    }
    return 1;
}

int duration(int ms)
{
    return int(std::lround(ms * factor()));
}

void followDesktop()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusMessage message = QDBusMessage::createMethodCall(PortalService, PortalPath, PortalInterface, u"ReadAll"_s);
    message << QStringList{Appearance, GnomeInterface, KdeGroup};
    // Not blocking, unlike the colour scheme: nothing moves until the checks finish, and starting
    // a portal that isn't running yet can take a while.
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, PortalTimeoutMs), qApp);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, qApp, [](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const auto settings = portalSettings(call->reply());
        for (auto group = settings.begin(); group != settings.end(); ++group) {
            for (auto entry = group->begin(); entry != group->end(); ++entry) {
                applySetting(group.key(), entry.key(), entry.value());
            }
        }
    });
    bus.connect(PortalService,
                PortalPath,
                PortalInterface,
                u"SettingChanged"_s,
                new PortalListener(qApp),
                SLOT(settingChanged(QString, QString, QDBusVariant)));
}

void overrideFactor(std::optional<qreal> factor)
{
    s_override = factor;
}

void crossFade(QWidget *widget, const std::function<void()> &change)
{
    QWidget *running = widget->findChild<QWidget *>(CrossFadeName, Qt::FindDirectChildrenOnly);
    if (duration(Long) == 0 || !widget->isVisible()) {
        delete running;
        change();
        return;
    }
    // A fade that's still running is part of the picture, so the new one starts from it.
    const QPixmap before = widget->grab();
    delete running;
    change();

    auto *overlay = new CrossFadeOverlay(before, widget);
    overlay->show();
    overlay->raise();
    auto *animation = new QVariantAnimation(overlay);
    animation->setDuration(duration(Long));
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    QObject::connect(animation, &QVariantAnimation::valueChanged, overlay, [overlay](const QVariant &value) {
        overlay->setOpacity(value.toReal());
    });
    QObject::connect(animation, &QVariantAnimation::finished, overlay, &QObject::deleteLater);
    animation->start();
}
} // namespace Motion

#include "Motion.moc"
