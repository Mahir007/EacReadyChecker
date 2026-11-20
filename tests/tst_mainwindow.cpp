// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/CheckRow.h"
#include "app/ColorScheme.h"
#include "app/IconWidget.h"
#include "app/Icons.h"
#include "app/MainWindow.h"
#include "app/MessageBar.h"
#include "app/Motion.h"
#include "app/Revealer.h"
#include "app/SummaryBanner.h"
#include "core/AppInfo.h"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QGraphicsEffect>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QPalette>
#include <QStyle>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStandardPaths>
#include <QToolButton>
#include <QTest>
#include <QVariantAnimation>

#include <algorithm>

using namespace Qt::StringLiterals;

// Drives the window offscreen. Set EACHECK_SCREENSHOT_DIR to also save screenshots.
class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    const QString m_fixture = QStringLiteral(EACHECK_FIXTURES_DIR) + u"/ubuntu-default"_s;

    static QPushButton *button(const QWidget &window, const QString &text)
    {
        const auto buttons = window.findChildren<QPushButton *>();
        for (QPushButton *b : buttons) {
            if (b->text() == text) {
                return b;
            }
        }
        return nullptr;
    }

    // A run has finished when Copy Report is enabled again. Found by its menu, since it may say
    // "Copied".
    static bool finished(const QWidget &window)
    {
        const auto buttons = window.findChildren<QPushButton *>();
        return std::any_of(buttons.begin(), buttons.end(), [](const QPushButton *b) {
            return b->menu() && b->isEnabled();
        });
    }

    static QString message(const QWidget &window)
    {
        return window.findChild<MessageBar *>()->findChild<QLabel *>()->text();
    }

    // The message bar's Revealer.
    static Revealer *messageReveal(const QWidget &window)
    {
        return qobject_cast<Revealer *>(window.findChild<MessageBar *>()->parentWidget()->parentWidget());
    }

    static void screenshot(QWidget &window, const QString &name)
    {
        const QString dir = qEnvironmentVariable("EACHECK_SCREENSHOT_DIR");
        if (!dir.isEmpty()) {
            window.grab().save(dir + u'/' + name + u".png"_s);
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        // The offscreen platform has no icon search paths. Use the system's, for screenshots.
        QIcon::setThemeSearchPaths(QIcon::themeSearchPaths()
                                   + QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, u"icons"_s, QStandardPaths::LocateDirectory));
        QIcon::setThemeName(u"breeze"_s);
        // Off, so each change is done when the call returns. animations() turns them on.
        Motion::overrideFactor(0);
    }

    void bundledIconsWithoutTheme()
    {
        // No theme has our icons (e.g. Adwaita without Breeze installed): use the bundled ones.
        const QString theme = QIcon::themeName();
        const QString fallback = QIcon::fallbackThemeName();
        QIcon::setThemeName(u"no-such-theme"_s);
        QIcon::setFallbackThemeName(u"no-such-theme"_s);
        for (CheckStatus status : {CheckStatus::Pass, CheckStatus::Fail, CheckStatus::NotRequired, CheckStatus::NeedsAdmin, CheckStatus::Unknown}) {
            QVERIFY2(!statusIcon(status).pixmap(16).isNull(), qPrintable(statusKey(status)));
        }
        for (Verdict verdict : {Verdict::Ready, Verdict::NotReady, Verdict::Incomplete}) {
            QVERIFY2(!verdictIcon(verdict).pixmap(48).isNull(), qPrintable(verdictKey(verdict)));
        }
        QVERIFY(!appIcon().pixmap(48).isNull());
        QIcon::setThemeName(theme);
        QIcon::setFallbackThemeName(fallback);
    }

    void runAndElevate()
    {
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QCOMPARE(window.findChildren<CheckRow *>().size(), 8);
        QPushButton *runFull = window.findChild<MessageBar *>()->button();
        // No Refresh button: F5 runs the checks again.
        QVERIFY(!button(window, u"Refresh"_s));
        QVERIFY(!runFull->isVisible());

        window.runChecks(false);
        QTRY_VERIFY(finished(window));
        // Checks that need the event log show Needs admin, so the message and its button appear.
        QVERIFY(runFull->isVisible() && runFull->isEnabled());
        QCOMPARE(runFull->text(), u"Run Full Check"_s);
        QCOMPARE(message(window), u"Some checks need admin access."_s);

        // The list is on the View colours (Base), not the window's.
        const QList<CheckRow *> rows = window.findChildren<CheckRow *>();
        QCOMPARE(rows[0]->backgroundRole(), QPalette::Base);
        QVERIFY(rows[0]->parentWidget()->autoFillBackground());

        // A passing row has no detail line. TPM passes on this fixture.
        QVERIFY(!rows[1]->findChild<QLabel *>(u"detail"_s)->isVisible());

        // Expand the IOMMU row's sub-checks with its arrow, and collapse it by clicking its title.
        QToolButton *expander = rows[4]->findChild<QToolButton *>();
        QVERIFY(expander && expander->isVisible());
        expander->click();
        const QLabel *subDetail = rows[4]->findChildren<QLabel *>().constLast();
        QTRY_VERIFY(subDetail->isVisible() && subDetail->height() > 0);
        screenshot(window, u"mainwindow"_s);
        QLabel *iommuTitle = rows[4]->findChildren<QLabel *>().constFirst();
        QCOMPARE(iommuTitle->text(), u"IOMMU"_s);
        QTest::mouseMove(iommuTitle);
        QTRY_VERIFY(iommuTitle->parentWidget()->underMouse());
        screenshot(window, u"mainwindow-hover"_s);
        QTest::mouseClick(iommuTitle, Qt::LeftButton);
        QTRY_VERIFY(!subDetail->isVisible());
        QTest::mouseClick(iommuTitle, Qt::LeftButton);
        QTRY_VERIFY(subDetail->isVisible());
        // A row without sub-checks doesn't expand.
        QLabel *tpmTitle = rows[1]->findChildren<QLabel *>().constFirst();
        const QSize tpmSize = rows[1]->size();
        QTest::mouseClick(tpmTitle, Qt::LeftButton);
        QCOMPARE(rows[1]->size(), tpmSize);

        // A fixture sysroot grants admin reads without pkexec. While it runs, only the rows that
        // need admin go back to "Checking…".
        const auto detail = [&rows](int row) {
            return rows[row]->findChild<QLabel *>(u"detail"_s)->text();
        };
        // An expanded row that's waiting hides its old sub-checks, then shows the new ones.
        QToolButton *kernelExpander = rows[0]->findChild<QToolButton *>();
        kernelExpander->click();
        QTRY_VERIFY(rows[0]->findChildren<QLabel *>().constLast()->isVisible());
        const QString tpmDetail = detail(1);
        runFull->click();
        QCOMPARE(detail(0), u"Checking…"_s); // kernel signing
        QVERIFY(!kernelExpander->isVisible());
        QCOMPARE(rows[0]->findChildren<QLabel *>().size(), 2); // title, detail: no sub-checks
        QCOMPARE(detail(1), tpmDetail);
        // The message stays while the password dialog is open, so it's clear what it's for.
        QVERIFY(runFull->isVisible() && !runFull->isEnabled());
        QTRY_VERIFY(finished(window));
        QVERIFY(kernelExpander->isVisible());
        QTRY_VERIFY(rows[0]->findChildren<QLabel *>().constLast()->isVisible());
        QVERIFY(!runFull->isVisible());
        screenshot(window, u"mainwindow-elevated"_s);

        // F5 with admin access asks for the password again. Same rows wait.
        QTest::keyClick(&window, Qt::Key_F5);
        QCOMPARE(detail(0), u"Checking…"_s);
        QCOMPARE(detail(1), tpmDetail);
        // F5 again during the run doesn't start another.
        QTest::keyClick(&window, Qt::Key_F5);
        QTRY_VERIFY(finished(window));
        QVERIFY(!runFull->isVisible());
    }

    void copyReportAndAbout()
    {
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QPushButton *copy = button(window, u"Copy Report"_s);
        QTRY_VERIFY(copy->isEnabled());

        const int width = copy->width();
        const auto actions = copy->menu()->actions();
        QCOMPARE(actions.size(), 2);
        actions[0]->trigger(); // as text
        QVERIFY(QGuiApplication::clipboard()->text().startsWith(u"EACheck "_s));
        QCOMPARE(copy->text(), u"Copied"_s);
        QCOMPARE(copy->width(), width);
        actions[1]->trigger(); // as JSON
        QCOMPARE(QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object().value(u"checks"_s).toArray().size(), 8);
        QTRY_COMPARE(copy->text(), u"Copy Report"_s);

        button(window, u"About"_s)->click();
        auto *about = window.findChild<QMessageBox *>();
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        QVERIFY(about->text().contains(AppInfo::version()));
        QVERIFY(about->text().contains(AppInfo::requirementsUrl()));
        QVERIFY(about->text().contains(u"EAC performs its own verification"_s));
        screenshot(*about, u"about"_s);
        about->close();
    }

    void menuBar()
    {
        // The menu bar is only shown as a global menu. Offscreen has none, so it stays hidden, and
        // its shortcuts work anyway (F5 in runAndElevate).
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *bar = window.findChild<QMenuBar *>();
        QVERIFY(bar);
        QVERIFY(!bar->isNativeMenuBar());
        QVERIFY(!bar->isVisible());

        const auto texts = [](const QList<QAction *> &actions) {
            QStringList list;
            for (const QAction *action : actions) {
                list.append(action->text());
            }
            return list;
        };
        const QList<QAction *> menus = bar->actions();
        QCOMPARE(texts(menus), (QStringList{u"&File"_s, u"&Edit"_s, u"&Help"_s}));
        const QList<QAction *> file = menus[0]->menu()->actions();
        const QList<QAction *> edit = menus[1]->menu()->actions();
        const QList<QAction *> help = menus[2]->menu()->actions();
        QCOMPARE(texts(file), (QStringList{u"Run &Again"_s, u"Run &Full Check…"_s, QString(), u"&Quit"_s}));
        QCOMPARE(texts(edit), (QStringList{u"Copy Report as &Text"_s, u"Copy Report as &JSON"_s}));
        QCOMPARE(texts(help), QStringList{u"&About EACheck"_s});
        QAction *runAgain = file[0];
        QAction *runFull = file[1];
        QAction *quit = file[3];
        QCOMPARE(runAgain->shortcut(), QKeySequence(Qt::Key_F5));
        QCOMPARE(quit->shortcut(), QKeySequence(QKeySequence::Quit));
        QVERIFY(!runAgain->icon().isNull() && !edit[0]->icon().isNull());

        // During a run there's nothing to copy, and no second run.
        window.runChecks(false);
        for (const QAction *action : {runAgain, runFull, edit[0], edit[1]}) {
            QVERIFY2(!action->isEnabled(), qPrintable(action->text()));
        }
        QTRY_VERIFY(finished(window));
        QVERIFY(runAgain->isEnabled());
        // Run Full Check is there while the message is.
        QVERIFY(runFull->isEnabled());

        edit[0]->trigger();
        QVERIFY(QGuiApplication::clipboard()->text().startsWith(u"EACheck "_s));
        edit[1]->trigger();
        QCOMPARE(QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object().value(u"checks"_s).toArray().size(), 8);

        runFull->trigger();
        QVERIFY(!runFull->isEnabled());
        QTRY_VERIFY(finished(window));
        QVERIFY(!runFull->isEnabled());
        QVERIFY(!window.findChild<MessageBar *>()->button()->isVisible());

        help[0]->trigger();
        auto *about = window.findChild<QMessageBox *>();
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        about->close();

        // Offscreen has no key for QKeySequence::Quit, so trigger it.
        quit->trigger();
        QTRY_VERIFY(!window.isVisible());
    }

    void breezeWithoutPlasma()
    {
        // The AppImage on Plasma: Breeze icons under Fusion, with nothing to recolour them. Before
        // darkColourScheme, which replaces Adwaita, after which Breeze always follows the palette.
        const QString style = QApplication::style()->name();
        const QPalette palette = QApplication::palette();
        QApplication::setStyle(u"Fusion"_s);
        QIcon::setThemeName(u"breeze"_s);
        QApplication::setPalette(breezeDarkPalette());
        syncIconThemes();
        QCOMPARE(QIcon::themeName(), u"breeze-dark"_s);
        QApplication::setPalette(QApplication::style()->standardPalette());
        syncIconThemes();
        QCOMPARE(QIcon::themeName(), u"breeze"_s);

        QApplication::setPalette(palette);
        QApplication::setStyle(style);
        QIcon::setThemeName(u"breeze"_s);
    }

    void darkColourScheme()
    {
        // GNOME: Adwaita is replaced by Breeze. With a dark palette that has to be breeze-dark, or
        // the glyphs are dark on dark. Switch at runtime too, as when the desktop's colour scheme
        // changes while the app runs.
        const QString theme = QIcon::themeName();
        const QPalette palette = QApplication::palette();
        QApplication::setStyle(u"Fusion"_s);
        QIcon::setThemeName(u"Adwaita"_s);
        QIcon::setFallbackThemeName(u"breeze"_s);
        QApplication::setPalette(breezeDarkPalette());

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QPushButton *copy = button(window, u"Copy Report"_s);
        QTRY_VERIFY(finished(window));
        screenshot(window, u"mainwindow-dark"_s);
        QCOMPARE(QIcon::fallbackThemeName(), u"breeze-dark"_s);
        QCOMPARE(QIcon::themeName(), u"breeze-dark"_s);
        QVERIFY(glyphLightness(copy->icon()) > 128);

        QApplication::setPalette(QApplication::style()->standardPalette());
        QTRY_COMPARE(QIcon::fallbackThemeName(), u"breeze"_s);
        QCOMPARE(QIcon::themeName(), u"breeze"_s);
        QVERIFY(glyphLightness(copy->icon()) < 128);
        QApplication::setPalette(breezeDarkPalette());
        QTRY_COMPARE(QIcon::fallbackThemeName(), u"breeze-dark"_s);
        QVERIFY(glyphLightness(copy->icon()) > 128);

        QApplication::setPalette(palette);
        QIcon::setThemeName(theme);
    }

    void yaruIsReplaced()
    {
        // Ubuntu's Yaru, in any accent colour, is replaced by Breeze like Adwaita.
        const QString theme = QIcon::themeName();
        const QString breeze = isDarkPalette(QApplication::palette()) ? u"breeze-dark"_s : u"breeze"_s;
        for (const QString &yaru : {u"Yaru"_s, u"Yaru-blue-dark"_s}) {
            QIcon::setThemeName(yaru);
            syncIconThemes();
            QCOMPARE(QIcon::themeName(), breeze);
        }
        QIcon::setThemeName(theme);
    }

    void gnomeThemesWithoutBreeze_data()
    {
        QTest::addColumn<QString>("theme");
        QTest::newRow("Adwaita") << u"Adwaita"_s;
        QTest::newRow("Yaru") << u"Yaru-blue"_s;
    }

    void gnomeThemesWithoutBreeze()
    {
        // A native build on GNOME or Ubuntu: the verdict and status icons are the bundled ones, and
        // the rows' arrows the style's, not the theme's.
        QFETCH(QString, theme);
        const QString previous = QIcon::themeName();
        const QStringList paths = QIcon::themeSearchPaths();
        QIcon::setThemeSearchPaths({});
        QIcon::setThemeName(theme);
        syncIconThemes();
        QCOMPARE(QIcon::themeName(), theme);
        QVERIFY(verdictIcon(Verdict::NotReady).name().isEmpty());
        QVERIFY(!verdictIcon(Verdict::NotReady).pixmap(48).isNull());
        QVERIFY(statusIcon(CheckStatus::Fail).name().isEmpty());
        CheckRow row(u"IOMMU"_s);
        QCOMPARE(row.findChild<QToolButton *>()->arrowType(), Qt::RightArrow);
        QIcon::setThemeSearchPaths(paths);
        QIcon::setThemeName(previous);
    }

    void breezeLinesAndArrows()
    {
        // With every style, the separators and the rows' arrows look as Breeze draws them: a 1 px
        // line, and chevrons rather than Fusion's triangles.
        const QString style = QApplication::style()->name();
        QApplication::setStyle(u"Fusion"_s);
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));

        const QList<QWidget *> lines = window.findChildren<QWidget *>(u"separator"_s);
        QCOMPARE(lines.size(), 2);
        const QImage image = window.grab().toImage();
        const QColor windowColour = window.palette().color(QPalette::Window);
        const QColor text = window.palette().color(QPalette::WindowText);
        for (const QWidget *line : lines) {
            QCOMPARE(line->height(), 1);
            const QColor drawn = image.pixelColor(line->geometry().center());
            QVERIFY(qAbs(drawn.red() - (windowColour.red() * 4 + text.red()) / 5) <= 1);
        }

        QToolButton *arrow = window.findChildren<CheckRow *>().at(4)->findChild<QToolButton *>();
        QCOMPARE(arrow->arrowType(), Qt::NoArrow);
        QCOMPARE(arrow->icon().name(), u"arrow-right"_s);
        arrow->click();
        QCOMPARE(arrow->icon().name(), u"arrow-down"_s);
        screenshot(window, u"mainwindow-breeze-lines"_s);
        QApplication::setStyle(style);
    }

    void desktopAsksForDark()
    {
        // GNOME's dark style: the platform theme reports dark and keeps a light palette.
        const QColor lightWindow = QApplication::palette().color(QPalette::Window);
        QVERIFY(!isDarkPalette(QApplication::palette()));

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));

        // The About box is open while the desktop switches. Its links follow.
        button(window, u"About"_s)->click();
        auto *about = window.findChild<QMessageBox *>();
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        auto *aboutText = about->findChild<QLabel *>(u"qt_msgbox_label"_s);
        QVERIFY(aboutText);

        applyColorScheme(Qt::ColorScheme::Dark);
        QVERIFY(isDarkPalette(QApplication::palette()));
        QTRY_VERIFY(isDarkPalette(window.palette()));
        QTRY_COMPARE(QIcon::fallbackThemeName(), u"breeze-dark"_s);
        screenshot(window, u"mainwindow-gnome-dark"_s);
        QTRY_VERIFY(hasColour(aboutText->grab().toImage(), QApplication::palette().color(QPalette::Link)));

        applyColorScheme(Qt::ColorScheme::Light);
        QCOMPARE(QApplication::palette().color(QPalette::Window), lightWindow);
        QTRY_COMPARE(QIcon::fallbackThemeName(), u"breeze"_s);
        QTRY_VERIFY(hasColour(aboutText->grab().toImage(), QApplication::palette().color(QPalette::Link)));

        // The links follow the box's own colours, such as the accent the window gets in light.
        QPalette accentLinks = about->palette();
        accentLinks.setColor(QPalette::Link, QColor(0xc8, 0x00, 0x88));
        about->setPalette(accentLinks);
        QTRY_VERIFY(hasColour(aboutText->grab().toImage(), QColor(0xc8, 0x00, 0x88)));
        about->close();
    }

    void gtkPaletteGetsListColours()
    {
        // Qt's GTK plugin gives Base the window's colour, and dims text in an unfocused window except
        // in labels, which get a palette of their own. The test sets such palettes on the app.
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        const QPalette platform = QApplication::palette();
        QPalette gtk = platform;
        gtk.setColor(QPalette::Base, gtk.color(QPalette::Window));
        gtk.setColor(QPalette::Inactive, QPalette::WindowText, Qt::gray);
        QApplication::setPalette(gtk);
        const QColor text = gtk.color(QPalette::Active, QPalette::WindowText);
        QPalette gtkLabel = gtk;
        gtkLabel.setColor(QPalette::Inactive, QPalette::WindowText, text);
        QApplication::setPalette(gtkLabel, "QLabel");

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));

        // GNOME's light style: libadwaita's sidebar grey around a white list, and no dimmed text.
        QCOMPARE(window.palette().color(QPalette::Window), QColor(0xeb, 0xeb, 0xed));
        QCOMPARE(window.palette().color(QPalette::Base), QColor(0xff, 0xff, 0xff));
        QCOMPARE(window.findChild<QToolButton *>()->palette().color(QPalette::Inactive, QPalette::WindowText), text);
        screenshot(window, u"mainwindow-gnome-light"_s);
        // Set on the window, not the app, which would replace the plugin's palette for labels.
        QCOMPARE(QApplication::palette().color(QPalette::Window), gtk.color(QPalette::Window));
        QCOMPARE(QApplication::palette("QLabel").color(QPalette::Inactive, QPalette::WindowText), text);

        button(window, u"About"_s)->click();
        auto *about = window.findChild<QMessageBox *>();
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        QCOMPARE(about->palette().color(QPalette::Window), QColor(0xeb, 0xeb, 0xed));
        about->close();

        // The dark style uses the app's dark palette, and the window's own colours go.
        applyColorScheme(Qt::ColorScheme::Dark);
        QTRY_VERIFY(isDarkPalette(window.palette()));
        QCOMPARE(window.palette().resolveMask(), 0);
        applyColorScheme(Qt::ColorScheme::Light);
        QTRY_COMPARE(window.palette().color(QPalette::Window), platform.color(QPalette::Window));

        // A dark GTK theme gives lists the window's colour too, so the dark palette replaces it.
        QPalette gtkDark = breezeDarkPalette();
        gtkDark.setColor(QPalette::Base, gtkDark.color(QPalette::Window));
        QApplication::setPalette(gtkDark);
        applyColorScheme(Qt::ColorScheme::Dark);
        QVERIFY(QApplication::palette().color(QPalette::Base).lightness() < QApplication::palette().color(QPalette::Window).lightness());
        applyColorScheme(Qt::ColorScheme::Light);
        QCOMPARE(QApplication::palette().color(QPalette::Window), platform.color(QPalette::Window));
        qunsetenv("XDG_CURRENT_DESKTOP");
    }

    void kdeThemeGetsInactiveHighlight()
    {
        // Qt's own KDE theme, as in the AppImage on Plasma: Fusion, and the same highlight for an
        // unfocused window. The window gets 29% of it over Breeze Light's window colour.
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        const QString style = QApplication::style()->name();
        const QPalette platform = QApplication::palette();
        QApplication::setStyle(u"Fusion"_s);
        QPalette kde = platform;
        kde.setColor(QPalette::Window, QColor(239, 240, 241));
        kde.setColor(QPalette::Highlight, QColor(61, 174, 233));
        QApplication::setPalette(kde);

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.palette().color(QPalette::Inactive, QPalette::Highlight), QColor(187, 221, 239));
        QCOMPARE(window.palette().color(QPalette::Active, QPalette::Highlight), QColor(61, 174, 233));
        QCOMPARE(window.palette().color(QPalette::Inactive, QPalette::HighlightedText), kde.color(QPalette::Inactive, QPalette::WindowText));
        // Set on the window, so the app's palette keeps the theme's.
        QCOMPARE(QApplication::palette().color(QPalette::Inactive, QPalette::Highlight), QColor(61, 174, 233));

        QApplication::setPalette(platform);
        QApplication::setStyle(style);
        qunsetenv("XDG_CURRENT_DESKTOP");
    }

    void infoIconFollowsAccent()
    {
        // Breeze icons under Fusion, as in the AppImage: nothing recolours Breeze's information icon,
        // so the message bar draws it in the accent. Other icon themes keep their own.
        const QString style = QApplication::style()->name();
        const QPalette platform = QApplication::palette();
        QApplication::setStyle(u"Fusion"_s);

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));
        auto *icon = window.findChild<MessageBar *>()->findChild<IconWidget *>();
        // Inside the square, beside the "i".
        const QPoint inside(icon->width() / 4, icon->height() / 2);
        const auto drawn = [&] { return icon->grab().toImage().pixelColor(inside); };

        // An accent picked while the app runs.
        const QColor green(49, 156, 35);
        QPalette accent = platform;
        accent.setColor(QPalette::Highlight, green);
        QApplication::setPalette(accent);
        QTRY_COMPARE(drawn(), green);

        const QColor red(218, 68, 83);
        QIcon::setThemeName(u"hicolor"_s);
        accent.setColor(QPalette::Highlight, red);
        QApplication::setPalette(accent);
        QTRY_VERIFY(drawn() != green);
        QVERIFY(drawn() != red);

        QIcon::setThemeName(u"breeze"_s);
        QApplication::setPalette(platform);
        QApplication::setStyle(style);
    }

    void rowsLayOutAtOnce()
    {
        // A row that expands or collapses is laid out with the list before anything is drawn.
        // Otherwise a collapsed row keeps its height for a frame, its text stretched into it.
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));
        // Short, so that expanding makes the list scroll.
        window.resize(window.width(), 300);
        QTRY_VERIFY(window.height() == 300);

        const QList<CheckRow *> rows = window.findChildren<CheckRow *>();
        QLabel *iommuTitle = rows[4]->findChildren<QLabel *>().constFirst();
        const auto geometry = [&] {
            QList<QRect> rects{rows[0]->parentWidget()->geometry(), iommuTitle->geometry()};
            for (const CheckRow *row : rows) {
                rects.append(row->geometry());
            }
            return rects;
        };
        for (int i = 0; i < 2; ++i) {
            QTest::mouseClick(iommuTitle, Qt::LeftButton);
            const QList<QRect> atClick = geometry();
            QTest::qWait(50);
            QCOMPARE(geometry(), atClick);
        }
    }

    void animations()
    {
        // Three times slower than normal, so each animation is still running when it's checked.
        constexpr qreal slow = 3;
        Motion::overrideFactor(slow);
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QPushButton *runFull = window.findChild<MessageBar *>()->button();
        Revealer *bar = messageReveal(window);
        QVERIFY(bar);
        auto *reveal = bar->findChild<QVariantAnimation *>();
        QVERIFY(reveal);
        bool slid = false;
        connect(reveal, &QAbstractAnimation::stateChanged, this, [&](QAbstractAnimation::State state) {
            slid = slid || state == QAbstractAnimation::Running;
        });
        window.runChecks(false);
        QTRY_VERIFY(finished(window));
        // The message slides in. Whether it's still moving isn't checked: the first paint after the
        // checks can take longer than the slide on a busy CI runner, and the animation then ends at
        // its next step.
        QVERIFY(slid);
        QCOMPARE(reveal->duration(), int(Motion::Long * slow));
        QTRY_COMPARE(bar->progress(), 1.0);

        // The IOMMU row's sub-checks slide open and closed. The row's height changes in steps, and
        // its title stays where it is, not stretched into the new height. The arrow turns.
        CheckRow *row = window.findChildren<CheckRow *>().at(4);
        QLabel *title = row->findChildren<QLabel *>().constFirst();
        QToolButton *arrow = row->findChild<QToolButton *>();
        const QRect titleGeometry = title->geometry();
        const int collapsed = row->height();
        const auto slide = [&](const QString &endArrow) {
            QList<int> heights{row->height()};
            bool turning = false;
            QElapsedTimer timer;
            timer.start();
            while (arrow->icon().name() != endArrow && timer.elapsed() < Motion::Long * slow * 3) {
                QTest::qWait(10);
                heights.append(row->height());
                turning = turning || arrow->icon().name().isEmpty(); // drawn turned, not a theme icon
                // Its width can change, when the list grows a scroll bar.
                if (title->y() != titleGeometry.y() || title->height() != titleGeometry.height()) {
                    return QList<int>();
                }
            }
            heights.append(row->height());
            return turning ? heights : QList<int>();
        };
        arrow->click();
        const QList<int> opening = slide(u"arrow-down"_s);
        QVERIFY(!opening.isEmpty());
        QVERIFY(std::is_sorted(opening.begin(), opening.end()));
        QVERIFY(QSet<int>(opening.begin(), opening.end()).size() > 3);
        QVERIFY(opening.constLast() > collapsed);
        QTest::mouseClick(title, Qt::LeftButton);
        const QList<int> closing = slide(u"arrow-right"_s);
        QVERIFY(!closing.isEmpty());
        QVERIFY(std::is_sorted(closing.rbegin(), closing.rend()));
        QCOMPARE(closing.constLast(), collapsed);
        QVERIFY(!row->findChildren<QLabel *>().constLast()->isVisible());

        // "Copied" fades in over the button as it was, and back.
        QPushButton *copy = button(window, u"Copy Report"_s);
        copy->menu()->actions().constFirst()->trigger();
        QCOMPARE(copy->text(), u"Copied"_s);
        QVERIFY(copy->findChild<QWidget *>(u"eacheck-cross-fade"_s));
        QTRY_VERIFY(!copy->findChild<QWidget *>(u"eacheck-cross-fade"_s));

        // After the full check nothing needs admin, so the message slides out. Its button can't be
        // clicked while it does.
        runFull->click();
        QTRY_VERIFY(finished(window));
        QVERIFY(bar->isVisible() && bar->progress() > 0);
        QVERIFY(!runFull->isEnabled());
        QTRY_VERIFY(!bar->isVisible());
        Motion::overrideFactor(0);
    }

    void scrollBar()
    {
        // Fusion's own scroll bar is out of place on GNOME. Other styles keep theirs.
        const QString style = QApplication::style()->name();
        const QPalette palette = QApplication::palette();
        QApplication::setStyle(u"Fusion"_s);

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));
        screenshot(window, u"mainwindow-fusion"_s);
        // The rows fit at the default size, so make the window short enough to need the bar.
        window.resize(window.width(), 300);
        QScrollBar *bar = window.findChild<QScrollArea *>()->verticalScrollBar();
        QTRY_VERIFY(bar->isVisible());
        QCOMPARE(bar->sizeHint().width(), 12);
        const QString lightSheet = bar->styleSheet();

        // The handle's colour follows the palette, as when the desktop switches to dark.
        QApplication::setPalette(breezeDarkPalette());
        QTRY_VERIFY(bar->styleSheet() != lightSheet);
        QVERIFY(bar->styleSheet().contains(QColor(0xfc, 0xfc, 0xfc).name().mid(1)));
        QApplication::setPalette(palette);

        QApplication::setStyle(u"Windows"_s);
        QTRY_VERIFY(bar->styleSheet().isEmpty());
        QVERIFY(bar->sizeHint().width() != 12);

        QApplication::setStyle(style);
    }

    void fusionOnPlasmaLooksLikeBreeze()
    {
        // The AppImage on Plasma: Fusion, with Breeze's margins, row hover and scroll bar colours.
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        const QString style = QApplication::style()->name();
        const QPalette platform = QApplication::palette();
        QApplication::setStyle(u"Fusion"_s);
        const QColor highlight(61, 174, 233);
        QPalette kde = platform;
        kde.setColor(QPalette::Base, Qt::white);
        kde.setColor(QPalette::Highlight, highlight);
        QApplication::setPalette(kde);

        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QTRY_VERIFY(finished(window));

        // 10 px around the window's contents and 6 px inside, where Fusion has 9 for both. The
        // list's 4 px and a row's 6 line the arrows up with the banner's icon. The arrow has 6 px
        // around its icon, as Breeze's tool buttons do.
        QCOMPARE(window.findChild<SummaryBanner *>()->layout()->contentsMargins(), QMargins(10, 10, 10, 10));
        const QList<CheckRow *> rows = window.findChildren<CheckRow *>();
        QLabel *title = rows[4]->findChildren<QLabel *>().constFirst();
        QWidget *header = title->parentWidget();
        QCOMPARE(header->layout()->contentsMargins(), QMargins(6, 6, 6, 6));
        QCOMPARE(rows[4]->parentWidget()->layout()->contentsMargins(), QMargins(4, 4, 4, 4));
        const int arrow = window.style()->pixelMetric(QStyle::PM_SmallIconSize) + 12;
        QCOMPARE(rows[4]->findChild<QToolButton *>()->size(), QSize(arrow, arrow));

        // Breeze's hover: 30% of the highlight over the list, in a line of it at 80%.
        QTest::mouseMove(title);
        QTRY_VERIFY(header->underMouse());
        screenshot(window, u"mainwindow-plasma-hover"_s);
        const QImage image = window.grab().toImage();
        const int middle = header->height() / 2;
        const QColor fill = image.pixelColor(header->mapTo(&window, QPoint(header->width() - 6, middle)));
        const auto mix = [](int base, int colour) { return base + (colour - base) * 0.3; };
        QVERIFY(std::abs(fill.red() - mix(255, highlight.red())) <= 1);
        QVERIFY(std::abs(fill.green() - mix(255, highlight.green())) <= 1);
        QVERIFY(std::abs(fill.blue() - mix(255, highlight.blue())) <= 1);
        const QColor line = image.pixelColor(header->mapTo(&window, QPoint(header->width() - 3, middle)));
        QVERIFY(line.red() < fill.red() - 40);

        // The scroll bar's handle is half the highlight under the pointer, and 35% of it while the
        // list has focus.
        window.resize(window.width(), 300);
        QScrollArea *scroll = window.findChild<QScrollArea *>();
        QScrollBar *bar = scroll->verticalScrollBar();
        QTRY_VERIFY(bar->isVisible());
        QColor hover = highlight;
        hover.setAlphaF(0.5);
        QVERIFY(bar->styleSheet().contains(hover.name(QColor::HexArgb)));
        QColor focused = highlight;
        focused.setAlphaF(0.35);
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        scroll->setFocus();
        QTRY_VERIFY(bar->styleSheet().contains(focused.name(QColor::HexArgb)));
        button(window, u"About"_s)->setFocus();
        QTRY_VERIFY(!bar->styleSheet().contains(focused.name(QColor::HexArgb)));

        QApplication::setPalette(platform);
        QApplication::setStyle(style);
        qunsetenv("XDG_CURRENT_DESKTOP");
    }

private:
    static bool hasColour(const QImage &image, const QColor &colour)
    {
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y).rgb() == colour.rgb()) {
                    return true;
                }
            }
        }
        return false;
    }

    // Breeze Dark's main colours.
    static QPalette breezeDarkPalette()
    {
        QPalette p(QColor(0x31, 0x36, 0x3b), QColor(0x20, 0x23, 0x26));
        p.setColor(QPalette::WindowText, QColor(0xfc, 0xfc, 0xfc));
        p.setColor(QPalette::Text, QColor(0xfc, 0xfc, 0xfc));
        p.setColor(QPalette::ButtonText, QColor(0xfc, 0xfc, 0xfc));
        p.setColor(QPalette::Base, QColor(0x14, 0x16, 0x18));
        p.setColor(QPalette::PlaceholderText, QColor(0xa1, 0xa9, 0xb1));
        return p;
    }

    // Average lightness of an icon's opaque pixels.
    static int glyphLightness(const QIcon &icon)
    {
        const QImage image = icon.pixmap(16).toImage();
        qint64 sum = 0;
        qint64 count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (c.alpha() > 128) {
                    sum += c.lightness();
                    ++count;
                }
            }
        }
        return count > 0 ? int(sum / count) : -1;
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
