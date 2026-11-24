// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// On GNOME the title bar comes from a Qt plugin. Qt's own paints no shadow, and from Qt 6.10.1 to
// 6.11.2 its light-mode border doesn't show on white. The app bundles QAdwaitaDecorations for both.
// It uses Qt's private API, so it only runs on the Qt it was built with or a
// patch release that uses that API the same way: a plugin built for another version can crash the app.
enum class Decoration {
    QtPlugin, // Qt picks the title bar, as without the bundled plugin
    Bundled,
    QtVersionDiffers, // would be Bundled, but Qt changed since the build
};

struct DecorationInputs {
    QString platformName;       // QGuiApplication::platformName()
    QString currentDesktop;     // XDG_CURRENT_DESKTOP
    bool decorationSet = false; // the user set QT_WAYLAND_DECORATION
    bool pluginInstalled = false;
    QString builtQtVersion;
    QString runningQtVersion;
};

Decoration chooseDecoration(const DecorationInputs &inputs);

// Makes Qt use the bundled plugin when chooseDecoration() says so. Call after QApplication and before
// the first window. decorationSet is whether QT_WAYLAND_DECORATION was set before QApplication was
// created: in the KDE Flatpak runtime, Qt sets it to adwaita on GNOME while it starts.
void useBundledDecoration(bool decorationSet);
