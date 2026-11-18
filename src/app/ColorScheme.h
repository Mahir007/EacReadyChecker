// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPalette>
#include <Qt>

// Follows GNOME's dark style, which Qt's GTK theme plugin gets wrong in two ways. It keeps GTK's light
// palette, because GTK 3 only knows dark from its theme name. And since Qt 6.10 it can report light
// at startup while the desktop is dark, because it uses one of its two portal replies before the other
// arrives. So this asks the settings portal directly, like Qt's title bar plugin does, and only uses
// what Qt reports if the portal doesn't answer. It also follows the accent the portal reports.
// Call once, after QApplication.
void followDesktopColorScheme();

// Sets libadwaita's dark palette if the scheme is dark and the current palette isn't, or gives lists
// the window's colour. Undoes it for other schemes.
void applyColorScheme(Qt::ColorScheme scheme);

// The colours to set on a window over the app's palette, for GNOME's light style.
// There, Qt's GTK plugin gives lists the window's colour and dims some text while the window isn't
// focused, and GNOME's accent may not be its highlight. On Plasma without KDE's platform theme, as in
// the AppImage, it's the highlight of an unfocused window. Otherwise it's an empty
// palette, which changes nothing. Set it on
// the window, not the app: an app palette replaces the plugin's palettes for labels and buttons.
QPalette windowColours(const QPalette &appPalette);
