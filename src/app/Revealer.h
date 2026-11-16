// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QVariantAnimation;

// Shows its content by sliding it down from under the widget above, as GTK's revealer does. The
// content keeps its full size and is cut off, so its text doesn't squeeze while it moves. Hidden
// while closed.
class Revealer : public QWidget
{
    Q_OBJECT

public:
    explicit Revealer(QWidget *content, QWidget *parent = nullptr);

    // Opens or closes. Without `animate`, or with animations off, at once.
    void setRevealed(bool revealed, bool animate);
    // How far open: 0 closed, 1 open.
    qreal progress() const { return m_progress; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

Q_SIGNALS:
    // Its height changed, so the layouts around it need to run again. Sent on each step.
    void progressChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setProgress(qreal progress);
    int contentHeight(int width) const;
    void placeContent();

    QWidget *m_content;
    QVariantAnimation *m_animation;
    qreal m_progress = 0;
};
