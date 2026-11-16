// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QWidget>

class QVariantAnimation;

// An icon drawn at the window's current scale. A QLabel keeps the pixmap it was given, which goes
// blurry when the desktop's scale changes or the window moves to a screen with another scale.
class IconWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IconWidget(QWidget *parent = nullptr);

    // Shows `icon` at once, as after a theme change.
    void setIcon(const QIcon &icon);
    // Fades from the current icon to `icon`, for a new status.
    void fadeTo(const QIcon &icon);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QIcon m_icon;
    QIcon m_previous; // fading out
    QVariantAnimation *m_fade;
};
