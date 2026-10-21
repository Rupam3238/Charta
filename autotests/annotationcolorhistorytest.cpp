/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#include <QStandardPaths>
#include <QTest>

#include "../part/annotationcolorhistory.h"
#include "../settings.h"

using namespace Okular::AnnotationColorHistory;

class AnnotationColorHistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void testEmptyHistory();
    void testSingleColor();
    void testMultipleColorsOrdering();
    void testDeduplicationAndMoveToFront();
    void testCanonicalColorComparison();
    void testMaxEightColorsCap();
    void testInvalidColorIgnored();
    void testCyclingSemantics();
    void testCyclingPreservesHistory();
    void testPersistence();
};

void AnnotationColorHistoryTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    Okular::Settings::instance(QStringLiteral("annotationcolorhistorytest"));
}

void AnnotationColorHistoryTest::init()
{
    Okular::Settings::self()->setDefaults();
    Okular::Settings::setRecentAnnotationColors({});
    Okular::Settings::self()->save();
}

void AnnotationColorHistoryTest::testEmptyHistory()
{
    QVERIFY(recentColors().isEmpty());
    const QColor current(Qt::red);
    QCOMPARE(nextCycledColor({}, current), current);
}

void AnnotationColorHistoryTest::testSingleColor()
{
    addRecentColor(QColor(Qt::red));
    const QList<QColor> colors = recentColors();
    QCOMPARE(colors.size(), 1);
    QVERIFY(colorsEqual(colors.first(), QColor(Qt::red)));

    // Cycling with only 1 color does nothing
    QCOMPARE(nextCycledColor(colors, QColor(Qt::red)), QColor(Qt::red));
}

void AnnotationColorHistoryTest::testMultipleColorsOrdering()
{
    addRecentColor(QColor(Qt::red));
    addRecentColor(QColor(Qt::green));
    addRecentColor(QColor(Qt::blue));

    const QList<QColor> colors = recentColors();
    QCOMPARE(colors.size(), 3);
    // Newest first: Blue, Green, Red
    QVERIFY(colorsEqual(colors[0], QColor(Qt::blue)));
    QVERIFY(colorsEqual(colors[1], QColor(Qt::green)));
    QVERIFY(colorsEqual(colors[2], QColor(Qt::red)));
}

void AnnotationColorHistoryTest::testDeduplicationAndMoveToFront()
{
    addRecentColor(QColor(Qt::red));
    addRecentColor(QColor(Qt::green));
    addRecentColor(QColor(Qt::blue));

    // Re-use green: should move to front without duplicate
    addRecentColor(QColor(Qt::green));

    const QList<QColor> colors = recentColors();
    QCOMPARE(colors.size(), 3);
    QVERIFY(colorsEqual(colors[0], QColor(Qt::green)));
    QVERIFY(colorsEqual(colors[1], QColor(Qt::blue)));
    QVERIFY(colorsEqual(colors[2], QColor(Qt::red)));
}

void AnnotationColorHistoryTest::testCanonicalColorComparison()
{
    const QColor hexRgb(QStringLiteral("#ff0000"));
    const QColor hexArgb(QStringLiteral("#ffff0000"));
    const QColor named(Qt::red);

    QVERIFY(colorsEqual(hexRgb, hexArgb));
    QVERIFY(colorsEqual(hexRgb, named));

    addRecentColor(hexRgb);
    addRecentColor(hexArgb);

    // Should only have 1 color in history
    const QList<QColor> colors = recentColors();
    QCOMPARE(colors.size(), 1);
    QVERIFY(colorsEqual(colors.first(), named));
}

void AnnotationColorHistoryTest::testMaxEightColorsCap()
{
    const QList<QColor> testColors = {
        QColor(10, 0, 0),
        QColor(20, 0, 0),
        QColor(30, 0, 0),
        QColor(40, 0, 0),
        QColor(50, 0, 0),
        QColor(60, 0, 0),
        QColor(70, 0, 0),
        QColor(80, 0, 0),
        QColor(90, 0, 0),
        QColor(100, 0, 0),
    };

    for (const QColor &c : testColors) {
        addRecentColor(c);
    }

    const QList<QColor> colors = recentColors();
    QCOMPARE(colors.size(), 8);
    // Newest is 100, oldest kept is 30. 10 and 20 were discarded.
    QVERIFY(colorsEqual(colors.first(), QColor(100, 0, 0)));
    QVERIFY(colorsEqual(colors.last(), QColor(30, 0, 0)));
}

void AnnotationColorHistoryTest::testInvalidColorIgnored()
{
    addRecentColor(QColor(Qt::red));
    addRecentColor(QColor()); // invalid

    const QList<QColor> colors = recentColors();
    QCOMPARE(colors.size(), 1);
    QVERIFY(colorsEqual(colors.first(), QColor(Qt::red)));
}

void AnnotationColorHistoryTest::testCyclingSemantics()
{
    // History: [Red, Blue, Green] (Red newest, Green oldest)
    addRecentColor(QColor(Qt::green));
    addRecentColor(QColor(Qt::blue));
    addRecentColor(QColor(Qt::red));

    const QList<QColor> history = recentColors();
    QCOMPARE(history.size(), 3);
    QVERIFY(colorsEqual(history[0], QColor(Qt::red)));
    QVERIFY(colorsEqual(history[1], QColor(Qt::blue)));
    QVERIFY(colorsEqual(history[2], QColor(Qt::green)));

    // Active color is Red -> next older is Blue
    QColor next = nextCycledColor(history, QColor(Qt::red));
    QVERIFY(colorsEqual(next, QColor(Qt::blue)));

    // Active color is Blue -> next older is Green
    next = nextCycledColor(history, QColor(Qt::blue));
    QVERIFY(colorsEqual(next, QColor(Qt::green)));

    // Active color is Green (oldest) -> wraps around to Red (newest)
    next = nextCycledColor(history, QColor(Qt::green));
    QVERIFY(colorsEqual(next, QColor(Qt::red)));

    // Active color is Purple (absent from history) -> selects Red (newest)
    next = nextCycledColor(history, QColor(Qt::magenta));
    QVERIFY(colorsEqual(next, QColor(Qt::red)));

    // Active color is invalid -> selects Red (newest)
    next = nextCycledColor(history, QColor());
    QVERIFY(colorsEqual(next, QColor(Qt::red)));
}

void AnnotationColorHistoryTest::testCyclingPreservesHistory()
{
    addRecentColor(QColor(Qt::green));
    addRecentColor(QColor(Qt::blue));
    addRecentColor(QColor(Qt::red));

    const QList<QColor> before = recentColors();
    Q_UNUSED(nextCycledColor(before, QColor(Qt::red)));
    Q_UNUSED(nextCycledColor(before, QColor(Qt::blue)));
    Q_UNUSED(nextCycledColor(before, QColor(Qt::green)));
    Q_UNUSED(nextCycledColor(before, QColor(Qt::yellow)));

    const QList<QColor> after = recentColors();
    QCOMPARE(before.size(), after.size());
    for (int i = 0; i < before.size(); ++i) {
        QVERIFY(colorsEqual(before[i], after[i]));
    }
}

void AnnotationColorHistoryTest::testPersistence()
{
    addRecentColor(QColor(255, 0, 0, 200));
    const QStringList stored = Okular::Settings::recentAnnotationColors();
    QCOMPARE(stored.size(), 1);
    QCOMPARE(stored.first(), QColor(255, 0, 0, 200).name(QColor::HexArgb));

    // Reload
    const QList<QColor> reloaded = recentColors();
    QCOMPARE(reloaded.size(), 1);
    QCOMPARE(reloaded.first().alpha(), 200);
    QVERIFY(colorsEqual(reloaded.first(), QColor(255, 0, 0, 200)));
}

QTEST_MAIN(AnnotationColorHistoryTest)
#include "annotationcolorhistorytest.moc"
