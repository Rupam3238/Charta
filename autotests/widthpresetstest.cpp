/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardPaths>
#include <QTest>

#include "../part/widthpresetutils.h"
#include "../settings.h"

class WidthPresetsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void testDefaultsAndSharedConsumers();
    void testValidation();
    void testSelectionAndEditing();
    void testPresetReplacement();
    void testPersistence();
    void testResizeMinimumAndAdjustmentStep();
};

void WidthPresetsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    Okular::Settings::instance(QStringLiteral("widthpresetstest"));
}

void WidthPresetsTest::init()
{
    Okular::Settings::self()->setDefaults();
}

void WidthPresetsTest::testDefaultsAndSharedConsumers()
{
    QCOMPARE(Okular::Settings::annotationWidthMenuPresets(), AnnotationWidthPresets::defaultMenuPresetStrings());
    QCOMPARE(Okular::Settings::toolSettingsWidthPresets(), AnnotationWidthPresets::defaultToolPresetStrings());
    QCOMPARE(AnnotationWidthPresets::presetValuesOrDefaults(Okular::Settings::annotationWidthMenuPresets(), AnnotationWidthPresets::defaultMenuPresetStrings()).size(), 12);
    QCOMPARE(AnnotationWidthPresets::presetValuesOrDefaults(Okular::Settings::toolSettingsWidthPresets(), AnnotationWidthPresets::defaultToolPresetStrings()), QList<double>({1.0, 5.0, 20.0, 50.0}));
    QCOMPARE(AnnotationWidthPresets::presetValuesOrDefaults({}, AnnotationWidthPresets::defaultMenuPresetStrings()), QList<double>({0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 20.0, 50.0}));
}

void WidthPresetsTest::testValidation()
{
    QList<double> parsed;
    QVERIFY(AnnotationWidthPresets::parsePresetStrings({QStringLiteral("5"), QStringLiteral("0.5"), QStringLiteral("1.5")}, &parsed));
    QCOMPARE(parsed, QList<double>({0.5, 1.5, 5.0}));

    for (const QStringList &invalid : {QStringList {}, QStringList {QStringLiteral("abc")}, QStringList {QStringLiteral("nan")}, QStringList {QStringLiteral("inf")},
                                       QStringList {QStringLiteral("0")}, QStringList {QStringLiteral("-1")}, QStringList {QStringLiteral("0.25")}, QStringList {QStringLiteral("64.5")},
                                       QStringList {QStringLiteral("1"), QStringLiteral("1.0")}}) {
        QVERIFY(!AnnotationWidthPresets::parsePresetStrings(invalid, &parsed));
    }
}

void WidthPresetsTest::testSelectionAndEditing()
{
    AnnotationWidthPresets::SelectionTracker tracker;
    QVERIFY(!tracker.activate(2));
    QVERIFY(tracker.activate(2));
    QVERIFY(!tracker.activate(3));
    QVERIFY(!tracker.activate(2));
    QVERIFY(tracker.activate(2));
}

void WidthPresetsTest::testPresetReplacement()
{
    const QStringList original {QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("20")};
    QStringList updated;
    QVERIFY(AnnotationWidthPresets::replacePresetString(original, 1, QStringLiteral("7.5"), &updated));
    QCOMPARE(updated, QStringList({QStringLiteral("1"), QStringLiteral("7.5"), QStringLiteral("20")}));
    QVERIFY(!AnnotationWidthPresets::replacePresetString(original, 1, QStringLiteral("nan"), &updated));
    QCOMPARE(original, QStringList({QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("20")}));
    QVERIFY(!AnnotationWidthPresets::replacePresetString(original, 1, QStringLiteral("20"), &updated));
}

void WidthPresetsTest::testPersistence()
{
    Okular::Settings::setAnnotationWidthMenuPresets({QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("12")});
    Okular::Settings::setToolSettingsWidthPresets({QStringLiteral("1.5"), QStringLiteral("8")});
    QVERIFY(Okular::Settings::self()->save());

    Okular::Settings::setAnnotationWidthMenuPresets({QStringLiteral("3")});
    Okular::Settings::setToolSettingsWidthPresets({QStringLiteral("4")});
    Okular::Settings::self()->load();

    QCOMPARE(Okular::Settings::annotationWidthMenuPresets(), QStringList({QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("12")}));
    QCOMPARE(Okular::Settings::toolSettingsWidthPresets(), QStringList({QStringLiteral("1.5"), QStringLiteral("8")}));
}

void WidthPresetsTest::testResizeMinimumAndAdjustmentStep()
{
    QCOMPARE(AnnotationWidthPresets::adjustmentStep, 0.5);
    QCOMPARE(AnnotationWidthPresets::minimumNormalizedResizeDimension(500.0) * 500.0, 0.5);
    QCOMPARE(AnnotationWidthPresets::minimumNormalizedResizeDimension(1000.0) * 1000.0, 0.5);
}

QTEST_GUILESS_MAIN(WidthPresetsTest)
#include "widthpresetstest.moc"
