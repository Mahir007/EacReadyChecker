// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/Motion.h"

#include "FakePortal.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{
const QString Appearance = u"org.freedesktop.appearance"_s;
const QString ReducedMotion = u"reduced-motion"_s;
const QString Gnome = u"org.gnome.desktop.interface"_s;
const QString EnableAnimations = u"enable-animations"_s;
const QString Kde = u"org.kde.kdeglobals.KDE"_s;
const QString AnimationDurationFactor = u"AnimationDurationFactor"_s;
} // namespace

class TestMotion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        const QString problem = m_portal.start();
        if (!problem.isEmpty()) {
            QSKIP(qPrintable(problem));
        }
    }

    void cleanupTestCase() { m_portal.stop(); }

    void followsTheStyleWithoutThePortal()
    {
        // Fusion has no setting of its own. Breeze reports KDE's.
        QApplication::setStyle(u"Fusion"_s);
        QCOMPARE(Motion::factor(), 1.0);
        QCOMPARE(Motion::duration(Motion::Long), Motion::Long);
        if (QStyleFactory::keys().contains(u"Breeze"_s, Qt::CaseInsensitive)) {
            QApplication::setStyle(u"Breeze"_s);
            QCOMPARE(Motion::factor(), QApplication::style()->styleHint(QStyle::SH_Widget_Animation_Duration) / 100.0);
            QApplication::setStyle(u"Fusion"_s);
        }
    }

    void followsThePortal()
    {
        // KDE's portal sends the factor as text.
        m_portal.set(Kde, AnimationDurationFactor, u"0.5"_s);
        Motion::followDesktop();
        QTRY_COMPARE(Motion::factor(), 0.5);
        QCOMPARE(Motion::duration(Motion::Long), Motion::Long / 2);

        // GNOME's switch and reduced motion turn them off, whatever the speed.
        QVERIFY(m_portal.change(Gnome, EnableAnimations, false));
        QTRY_COMPARE(Motion::factor(), 0.0);
        QVERIFY(m_portal.change(Gnome, EnableAnimations, true));
        QTRY_COMPARE(Motion::factor(), 0.5);
        QVERIFY(m_portal.change(Appearance, ReducedMotion, QVariant::fromValue(1U)));
        QTRY_COMPARE(Motion::factor(), 0.0);
        QCOMPARE(Motion::duration(Motion::Long), 0);
        QVERIFY(m_portal.change(Appearance, ReducedMotion, QVariant::fromValue(0U)));
        QTRY_COMPARE(Motion::factor(), 0.5);

        // KDE's slider at "Instant".
        QVERIFY(m_portal.change(Kde, AnimationDurationFactor, u"0"_s));
        QTRY_COMPARE(Motion::factor(), 0.0);

        // An override wins until it's taken away.
        Motion::overrideFactor(1);
        QCOMPARE(Motion::factor(), 1.0);
        Motion::overrideFactor(std::nullopt);
        QCOMPARE(Motion::factor(), 0.0);
    }

private:
    FakePortalService m_portal;
};

QTEST_MAIN(TestMotion)
#include "tst_motion.moc"
