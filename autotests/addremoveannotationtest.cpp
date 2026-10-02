/*
    SPDX-FileCopyrightText: 2013 Jon Mease <jon.mease@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QMimeDatabase>
#include <QDir>
#include <QImage>
#include <QTemporaryFile>
#include <QTest>

#ifdef HAVE_POPPLER_QT6
#include <poppler-annotation.h>
#include <poppler-qt6.h>
#endif

#include "../core/annotations.h"
#include "../core/document.h"
#include "../core/page.h"
#include "../settings_core.h"
#include "testingutils.h"

class AddRemoveAnnotationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testAddAnnotations();
    void testAddAnnotationUndoWithRotate_Bug318091();
    void testRemoveAnnotations();
    void testRemoveAnnotationGroupUndoOnce();
    void testInkCustomAppearanceSaveReload();

private:
    Okular::Document *m_document;
};

void AddRemoveAnnotationTest::initTestCase()
{
    Okular::SettingsCore::instance(QStringLiteral("addannotationtest"));
    m_document = new Okular::Document(nullptr);
}

void AddRemoveAnnotationTest::init()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/file1.pdf");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);
    QCOMPARE(m_document->openDocument(testFile, QUrl(), mime), Okular::Document::OpenSuccess);
}

void AddRemoveAnnotationTest::cleanup()
{
    m_document->closeDocument();
}

void AddRemoveAnnotationTest::testAddAnnotations()
{
    // Undo and Redo should be unavailable when docuemnt is first opened.
    QVERIFY(!m_document->canUndo());
    QVERIFY(!m_document->canRedo());

    // Create two distinct text annotations
    Okular::Annotation *annot1 = new Okular::TextAnnotation();
    annot1->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.15, 0.15));
    annot1->setContents(QStringLiteral("annot contents"));

    Okular::Annotation *annot2 = new Okular::TextAnnotation();
    annot2->setBoundingRectangle(Okular::NormalizedRect(0.2, 0.2, 0.3, 0.4));
    annot2->setContents(QStringLiteral("annot contents"));

    // The two annotations shold have different properties XML strings
    QVERIFY(TestingUtils::getAnnotationXml(annot1) != TestingUtils::getAnnotationXml(annot2));

    // We start with no annotations in the docuemnt
    QVERIFY(m_document->page(0)->annotations().size() == 0);

    // After adding annot1 we should have one annotation in the page and it should be annot1.
    m_document->addPageAnnotation(0, annot1);
    QVERIFY(m_document->page(0)->annotations().size() == 1);
    QCOMPARE(annot1, m_document->page(0)->annotations().constFirst());

    // Record the properties and name of annot1 just after insertion for later comparisons
    QString origLine1Xml = TestingUtils::getAnnotationXml(annot1);
    QString annot1Name = annot1->uniqueName();
    QVERIFY(!annot1Name.isEmpty());

    // Now undo the addition of annot1 and verify that annot1's properties haven't changed
    m_document->undo();
    QVERIFY(m_document->page(0)->annotations().empty());
    QVERIFY(!m_document->canUndo());
    QVERIFY(m_document->canRedo());
    QCOMPARE(TestingUtils::getAnnotationXml(annot1), origLine1Xml);

    // redo addition of annot1
    m_document->redo();
    QVERIFY(m_document->page(0)->annotations().size() == 1);
    QVERIFY(annot1 == m_document->page(0)->annotations().constFirst());
    QCOMPARE(TestingUtils::getAnnotationXml(annot1), origLine1Xml);

    // undo once more
    m_document->undo();
    QVERIFY(m_document->page(0)->annotations().empty());
    QVERIFY(!m_document->canUndo());
    QVERIFY(m_document->canRedo());
    QCOMPARE(TestingUtils::getAnnotationXml(annot1), origLine1Xml);

    // Set AnnotationDisposeWatcher dispose function on annot1 so we can detect
    // when it is deleted
    annot1->setDisposeDataFunction(TestingUtils::AnnotationDisposeWatcher::disposeAnnotation);
    TestingUtils::AnnotationDisposeWatcher::resetDisposedAnnotationName();
    QCOMPARE(TestingUtils::AnnotationDisposeWatcher::disposedAnnotationName(), QString());

    // now add annot2
    m_document->addPageAnnotation(0, annot2);
    QString annot2Name = annot2->uniqueName();
    QVERIFY(!annot2Name.isEmpty());
    QVERIFY(annot1Name != annot2Name);
    QVERIFY(m_document->page(0)->annotations().size() == 1);
    QCOMPARE(annot2, m_document->page(0)->annotations().constFirst());

    // Check that adding annot2 while annot1 was in the unadded state triggered the deletion of annot1
    QVERIFY(TestingUtils::AnnotationDisposeWatcher::disposedAnnotationName() == annot1Name);
}

void AddRemoveAnnotationTest::testAddAnnotationUndoWithRotate_Bug318091()
{
    Okular::Annotation *annot = new Okular::TextAnnotation();
    annot->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.15, 0.15));
    annot->setContents(QStringLiteral("annot contents"));

    m_document->addPageAnnotation(0, annot);
    QString origAnnotXml = TestingUtils::getAnnotationXml(annot);

    // Now undo annotation addition, rotate the page, and redo to annotation addition
    m_document->undo();
    m_document->setRotation(1);
    m_document->redo();

    // Verify that annotation's properties remain unchanged
    // In Bug318091 the bounding rectangle was being rotated upon each redo
    QString newAnnotXml = TestingUtils::getAnnotationXml(annot);
    QCOMPARE(origAnnotXml, newAnnotXml);
}

void AddRemoveAnnotationTest::testRemoveAnnotations()
{
    // Undo and Redo should be unavailable when docuemnt is first opened.
    QVERIFY(!m_document->canUndo());
    QVERIFY(!m_document->canRedo());

    // Create two distinct text annotations
    Okular::Annotation *annot1 = new Okular::TextAnnotation();
    annot1->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.15, 0.15));
    annot1->setContents(QStringLiteral("annot contents"));

    Okular::Annotation *annot2 = new Okular::TextAnnotation();
    annot2->setBoundingRectangle(Okular::NormalizedRect(0.2, 0.2, 0.3, 0.4));
    annot2->setContents(QStringLiteral("annot contents"));

    // Add annot1 and annot2 to document
    m_document->addPageAnnotation(0, annot1);
    m_document->addPageAnnotation(0, annot2);
    QVERIFY(m_document->page(0)->annotations().size() == 2);
    QVERIFY(m_document->page(0)->annotations().contains(annot1));
    QVERIFY(m_document->page(0)->annotations().contains(annot2));

    // Now remove annot1
    m_document->removePageAnnotation(0, annot1);
    QVERIFY(m_document->page(0)->annotations().size() == 1);
    QVERIFY(m_document->page(0)->annotations().contains(annot2));

    // Undo removal of annot1
    m_document->undo();
    QVERIFY(m_document->page(0)->annotations().size() == 2);
    QVERIFY(m_document->page(0)->annotations().contains(annot1));
    QVERIFY(m_document->page(0)->annotations().contains(annot2));

    // Redo removal
    m_document->redo();
    QVERIFY(m_document->page(0)->annotations().size() == 1);
    QVERIFY(m_document->page(0)->annotations().contains(annot2));

    // Verify that annot1 is disposed of if document is closed with annot1 in removed state
    QString annot1Name = annot1->uniqueName();
    annot1->setDisposeDataFunction(TestingUtils::AnnotationDisposeWatcher::disposeAnnotation);
    TestingUtils::AnnotationDisposeWatcher::resetDisposedAnnotationName();
    QVERIFY(TestingUtils::AnnotationDisposeWatcher::disposedAnnotationName().isEmpty());
    m_document->closeDocument();
    QVERIFY(TestingUtils::AnnotationDisposeWatcher::disposedAnnotationName() == annot1Name);
}

void AddRemoveAnnotationTest::testRemoveAnnotationGroupUndoOnce()
{
    QList<Okular::Annotation *> annotations;
    for (int index = 0; index < 3; ++index) {
        auto *annotation = new Okular::TextAnnotation;
        annotation->setBoundingRectangle(Okular::NormalizedRect(0.1 + index * 0.1, 0.1, 0.2 + index * 0.1, 0.2));
        annotation->setContents(QString::number(index));
        m_document->addPageAnnotation(0, annotation);
        annotations.append(annotation);
    }
    QCOMPARE(m_document->page(0)->annotations().size(), 3);

    m_document->beginUndoMacro(QStringLiteral("erase gesture"));
    m_document->removePageAnnotations(0, annotations);
    m_document->endUndoMacro();
    QVERIFY(m_document->page(0)->annotations().isEmpty());

    m_document->undo();
    QCOMPARE(m_document->page(0)->annotations().size(), 3);
    for (Okular::Annotation *annotation : annotations) {
        QVERIFY(m_document->page(0)->annotations().contains(annotation));
    }

    m_document->undo();
    QCOMPARE(m_document->page(0)->annotations().size(), 2);
}

void AddRemoveAnnotationTest::testInkCustomAppearanceSaveReload()
{
#if !defined(HAVE_POPPLER_QT6) || !defined(POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE)
    QSKIP("This test requires Poppler's custom Ink appearance API");
#else
    const QByteArray appearance = QByteArrayLiteral("q\n1 0 1 rg\n10 10 m\n90 10 l\n50 90 l\nh\nf\nQ\n");
    auto countMagentaPixels = [](const QImage &image) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                if (color.red() > 220 && color.blue() > 220 && color.green() < 80 && color.alpha() > 200) {
                    ++count;
                }
            }
        }
        return count;
    };

    const QString sourceFile = QStringLiteral(KDESRCDIR "data/file1.pdf");
    auto liveDocument = Poppler::Document::load(sourceFile);
    QVERIFY(liveDocument);
    auto livePage = liveDocument->page(0);
    QVERIFY(livePage);

    Poppler::InkAnnotation liveInk;
    liveInk.setBoundary(QRectF(0.2, 0.2, 0.2, 0.2));
    liveInk.setInkPaths({QVector<QPointF>{{0.2, 0.2}, {0.4, 0.4}}});
    liveInk.setAppearanceStream(appearance, QRectF(0, 0, 100, 100));
    livePage->addAnnotation(&liveInk);
    QVERIFY(countMagentaPixels(livePage->renderToImage(72, 72)) > 100);

    auto *ink = new Okular::InkAnnotation;
    ink->setInkPaths({{Okular::NormalizedPoint(0.2, 0.2), Okular::NormalizedPoint(0.4, 0.4)}});
    ink->setBoundingRectangle(Okular::NormalizedRect(0.2, 0.2, 0.4, 0.4));
    ink->style().setColor(Qt::black);
    ink->style().setWidth(2.0);
    ink->setAppearanceStream(appearance, QRectF(0, 0, 100, 100));
    m_document->addPageAnnotation(0, ink);
    QCOMPARE(ink->subType(), Okular::Annotation::AInk);

    QTemporaryFile savedPdf(QDir::tempPath() + QStringLiteral("/charta-ink-appearance-XXXXXX.pdf"));
    QVERIFY(savedPdf.open());
    savedPdf.close();
    QString errorText;
    QVERIFY2(m_document->saveChanges(savedPdf.fileName(), &errorText), qPrintable(errorText));

    auto savedDocument = Poppler::Document::load(savedPdf.fileName());
    QVERIFY(savedDocument);
    auto savedPage = savedDocument->page(0);
    QVERIFY(savedPage);
    const auto savedAnnotations = savedPage->annotations();
    Poppler::InkAnnotation *savedInk = nullptr;
    for (const auto &annotation : savedAnnotations) {
        if (annotation->subType() == Poppler::Annotation::AInk) {
            savedInk = static_cast<Poppler::InkAnnotation *>(annotation.get());
            break;
        }
    }
    QVERIFY(savedInk);
    QCOMPARE(savedInk->inkPaths().size(), 1);
    QCOMPARE(savedInk->inkPaths().constFirst().size(), 2);
    QVERIFY(countMagentaPixels(savedPage->renderToImage(72, 72)) > 100);

    auto reopenedOkularDocument = std::make_unique<Okular::Document>(nullptr);
    QMimeDatabase mimeDatabase;
    QCOMPARE(reopenedOkularDocument->openDocument(savedPdf.fileName(), QUrl(), mimeDatabase.mimeTypeForFile(savedPdf.fileName())), Okular::Document::OpenSuccess);
    Okular::Annotation *reopenedInk = nullptr;
    for (Okular::Annotation *annotation : reopenedOkularDocument->page(0)->annotations()) {
        if (annotation->subType() == Okular::Annotation::AInk) {
            reopenedInk = annotation;
            break;
        }
    }
    QVERIFY(reopenedInk);
    QVERIFY(reopenedOkularDocument->canRemovePageAnnotation(reopenedInk));
    const auto originalPaths = static_cast<Okular::InkAnnotation *>(reopenedInk)->inkPaths();
    reopenedOkularDocument->translatePageAnnotation(0, reopenedInk, Okular::NormalizedPoint(0.01, 0.01));
    const auto movedPaths = static_cast<Okular::InkAnnotation *>(reopenedInk)->inkPaths();
    QVERIFY(movedPaths.first().first().x != originalPaths.first().first().x);
    QVERIFY(movedPaths.first().first().y != originalPaths.first().first().y);
    reopenedOkularDocument->undo();
    const auto restoredPaths = static_cast<Okular::InkAnnotation *>(reopenedInk)->inkPaths();
    QCOMPARE(restoredPaths.first().first().x, originalPaths.first().first().x);
    QCOMPARE(restoredPaths.first().first().y, originalPaths.first().first().y);
    reopenedOkularDocument->removePageAnnotation(0, reopenedInk);
    QVERIFY(reopenedOkularDocument->page(0)->annotations().isEmpty());
    reopenedOkularDocument->undo();
    QCOMPARE(reopenedOkularDocument->page(0)->annotations().size(), 1);
#endif
}

QTEST_MAIN(AddRemoveAnnotationTest)
#include "addremoveannotationtest.moc"
