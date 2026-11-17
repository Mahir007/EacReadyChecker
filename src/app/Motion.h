// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

#include <functional>
#include <optional>

class QWidget;

// The window's animations follow the desktop's setting: KDE's animation speed, GNOME's
// Animations switch, or the settings portal's reduced motion.
namespace Motion
{
// Durations at normal speed, in ms.
constexpr int Short = 100; // hover
constexpr int Long = 200; // fades, and rows opening and closing

// How long animations take, as a multiple of their normal duration. 0 when they're off.
qreal factor();

// `ms` scaled by factor(). 0 means don't animate.
int duration(int ms);

// Reads the desktop's setting from the settings portal and follows its changes. Call once, after
// QApplication. Until the portal answers, it's the style's setting (KDE's, under Breeze).
void followDesktop();

// Replaces the desktop's factor, or with nullopt goes back to it. Tests use 0 to turn animations off.
void overrideFactor(std::optional<qreal> factor);

// Runs `change`, then fades from how `widget` looked before it to how it looks after.
void crossFade(QWidget *widget, const std::function<void()> &change);
} // namespace Motion
