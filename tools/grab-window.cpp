// SPDX-License-Identifier: GPL-3.0-or-later
//
// Saves the active window, with its title bar and shadow, through KWin's ScreenShot2 interface.
// Used by take-screenshots.sh, which builds it.
//
//   grab-window <out.png>
//
// KWin only answers apps it trusts unless it runs with KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1.

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QGuiApplication>
#include <QImage>

#include <cstdio>
#include <unistd.h>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <out.png>\n", argv[0]);
        return 2;
    }

    int fds[2];
    if (pipe(fds) != 0)
        return 1;
    QDBusInterface kwin(u"org.kde.KWin.ScreenShot2"_s, u"/org/kde/KWin/ScreenShot2"_s, u"org.kde.KWin.ScreenShot2"_s);
    const QVariantMap options{
        {u"include-decoration"_s, true},
        {u"include-shadow"_s, true},
        {u"include-cursor"_s, false},
        {u"native-resolution"_s, true},
    };
    const QDBusReply<QVariantMap> reply = kwin.call(u"CaptureActiveWindow"_s, options, QVariant::fromValue(QDBusUnixFileDescriptor(fds[1])));
    close(fds[1]);
    if (!reply.isValid()) {
        std::fprintf(stderr, "%s\n", qPrintable(reply.error().message()));
        return 1;
    }

    QFile pipeIn;
    pipeIn.open(fds[0], QIODevice::ReadOnly, QFile::AutoCloseHandle);
    const QByteArray data = pipeIn.readAll();
    const QVariantMap m = reply.value();
    const QImage image(reinterpret_cast<const uchar *>(data.constData()),
                       m[u"width"_s].toInt(),
                       m[u"height"_s].toInt(),
                       m[u"stride"_s].toInt(),
                       QImage::Format(m[u"format"_s].toInt()));
    return image.save(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}
