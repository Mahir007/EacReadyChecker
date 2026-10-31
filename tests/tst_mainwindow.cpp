// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/CheckRow.h"
#include "app/Icons.h"
#include "app/MainWindow.h"
#include "core/AppInfo.h"

#include <QApplication>
#include <QClipboard>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStyle>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QToolButton>
#include <QTest>

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
        QPushButton *runFull = button(window, u"Run full check"_s);
        QPushButton *refresh = button(window, u"Refresh"_s);
        QVERIFY(runFull);
        QVERIFY(refresh);

        window.runChecks(false);
        QTRY_VERIFY(refresh->isEnabled());
        // Checks that need the event log show Needs admin, so the button appears.
        QVERIFY(runFull->isVisible());

        // Expand the IOMMU row's sub-checks.
        const QList<CheckRow *> rows = window.findChildren<CheckRow *>();
        QToolButton *expander = rows[4]->findChild<QToolButton *>();
        QVERIFY(expander && expander->isVisible());
        expander->click();
        const QLabel *subDetail = rows[4]->findChildren<QLabel *>().constLast();
        QTRY_VERIFY(subDetail->isVisible() && subDetail->height() > 0);
        screenshot(window, u"mainwindow"_s);

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
        QCOMPARE(rows[0]->findChildren<QLabel *>().size(), 3); // icon, title, detail: no sub-checks
        QCOMPARE(detail(1), tpmDetail);
        QTRY_VERIFY(refresh->isEnabled());
        QVERIFY(kernelExpander->isVisible());
        QTRY_VERIFY(rows[0]->findChildren<QLabel *>().constLast()->isVisible());
        QVERIFY(!runFull->isVisible());
        screenshot(window, u"mainwindow-elevated"_s);
    }

    void copyReportAndAbout()
    {
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_fixture));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.runChecks(false);
        QPushButton *copy = button(window, u"Copy report"_s);
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
        QTRY_COMPARE(copy->text(), u"Copy report"_s);

        button(window, u"About"_s)->click();
        auto *about = window.findChild<QMessageBox *>();
        QVERIFY(about);
        QTRY_VERIFY(about->isVisible());
        QVERIFY(about->text().contains(AppInfo::version()));
        QVERIFY(about->text().contains(AppInfo::requirementsUrl()));
        screenshot(*about, u"about"_s);
        about->close();
    }

    void darkColourScheme()
    {
        // GNOME: Adwaita lacks most of our icons, so they come from the Breeze fallback. With a
        // dark palette that has to be breeze-dark, or the glyphs are dark on dark. Switch at
        // runtime too, as when the desktop's colour scheme changes while the app runs.
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
        QPushButton *refresh = button(window, u"Refresh"_s);
        QTRY_VERIFY(refresh->isEnabled());
        screenshot(window, u"mainwindow-dark"_s);
        QCOMPARE(QIcon::fallbackThemeName(), u"breeze-dark"_s);
        QVERIFY(glyphLightness(refresh->icon()) > 128);

        QApplication::setPalette(QApplication::style()->standardPalette());
        QTRY_COMPARE(QIcon::fallbackThemeName(), u"breeze"_s);
        QVERIFY(glyphLightness(refresh->icon()) < 128);
        QApplication::setPalette(breezeDarkPalette());
        QTRY_COMPARE(QIcon::fallbackThemeName(), u"breeze-dark"_s);
        QVERIFY(glyphLightness(refresh->icon()) > 128);

        QApplication::setPalette(palette);
        QIcon::setThemeName(theme);
    }

private:
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
