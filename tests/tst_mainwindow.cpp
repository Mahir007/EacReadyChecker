// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/CheckRow.h"
#include "app/Icons.h"
#include "app/MainWindow.h"

#include <QIcon>
#include <QLabel>
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
        QVERIFY(!QIcon(u":/icons/io.github.Mahir007.EacReadyChecker.svg"_s).pixmap(48).isNull());
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
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
