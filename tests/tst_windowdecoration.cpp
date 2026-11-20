// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/WindowDecoration.h"

#include <QTest>

using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(Decoration)
Q_DECLARE_METATYPE(DecorationInputs)

class TestWindowDecoration : public QObject
{
    Q_OBJECT

private:
    static DecorationInputs gnome()
    {
        return {
            .platformName = u"wayland"_s,
            .currentDesktop = u"GNOME"_s,
            .decorationSet = false,
            .pluginInstalled = true,
            .builtQtVersion = u"6.11.2"_s,
            .runningQtVersion = u"6.11.2"_s,
        };
    }

private Q_SLOTS:
    void choice_data()
    {
        QTest::addColumn<DecorationInputs>("inputs");
        QTest::addColumn<Decoration>("expected");

        QTest::newRow("GNOME") << gnome() << Decoration::Bundled;

        DecorationInputs in = gnome();
        in.currentDesktop = u"ubuntu:GNOME"_s;
        QTest::newRow("Ubuntu's GNOME") << in << Decoration::Bundled;

        in = gnome();
        in.currentDesktop = u"GNOME-Classic:GNOME"_s;
        QTest::newRow("GNOME Classic") << in << Decoration::Bundled;

        in = gnome();
        in.currentDesktop = u"KDE"_s;
        QTest::newRow("Plasma") << in << Decoration::QtPlugin;

        in = gnome();
        in.currentDesktop.clear();
        QTest::newRow("no desktop") << in << Decoration::QtPlugin;

        in = gnome();
        in.platformName = u"xcb"_s;
        QTest::newRow("X11") << in << Decoration::QtPlugin;

        in = gnome();
        in.decorationSet = true;
        QTest::newRow("QT_WAYLAND_DECORATION set") << in << Decoration::QtPlugin;

        in = gnome();
        in.pluginInstalled = false;
        QTest::newRow("plugin not installed") << in << Decoration::QtPlugin;

        in = gnome();
        in.runningQtVersion = u"6.11.3"_s;
        QTest::newRow("newer patch release") << in << Decoration::Bundled;

        in = gnome();
        in.runningQtVersion = u"6.11.0"_s;
        QTest::newRow("older patch release") << in << Decoration::Bundled;

        in = gnome();
        in.builtQtVersion = u"6.9.1"_s;
        in.runningQtVersion = u"6.9.2"_s;
        QTest::newRow("newer patch release, private API changed") << in << Decoration::QtVersionDiffers;

        in = gnome();
        in.builtQtVersion = u"6.9.2"_s;
        in.runningQtVersion = u"6.9.1"_s;
        QTest::newRow("older patch release, private API changed") << in << Decoration::QtVersionDiffers;

        in = gnome();
        in.runningQtVersion = u"6.11.4"_s;
        QTest::newRow("patch release not checked") << in << Decoration::QtVersionDiffers;

        in = gnome();
        in.builtQtVersion = u"6.12.1"_s;
        in.runningQtVersion = u"6.12.2"_s;
        QTest::newRow("built for a release not checked") << in << Decoration::QtVersionDiffers;

        in = gnome();
        in.runningQtVersion = u"6.12.0"_s;
        QTest::newRow("newer minor release") << in << Decoration::QtVersionDiffers;

        in = gnome();
        in.runningQtVersion = u"6.12.0"_s;
        in.decorationSet = true;
        QTest::newRow("version differs, variable set") << in << Decoration::QtPlugin;
    }

    void choice()
    {
        QFETCH(DecorationInputs, inputs);
        QFETCH(Decoration, expected);
        QCOMPARE(chooseDecoration(inputs), expected);
    }
};

QTEST_GUILESS_MAIN(TestWindowDecoration)
#include "tst_windowdecoration.moc"
