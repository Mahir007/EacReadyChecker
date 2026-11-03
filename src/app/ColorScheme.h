// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>

// Follows GNOME's dark style. Qt's GTK theme plugin reports it as the colour scheme, but keeps GTK's
// light palette, because GTK 3 only knows dark from its theme name. Call once, after QApplication.
void followDesktopColorScheme();

// Sets a dark palette if the scheme is dark and the current palette isn't. Undoes it for other schemes.
void applyColorScheme(Qt::ColorScheme scheme);
