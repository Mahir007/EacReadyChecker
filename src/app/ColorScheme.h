// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>

// Follows GNOME's dark style, which Qt's GTK theme plugin gets wrong in two ways. It keeps GTK's light
// palette, because GTK 3 only knows dark from its theme name. And since Qt 6.10 it can report light
// at startup while the desktop is dark, because it uses one of its two portal replies before the other
// arrives. So this asks the settings portal directly, like Qt's title bar plugin does, and only uses
// what Qt reports if the portal doesn't answer. Call once, after QApplication.
void followDesktopColorScheme();

// Sets a dark palette if the scheme is dark and the current palette isn't. Undoes it for other schemes.
void applyColorScheme(Qt::ColorScheme scheme);
