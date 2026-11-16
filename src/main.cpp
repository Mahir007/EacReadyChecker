// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ColorScheme.h"
#include "app/Icons.h"
#include "app/MainWindow.h"
#include "app/Motion.h"
#include "app/WindowDecoration.h"
#include "cli/CliMain.h"
#include "core/AppInfo.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStandardPaths>

#include <memory>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    const bool cli = isCliInvocation(argc, argv);

    std::unique_ptr<QCoreApplication> app;
    if (cli) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    } else {
        // Wayland only. Respect an explicit override for debugging.
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            qputenv("QT_QPA_PLATFORM", "wayland");
        }
        // Qt adds fake bold on top of a variable font's real Bold (QTBUG-112136), which smears
        // Adwaita Sans. A font with no bold at all stays regular instead.
        qputenv("QT_NO_SYNTHESIZED_BOLD", "1");
        // Wayland app_id: lets the compositor match the window to the .desktop file and icon.
        // Only when that file is installed. Qt registers the name with the host portal, which
        // refuses an ID it has no desktop file for, as when the app runs from its build folder.
        // Set before QApplication, which registers right away if it's set. Otherwise Qt
        // registers later, after the first portal call, and the portal refuses.
        if (!QStandardPaths::locate(QStandardPaths::ApplicationsLocation, AppInfo::id() + u".desktop"_s).isEmpty()) {
            QGuiApplication::setDesktopFileName(AppInfo::id());
        }
        // Read before QApplication: in the KDE Flatpak runtime, Qt sets it on GNOME while it starts.
        const bool decorationSet = !qEnvironmentVariableIsEmpty("QT_WAYLAND_DECORATION");
        app = std::make_unique<QApplication>(argc, argv);
        QGuiApplication::setApplicationDisplayName(AppInfo::displayName());
        // Icons the theme lacks come from Breeze (MainWindow sets that up, see Icons.h), then
        // from icons bundled in the app.
        QApplication::setWindowIcon(appIcon());
        followDesktopColorScheme();
        Motion::followDesktop();
        useBundledDecoration(decorationSet);
    }
    QCoreApplication::setApplicationName(AppInfo::binaryName());
    QCoreApplication::setApplicationVersion(AppInfo::version());

    QCommandLineParser parser;
    const QCommandLineOption jsonOption(u"json"_s, u"Print the results as JSON and exit."_s);
    const QCommandLineOption textOption(u"text"_s, u"Print the results as plain text and exit."_s);
    const QCommandLineOption elevateOption(u"elevate"_s, u"Also run the checks that need admin access (asks for a password once)."_s);
    const QCommandLineOption sysrootOption(u"sysroot"_s, u"Read host files below <dir> instead of / (for fixtures and testing)."_s, u"dir"_s);
    parser.setApplicationDescription(AppInfo::description());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({jsonOption, textOption, elevateOption, sysrootOption});
    parser.process(*app);

    const HostAccess host = HostAccess::forCurrentEnvironment(parser.value(sysrootOption));
    const bool elevate = parser.isSet(elevateOption);

    if (cli) {
        return runCli(host, parser.isSet(jsonOption) ? CliFormat::Json : CliFormat::Text, elevate);
    }

    MainWindow window(host);
    window.show();
    window.runChecks(elevate);
    return app->exec();
}
