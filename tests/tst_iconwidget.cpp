// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/CheckRow.h"
#include "app/IconWidget.h"
#include "app/Icons.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTest>
#include <QWindow>

using namespace Qt::StringLiterals;

// Runs on two offscreen screens (data/two-screens.json) with different scales.
class TestIconWidget : public QObject
{
    Q_OBJECT

public:
    static void initMain() { qputenv("QT_SCREEN_SCALE_FACTORS", "1;2"); }

private:
    // What the widget should show: the icon at this scale, one icon pixel to one screen pixel.
    static QImage sharp(const QWidget &widget, const QIcon &icon, qreal scale)
    {
        QImage image(widget.size() * scale, QImage::Format_ARGB32_Premultiplied);
        image.fill(widget.palette().color(QPalette::Window));
        QPixmap pixmap = icon.pixmap(widget.size(), scale);
        pixmap.setDevicePixelRatio(1);
        QPainter(&image).drawPixmap(0, 0, pixmap);
        image.setDevicePixelRatio(scale);
        return image;
    }

    static QImage grabbed(QWidget &widget)
    {
        return widget.grab().toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

private Q_SLOTS:
    void followsScaleChange()
    {
        const QList<QScreen *> screens = QGuiApplication::screens();
        QCOMPARE(screens.size(), 2);

        CheckResult result;
        result.status = CheckStatus::Pass;
        CheckRow row(u"Title"_s);
        row.setResult(result);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        QCOMPARE(row.devicePixelRatio(), 1.0);

        auto *widget = row.findChild<IconWidget *>();
        QVERIFY(widget);
        const QIcon icon = statusIcon(CheckStatus::Pass);
        QCOMPARE(grabbed(*widget), sharp(*widget, icon, 1.0));

        row.windowHandle()->setScreen(screens[1]);
        QTRY_COMPARE(row.devicePixelRatio(), 2.0);
        QCOMPARE(grabbed(*widget), sharp(*widget, icon, 2.0));
    }
};

QTEST_MAIN(TestIconWidget)
#include "tst_iconwidget.moc"
