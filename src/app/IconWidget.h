// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QWidget>

// An icon drawn at the window's current scale. A QLabel keeps the pixmap it was given, which goes
// blurry when the desktop's scale changes or the window moves to a screen with another scale.
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
