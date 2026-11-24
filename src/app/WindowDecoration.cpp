// SPDX-License-Identifier: GPL-3.0-or-later
#include "WindowDecoration.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPluginLoader>
#include <QVersionNumber>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
// Qt 6 patch releases on which the plugin uses Qt's private API the same way, so a build for one
// runs on the others. Found with tools/compare-qt-releases.py. A release not listed only runs a
// build for itself. Only patch releases matter: the app doesn't start on an older minor release
// than it was built for.
struct PatchReleases {
    int minor;
    int first;
    int last;
};
constexpr PatchReleases samePrivateApi[] = {
    {5, 0, 3},
    {6, 0, 3},
    {7, 0, 3},
    {8, 0, 3},
    {9, 0, 1},
    {9, 2, 3}, // 6.9.2 moved a field the plugin reads
    {10, 0, 3},
    {11, 0, 3},
    {12, 0, 1},
};

bool sharesPrivateApi(const QString &built, const QString &running)
{
    const QVersionNumber a = QVersionNumber::fromString(built);
    const QVersionNumber b = QVersionNumber::fromString(running);
    if (a.segmentCount() != 3 || b.segmentCount() != 3 || a.majorVersion() != 6 || b.majorVersion() != 6 || a.minorVersion() != b.minorVersion()) {
        return false;
    }
    return std::ranges::any_of(samePrivateApi, [&](const PatchReleases &r) {
        const auto in = [&](const QVersionNumber &v) {
            return r.minor == v.minorVersion() && r.first <= v.microVersion() && v.microVersion() <= r.last;
        };
        return in(a) && in(b);
    });
}
} // namespace

Decoration chooseDecoration(const DecorationInputs &inputs)
{
    // The same test Qt uses before it picks its own adwaita plugin.
    const bool gnome = inputs.currentDesktop.split(u':').contains(u"GNOME"_s, Qt::CaseInsensitive);
    if (inputs.platformName != "wayland"_L1 || !gnome || inputs.decorationSet || !inputs.pluginInstalled) {
        return Decoration::QtPlugin;
    }
    if (inputs.runningQtVersion != inputs.builtQtVersion && !sharesPrivateApi(inputs.builtQtVersion, inputs.runningQtVersion)) {
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
        qInfo("Title bar shadow off: built for Qt %s, running on Qt %s. Rebuild or update the app to turn it back on.", QT_VERSION_STR, qVersion());
        break;
    }
#endif
}
