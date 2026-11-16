// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>

// Follows GNOME's light and dark styles, which Qt's GTK theme plugin gets wrong in three ways. It keeps
// GTK's light palette in dark, because GTK 3 only knows dark from its theme name. Its palettes give lists
// the window's colour. And since Qt 6.10 it can report light at startup while the desktop is dark,
// because it uses one of its two portal replies before the other arrives. This asks the settings portal
// for the scheme directly, like Qt's title bar plugin does, and only uses what Qt reports if the portal
// doesn't answer. Call once, after QApplication.
void followDesktopColorScheme();

// Sets libadwaita's colours over the platform's palette where it needs them. For a dark scheme, a dark
// palette if the platform's isn't dark or gives lists the window's colour. For other schemes, separate
// window and list colours if the platform's palette has one colour for both. Undoes them otherwise.
void applyColorScheme(Qt::ColorScheme scheme);
