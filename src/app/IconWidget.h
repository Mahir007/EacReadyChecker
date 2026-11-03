// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QWidget>

// An icon drawn at the window's current scale. A QLabel keeps the pixmap it was given, which goes
// blurry when the scale changes: when the desktop's scale changes, when the window moves to another
// screen, or when Wayland only reports the scale after the first frame.
class IconWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IconWidget(QWidget *parent = nullptr);

    void setIcon(const QIcon &icon);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QIcon m_icon;
};
