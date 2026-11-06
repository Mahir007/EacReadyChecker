// SPDX-License-Identifier: GPL-3.0-or-later
#include "WindowDecoration.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPluginLoader>

using namespace Qt::StringLiterals;

Decoration chooseDecoration(const DecorationInputs &inputs)
{
    // The same test Qt uses before it picks its own adwaita plugin.
    const bool gnome = inputs.currentDesktop.split(u':').contains(u"GNOME"_s, Qt::CaseInsensitive);
    if (inputs.platformName != "wayland"_L1 || !gnome || inputs.decorationSet || !inputs.pluginInstalled) {
        return Decoration::QtPlugin;
    }
    // Even patch releases have changed what the plugin relies on (6.9.2 moved a field it reads).
    if (inputs.runningQtVersion != inputs.builtQtVersion) {
        return Decoration::QtVersionDiffers;
    }
    return Decoration::Bundled;
}

void useBundledDecoration([[maybe_unused]] bool decorationSet)
{
#ifdef EACHECK_DECORATION_PLUGIN_DIR
    const QString dir = QDir::cleanPath(QCoreApplication::applicationDirPath() + u'/' + QStringLiteral(EACHECK_DECORATION_PLUGIN_DIR));
    const QString file = dir + "/wayland-decoration-client/"_L1 + QStringLiteral(EACHECK_DECORATION_PLUGIN_FILE);
    const DecorationInputs inputs{
        .platformName = QGuiApplication::platformName(),
        .currentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        .decorationSet = decorationSet,
        .pluginInstalled = QFileInfo::exists(file),
        .builtQtVersion = QStringLiteral(QT_VERSION_STR),
        .runningQtVersion = QString::fromLatin1(qVersion()),
    };
    switch (chooseDecoration(inputs)) {
    case Decoration::QtPlugin:
        break;
    case Decoration::Bundled: {
        // If Qt can't load the plugin it picks, the window has no title bar, so try it here first.
        // This catches a missing library, such as Qt SVG removed since the build.
        QPluginLoader plugin(file);
        if (!plugin.load()) {
            qWarning("Using Qt's title bar: %s", qPrintable(plugin.errorString()));
            break;
        }
        QCoreApplication::addLibraryPath(dir);
        // Processes started from here inherit this. A Qt app without the plugin warns and uses its
        // default title bar.
        qputenv("QT_WAYLAND_DECORATION", "qadwaitadecorations");
        break;
    }
    case Decoration::QtVersionDiffers:
        qInfo("Title bar shadow off: built for Qt %s, running on Qt %s. Rebuild or update the app to turn it back on.",
              QT_VERSION_STR,
              qVersion());
        break;
    }
#endif
}
