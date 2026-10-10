/*
    SPDX-FileCopyrightText: 2013 Jon Mease <jon.mease@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QMimeDatabase>
#include <QDir>
#include <QImage>
#include <QTemporaryFile>
#include <QUuid>
#include <QTest>

#ifdef HAVE_POPPLER_QT6
#include <poppler-annotation.h>
#include <poppler-qt6.h>
#endif

#include "../core/annotations.h"
#include "../core/document.h"
#include "../core/page.h"
#include "../part/annotationtools.h"
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
    void testSingleClickInkAnnotation();
    void testClipboardImageStampSaveReloadRemainsEditable();
    void testInkCustomAppearanceSaveReload();
    void testInkCustomAppearanceRotatedPage();

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

void AddRemoveAnnotationTest::testSingleClickInkAnnotation()
{
    QDomDocument engineDocument;
    QDomElement toolElement = engineDocument.createElement(QStringLiteral("tool"));
    QDomElement annotationElement = engineDocument.createElement(QStringLiteral("annotation"));
    annotationElement.setAttribute(QStringLiteral("type"), QStringLiteral("Ink"));
    annotationElement.setAttribute(QStringLiteral("width"), QStringLiteral("20"));
    annotationElement.setAttribute(QStringLiteral("color"), QStringLiteral("#ff00ff"));
    toolElement.appendChild(annotationElement);
    engineDocument.appendChild(toolElement);

    SmoothPathEngine engine(toolElement);
    const auto modifiers = AnnotatorEngine::Modifiers {};
    const Okular::Page *page = m_document->page(0);
    const qreal x = 0.5;
    const qreal y = 0.5;
    engine.event(AnnotatorEngine::Press, AnnotatorEngine::Left, modifiers, x, y, page->width(), page->height(), page);
    engine.event(AnnotatorEngine::Release, AnnotatorEngine::Left, modifiers, x, y, page->width(), page->height(), page);

    QVERIFY(engine.creationCompleted());
    const QList<Okular::Annotation *> annotations = engine.end();
    QCOMPARE(annotations.size(), 1);
    auto *ink = dynamic_cast<Okular::InkAnnotation *>(annotations.constFirst());
    QVERIFY(ink);
    QCOMPARE(ink->style().width(), qreal(20.0));
    QCOMPARE(ink->inkPaths().size(), 1);
    QCOMPARE(ink->inkPaths().constFirst().size(), 2);
    QVERIFY(!ink->appearanceStream().isEmpty());
    QVERIFY(ink->boundingRectangle().width() > 0.0);
    QVERIFY(ink->boundingRectangle().height() > 0.0);

    m_document->addPageAnnotation(0, ink);
    QCOMPARE(m_document->page(0)->annotations().size(), 1);
    QCOMPARE(m_document->page(0)->annotations().constFirst(), ink);
}

void AddRemoveAnnotationTest::testClipboardImageStampSaveReloadRemainsEditable()
{
#ifndef HAVE_POPPLER_QT6
    QSKIP("This test requires Poppler Qt6 rendering");
#else
    auto cyanPixelBounds = [](const QImage &image) {
        QRect bounds;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                if (color.red() < 80 && color.green() > 160 && color.blue() > 180 && color.alpha() > 200) {
                    bounds |= QRect(x, y, 1, 1);
                }
            }
        }
        return bounds;
    };

    QTemporaryFile tempImage(QDir::tempPath() + QStringLiteral("/charta-clipboard-XXXXXX.png"));
    QVERIFY(tempImage.open());
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.fillRect(10, 10, 44, 44, QColor(0, 220, 255));
    painter.end();
    QVERIFY(image.save(&tempImage, "PNG"));
    tempImage.close();

    auto *stamp = new Okular::StampAnnotation();
    stamp->setUniqueName(QStringLiteral("charta-clipboard-image-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    stamp->setStampIconName(tempImage.fileName());
    stamp->setBoundingRectangle(Okular::NormalizedRect(0.20, 0.20, 0.55, 0.55));
    m_document->addPageAnnotation(0, stamp);

    QTemporaryFile savedPdf(QDir::tempPath() + QStringLiteral("/charta-clipboard-stamp-XXXXXX.pdf"));
    QVERIFY(savedPdf.open());
    savedPdf.close();

    QString errorText;
    QVERIFY2(m_document->saveChanges(savedPdf.fileName(), &errorText), qPrintable(errorText));

    auto renderedSavedDocument = Poppler::Document::load(savedPdf.fileName());
    QVERIFY(renderedSavedDocument);
    auto renderedSavedPage = renderedSavedDocument->page(0);
    QVERIFY(renderedSavedPage);
    const QImage beforeMoveImage = renderedSavedPage->renderToImage(72, 72);
    const QRect beforeMoveBounds = cyanPixelBounds(beforeMoveImage);
    QVERIFY(beforeMoveBounds.width() > 10);
    QVERIFY(beforeMoveBounds.height() > 10);
    Poppler::StampAnnotation *savedPopplerStamp = nullptr;
    const auto savedAnnotations = renderedSavedPage->annotations();
    for (const auto &annotation : savedAnnotations) {
        if (annotation->subType() == Poppler::Annotation::AStamp && annotation->uniqueName().startsWith(QStringLiteral("charta-clipboard-image-"))) {
            savedPopplerStamp = static_cast<Poppler::StampAnnotation *>(annotation.get());
            break;
        }
    }
    QVERIFY(savedPopplerStamp);
    QCOMPARE(savedPopplerStamp->stampIconName(), QStringLiteral("Draft"));
    QVERIFY(savedPopplerStamp->annotationAppearance());

    auto reopenedDocument = std::make_unique<Okular::Document>(nullptr);
    QMimeDatabase mimeDatabase;
    QCOMPARE(reopenedDocument->openDocument(savedPdf.fileName(), QUrl(), mimeDatabase.mimeTypeForFile(savedPdf.fileName())), Okular::Document::OpenSuccess);

    Okular::Annotation *reopenedStamp = nullptr;
    for (Okular::Annotation *annotation : reopenedDocument->page(0)->annotations()) {
        if (annotation->subType() == Okular::Annotation::AStamp) {
            reopenedStamp = annotation;
            break;
        }
    }

    QVERIFY(reopenedStamp);
    auto *reopenedStampImage = dynamic_cast<Okular::StampAnnotation *>(reopenedStamp);
    QVERIFY(reopenedStampImage);
    QVERIFY(reopenedStamp->uniqueName().startsWith(QStringLiteral("charta-clipboard-image-")));
    QCOMPARE(reopenedStampImage->stampIconName(), QStringLiteral("Draft"));
    QVERIFY(!(reopenedStamp->flags() & Okular::Annotation::DenyWrite));
    const Okular::NormalizedRect originalRect = reopenedStamp->boundingRectangle();
    reopenedDocument->translatePageAnnotation(0, reopenedStamp, Okular::NormalizedPoint(0.05, 0.05));
    const Okular::NormalizedRect movedRect = reopenedStamp->boundingRectangle();
    QVERIFY(movedRect.left != originalRect.left || movedRect.top != originalRect.top);

    QTemporaryFile movedPdf(QDir::tempPath() + QStringLiteral("/charta-clipboard-stamp-moved-XXXXXX.pdf"));
    QVERIFY(movedPdf.open());
    movedPdf.close();
    errorText.clear();
    QVERIFY2(reopenedDocument->saveChanges(movedPdf.fileName(), &errorText), qPrintable(errorText));

    auto renderedMovedDocument = Poppler::Document::load(movedPdf.fileName());
    QVERIFY(renderedMovedDocument);
    auto renderedMovedPage = renderedMovedDocument->page(0);
    QVERIFY(renderedMovedPage);
    const QImage afterMoveImage = renderedMovedPage->renderToImage(72, 72);
    const QRect afterMoveBounds = cyanPixelBounds(afterMoveImage);
    QVERIFY(afterMoveBounds.width() > 10);
    QVERIFY(afterMoveBounds.height() > 10);
    QVERIFY(afterMoveBounds.left() > beforeMoveBounds.left());
    QVERIFY(afterMoveBounds.top() > beforeMoveBounds.top());
    QVERIFY(qAbs(afterMoveBounds.width() - beforeMoveBounds.width()) <= 2);
    QVERIFY(qAbs(afterMoveBounds.height() - beforeMoveBounds.height()) <= 2);

    auto reopenedMovedDocument = std::make_unique<Okular::Document>(nullptr);
    QCOMPARE(reopenedMovedDocument->openDocument(movedPdf.fileName(), QUrl(), mimeDatabase.mimeTypeForFile(movedPdf.fileName())), Okular::Document::OpenSuccess);
    Okular::Annotation *reopenedMovedStamp = nullptr;
    for (Okular::Annotation *annotation : reopenedMovedDocument->page(0)->annotations()) {
        if (annotation->subType() == Okular::Annotation::AStamp && annotation->uniqueName().startsWith(QStringLiteral("charta-clipboard-image-"))) {
            reopenedMovedStamp = annotation;
            break;
        }
    }
    QVERIFY(reopenedMovedStamp);
    QVERIFY(!(reopenedMovedStamp->flags() & Okular::Annotation::DenyWrite));
    QCOMPARE(cyanPixelBounds(renderedMovedPage->renderToImage(72, 72)), afterMoveBounds);
#endif
}

void AddRemoveAnnotationTest::testInkCustomAppearanceSaveReload()
{
#if !defined(HAVE_POPPLER_QT6) || !defined(POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE)
    QSKIP("This test requires Poppler's custom Ink appearance API");
#else
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
    auto magentaPixelBounds = [](const QImage &image) {
        QRect bounds;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                if (color.red() > 220 && color.blue() > 220 && color.green() < 80 && color.alpha() > 200) {
                    bounds |= QRect(x, y, 1, 1);
                }
            }
        }
        return bounds;
    };

    const QString sourceFile = QStringLiteral(KDESRCDIR "data/file1.pdf");
    auto liveDocument = Poppler::Document::load(sourceFile);
    QVERIFY(liveDocument);
    auto livePage = liveDocument->page(0);
    QVERIFY(livePage);

    const QList<QPointF> samples {QPointF(0.2, 0.60), QPointF(0.25, 0.50), QPointF(0.31, 0.60), QPointF(0.37, 0.50), QPointF(0.43, 0.60)};
    QDomDocument engineDocument;
    QDomElement toolElement = engineDocument.createElement(QStringLiteral("tool"));
    toolElement.setAttribute(QStringLiteral("color"), QStringLiteral("#ff00ff"));
    QDomElement annotationElement = engineDocument.createElement(QStringLiteral("annotation"));
    annotationElement.setAttribute(QStringLiteral("type"), QStringLiteral("Ink"));
    annotationElement.setAttribute(QStringLiteral("width"), QStringLiteral("8"));
    annotationElement.setAttribute(QStringLiteral("color"), QStringLiteral("#ff00ff"));
    toolElement.appendChild(annotationElement);
    engineDocument.appendChild(toolElement);

    SmoothPathEngine engine(toolElement);
    const auto modifiers = AnnotatorEngine::Modifiers {};
    const Okular::Page *page = m_document->page(0);
    const qreal pageWidth = page->width();
    const qreal pageHeight = page->height();
    engine.event(AnnotatorEngine::Press, AnnotatorEngine::Left, modifiers, samples.first().x(), samples.first().y(), pageWidth, pageHeight, page);
    for (int index = 1; index < samples.size(); ++index) {
        engine.event(AnnotatorEngine::Move, AnnotatorEngine::Left, modifiers, samples[index].x(), samples[index].y(), pageWidth, pageHeight, page);
    }
    engine.event(AnnotatorEngine::Release, AnnotatorEngine::Left, modifiers, samples.last().x(), samples.last().y(), pageWidth, pageHeight, page);
    QVERIFY(engine.creationCompleted());
    QImage liveStrokeImage(qRound(pageWidth), qRound(pageHeight), QImage::Format_ARGB32_Premultiplied);
    liveStrokeImage.fill(Qt::transparent);
    QPainter liveStrokePainter(&liveStrokeImage);
    engine.paint(&liveStrokePainter, pageWidth, pageHeight, liveStrokeImage.rect());
    liveStrokePainter.end();
    const QRect liveStrokeBounds = magentaPixelBounds(liveStrokeImage);
    QVERIFY(!liveStrokeBounds.isEmpty());

    const QList<Okular::Annotation *> createdAnnotations = engine.end();
    QCOMPARE(createdAnnotations.size(), 1);
    auto *ink = dynamic_cast<Okular::InkAnnotation *>(createdAnnotations.first());
    QVERIFY(ink);
    QVERIFY(!ink->appearanceStream().isEmpty());

    const QList<QList<Okular::NormalizedPoint>> inkPaths = ink->inkPaths();
    QCOMPARE(inkPaths.size(), 1);
    QCOMPARE(inkPaths.constFirst().size(), samples.size());
    const QList<Okular::NormalizedPoint> &processedPoints = inkPaths.constFirst();
    qreal sumX = 0.0;
    qreal sumY = 0.0;
    for (qsizetype index = 0; index < samples.size(); ++index) {
        sumX += samples[index].x();
        sumY += samples[index].y();
        QCOMPARE(processedPoints[index].x, sumX / (index + 1));
        QCOMPARE(processedPoints[index].y, sumY / (index + 1));
    }

    QList<QPainterPath> expectedOutlines;
    QRectF expectedBounds;
    for (qsizetype index = 1; index < processedPoints.size(); ++index) {
        const QPointF start(processedPoints[index - 1].x * pageWidth, (1.0 - processedPoints[index - 1].y) * pageHeight);
        const QPointF end(processedPoints[index].x * pageWidth, (1.0 - processedPoints[index].y) * pageHeight);
        const QPainterPath outline = FreehandStrokeGeometry::segmentOutline(start, end, 8.0);
        expectedOutlines.append(outline);
        expectedBounds = expectedBounds.isNull() ? outline.boundingRect() : expectedBounds.united(outline.boundingRect());
    }
    expectedBounds.adjust(-0.25, -0.25, 0.25, 0.25);
    QList<QPainterPath> localOutlines;
    localOutlines.reserve(expectedOutlines.size());
    for (const QPainterPath &outline : expectedOutlines) {
        localOutlines.append(outline.translated(-expectedBounds.left(), -expectedBounds.top()));
    }
    QCOMPARE(ink->appearanceStream(), FreehandStrokeGeometry::pdfAppearance(localOutlines, QColor(Qt::magenta)));
    QCOMPARE(ink->appearanceBoundingBox(), QRectF(0, 0, expectedBounds.width(), expectedBounds.height()));
    QCOMPARE(ink->boundingRectangle().left, expectedBounds.left() / pageWidth);
    QCOMPARE(ink->boundingRectangle().right, expectedBounds.right() / pageWidth);
    QCOMPARE(ink->boundingRectangle().top, 1.0 - expectedBounds.bottom() / pageHeight);
    QCOMPARE(ink->boundingRectangle().bottom, 1.0 - expectedBounds.top() / pageHeight);

    Poppler::InkAnnotation liveInk;
    const Okular::NormalizedRect bounds = ink->boundingRectangle();
    liveInk.setBoundary(QRectF(bounds.left, bounds.top, bounds.width(), bounds.height()));
    QList<QList<QPointF>> popplerPaths;
    for (const QList<Okular::NormalizedPoint> &path : ink->inkPaths()) {
        QList<QPointF> points;
        for (const Okular::NormalizedPoint &point : path) {
            points.append(QPointF(point.x, point.y));
        }
        popplerPaths.append(points);
    }
    liveInk.setInkPaths(popplerPaths);
    liveInk.setAppearanceStream(ink->appearanceStream(), ink->appearanceBoundingBox());
    livePage->addAnnotation(&liveInk);
    const QImage immediateImage = livePage->renderToImage(72, 72);
    QVERIFY(countMagentaPixels(immediateImage) > 100);
    const QRect immediateBounds = magentaPixelBounds(immediateImage);
    const qreal pdfScaleX = static_cast<qreal>(immediateImage.width()) / liveStrokeImage.width();
    const qreal pdfScaleY = static_cast<qreal>(immediateImage.height()) / liveStrokeImage.height();
    const QRect expectedPdfBounds(qRound(liveStrokeBounds.left() * pdfScaleX),
                                  qRound(liveStrokeBounds.top() * pdfScaleY),
                                  qRound(liveStrokeBounds.width() * pdfScaleX),
                                  qRound(liveStrokeBounds.height() * pdfScaleY));
    QVERIFY(qAbs(immediateBounds.left() - expectedPdfBounds.left()) <= 3);
    QVERIFY(qAbs(immediateBounds.top() - expectedPdfBounds.top()) <= 3);
    QVERIFY(qAbs(immediateBounds.right() - expectedPdfBounds.right()) <= 3);
    QVERIFY(qAbs(immediateBounds.bottom() - expectedPdfBounds.bottom()) <= 3);

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
    QCOMPARE(savedInk->inkPaths().constFirst().size(), samples.size());
    const QImage savedImage = savedPage->renderToImage(72, 72);
    QVERIFY(countMagentaPixels(savedImage) > 100);
    const QRect savedBounds = magentaPixelBounds(savedImage);
    QVERIFY(qAbs(savedBounds.left() - expectedPdfBounds.left()) <= 3);
    QVERIFY(qAbs(savedBounds.top() - expectedPdfBounds.top()) <= 3);
    QVERIFY(qAbs(savedBounds.right() - expectedPdfBounds.right()) <= 3);
    QVERIFY(qAbs(savedBounds.bottom() - expectedPdfBounds.bottom()) <= 3);

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

void AddRemoveAnnotationTest::testInkCustomAppearanceRotatedPage()
{
#if !defined(HAVE_POPPLER_QT6) || !defined(POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE)
    QSKIP("This test requires Poppler's custom Ink appearance API");
#else
    QTemporaryFile rotatedPdf(QDir::tempPath() + QStringLiteral("/charta-rotated-ink-XXXXXX.pdf"));
    QVERIFY(rotatedPdf.open());

    QByteArray pdf("%PDF-1.4\n");
    QVector<qint64> objectOffsets(5);
    auto appendObject = [&pdf, &objectOffsets](int objectNumber, const QByteArray &contents) {
        objectOffsets[objectNumber] = pdf.size();
        pdf += QByteArray::number(objectNumber) + " 0 obj\n" + contents + "\nendobj\n";
    };
    appendObject(1, "<< /Type /Catalog /Pages 2 0 R >>");
    appendObject(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    appendObject(3, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 841.89 595.28] /Rotate 270 /Resources << >> /Contents 4 0 R >>");
    const QByteArray contentStream("q\n1 1 1 rg\n0 0 841.89 595.28 re\nf\nQ\n");
    appendObject(4, "<< /Length " + QByteArray::number(contentStream.size()) + " >>\nstream\n" + contentStream + "endstream");
    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n0 5\n0000000000 65535 f \n";
    for (int objectNumber = 1; objectNumber < objectOffsets.size(); ++objectNumber) {
        pdf += QByteArray::number(objectOffsets[objectNumber]).rightJustified(10, '0') + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";
    QCOMPARE(rotatedPdf.write(pdf), qint64(pdf.size()));
    rotatedPdf.flush();

    m_document->closeDocument();
    QMimeDatabase mimeDatabase;
    const QMimeType mime = mimeDatabase.mimeTypeForFile(rotatedPdf.fileName());
    QCOMPARE(m_document->openDocument(rotatedPdf.fileName(), QUrl(), mime), Okular::Document::OpenSuccess);
    const Okular::Page *page = m_document->page(0);
    QVERIFY(page);
    QCOMPARE(page->orientation(), Okular::Rotation270);

    QDomDocument engineDocument;
    QDomElement toolElement = engineDocument.createElement(QStringLiteral("tool"));
    toolElement.setAttribute(QStringLiteral("color"), QStringLiteral("#ff00ff"));
    QDomElement annotationElement = engineDocument.createElement(QStringLiteral("annotation"));
    annotationElement.setAttribute(QStringLiteral("type"), QStringLiteral("Ink"));
    annotationElement.setAttribute(QStringLiteral("width"), QStringLiteral("12"));
    annotationElement.setAttribute(QStringLiteral("color"), QStringLiteral("#ff00ff"));
    toolElement.appendChild(annotationElement);
    engineDocument.appendChild(toolElement);

    const QList<QPointF> samples {QPointF(0.2, 0.2), QPointF(0.38, 0.2), QPointF(0.38, 0.52), QPointF(0.62, 0.62)};
    SmoothPathEngine engine(toolElement);
    const auto modifiers = AnnotatorEngine::Modifiers {};
    const qreal pageWidth = page->width();
    const qreal pageHeight = page->height();
    engine.event(AnnotatorEngine::Press, AnnotatorEngine::Left, modifiers, samples.first().x(), samples.first().y(), pageWidth, pageHeight, page);
    for (int index = 1; index < samples.size(); ++index) {
        engine.event(AnnotatorEngine::Move, AnnotatorEngine::Left, modifiers, samples[index].x(), samples[index].y(), pageWidth, pageHeight, page);
    }
    engine.event(AnnotatorEngine::Release, AnnotatorEngine::Left, modifiers, samples.last().x(), samples.last().y(), pageWidth, pageHeight, page);
    QVERIFY(engine.creationCompleted());

    QImage preview(qRound(pageWidth), qRound(pageHeight), QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    QPainter previewPainter(&preview);
    engine.paint(&previewPainter, pageWidth, pageHeight, preview.rect());
    previewPainter.end();

    const QList<Okular::Annotation *> annotations = engine.end();
    QCOMPARE(annotations.size(), 1);
    auto *ink = dynamic_cast<Okular::InkAnnotation *>(annotations.constFirst());
    QVERIFY(ink);
    m_document->addPageAnnotation(0, ink);

    QTemporaryFile savedPdf(QDir::tempPath() + QStringLiteral("/charta-rotated-ink-saved-XXXXXX.pdf"));
    QVERIFY(savedPdf.open());
    savedPdf.close();
    QString errorText;
    QVERIFY2(m_document->saveChanges(savedPdf.fileName(), &errorText), qPrintable(errorText));

    auto savedDocument = Poppler::Document::load(savedPdf.fileName());
    QVERIFY(savedDocument);
    auto savedPage = savedDocument->page(0);
    QVERIFY(savedPage);
    QCOMPARE(savedPage->orientation(), Poppler::Page::Seascape);
    const QImage savedImage = savedPage->renderToImage(72, 72);

    auto isMagenta = [](const QColor &color) {
        return color.red() > 150 && color.blue() > 150 && color.green() < 140 && color.alpha() > 80;
    };
    int expectedPixels = 0;
    int renderedPixels = 0;
    int matchingPixels = 0;
    for (int y = 0; y < savedImage.height(); ++y) {
        for (int x = 0; x < savedImage.width(); ++x) {
            const int previewX = qBound(0, qRound(static_cast<qreal>(x) * preview.width() / savedImage.width()), preview.width() - 1);
            const int previewY = qBound(0, qRound(static_cast<qreal>(y) * preview.height() / savedImage.height()), preview.height() - 1);
            const bool expected = isMagenta(preview.pixelColor(previewX, previewY));
            const bool rendered = isMagenta(savedImage.pixelColor(x, y));
            expectedPixels += expected;
            renderedPixels += rendered;
            matchingPixels += expected && rendered;
        }
    }
    QVERIFY(expectedPixels > 100);
    QVERIFY(renderedPixels > 100);
    QVERIFY(static_cast<double>(matchingPixels) / (expectedPixels + renderedPixels - matchingPixels) > 0.5);
#endif
}

QTEST_MAIN(AddRemoveAnnotationTest)
#include "addremoveannotationtest.moc"
