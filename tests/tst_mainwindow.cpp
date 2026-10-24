// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestSysroot.h"
#include "app/CheckRow.h"
#include "app/MainWindow.h"

#include <QIcon>
#include <QPushButton>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;

// Drives the window offscreen. Set EACHECK_SCREENSHOT_DIR to also save screenshots.
class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    TestSysroot m_root;

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
        m_root.write(u"/sys/kernel/security/lsm"_s, "capability,bpf");
    }

    void runAndElevate()
    {
        MainWindow window(HostAccess(HostAccess::Transport::Direct, {}, m_root.path()));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QCOMPARE(window.findChildren<CheckRow *>().size(), 8);
        QPushButton *runFull = button(window, u"Run full check"_s);
        QPushButton *refresh = button(window, u"Refresh"_s);
        QVERIFY(runFull);
        QVERIFY(refresh);

        window.runChecks(false);
        QTRY_VERIFY(refresh->isEnabled());
        // Placeholder checks that need the event log show Needs admin, so the button appears.
        QVERIFY(runFull->isVisible());
        screenshot(window, u"mainwindow"_s);

        // A fixture sysroot grants admin reads without pkexec.
        runFull->click();
        QTRY_VERIFY(refresh->isEnabled());
        QVERIFY(!runFull->isVisible());
        screenshot(window, u"mainwindow-elevated"_s);
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
