// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/MainWindow.h"
#include "cli/CliMain.h"
#include "eacheck_version.h"

#include <KAboutData>
#include <KIconTheme>

#include <QApplication>
#include <QCommandLineParser>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
KAboutData aboutData()
{
    KAboutData about(u"eac-ready-checker"_s,
                     u"EACheck"_s,
                     QStringLiteral(EACHECK_VERSION_STRING),
                     u"Checks whether this PC meets the Easy Anti-Cheat Linux v2 requirements"_s,
                     KAboutLicense::GPL_V3,
                     u"© 2026 Mahir007"_s);
    about.setLicense(KAboutLicense::GPL_V3, KAboutLicense::OrLaterVersions);
    about.addAuthor(u"Mahir007"_s);
    about.setHomepage(u"https://github.com/Mahir007/EacReadyChecker"_s);
    about.setBugAddress("https://github.com/Mahir007/EacReadyChecker/issues");
    about.setDesktopFileName(QStringLiteral(EACHECK_APP_ID));
    return about;
}
} // namespace

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
        KIconTheme::initTheme();
        app = std::make_unique<QApplication>(argc, argv);
        QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(EACHECK_APP_ID)));
    }

    KAboutData about = aboutData();
    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    const QCommandLineOption jsonOption(u"json"_s, u"Print the results as JSON and exit."_s);
    const QCommandLineOption textOption(u"text"_s, u"Print the results as plain text and exit."_s);
    const QCommandLineOption elevateOption(u"elevate"_s, u"Also run the checks that need admin access (asks for a password once)."_s);
    const QCommandLineOption sysrootOption(u"sysroot"_s, u"Read host files below <dir> instead of / (for fixtures and testing)."_s, u"dir"_s);
    parser.addOptions({jsonOption, textOption, elevateOption, sysrootOption});
    about.setupCommandLine(&parser);
    parser.process(*app);
    about.processCommandLine(&parser);

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
