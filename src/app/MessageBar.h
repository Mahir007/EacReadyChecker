// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class IconWidget;
class QLabel;
class QPushButton;

// A message with an optional action, drawn like KDE's KMessageWidget: a tinted, rounded box with a
// border in the message type's colour.
class MessageBar : public QWidget
{
    Q_OBJECT

public:
    enum class Type { Information, Warning, Error };

    explicit MessageBar(QWidget *parent = nullptr);

    // Without `action`, there's no button.
    void setMessage(Type type, const QString &text, const QString &action = {});
    QPushButton *button() const { return m_button; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QColor colour() const;
    void updateIcon();

    Type m_type = Type::Information;
    IconWidget *m_icon;
    QLabel *m_text;
    QPushButton *m_button;
};
