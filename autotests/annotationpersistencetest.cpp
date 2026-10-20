/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <poppler-annotation.h>
#include <poppler-converter.h>
#include <poppler-qt6.h>

#include <cmath>
#include <memory>

namespace
{
constexpr double kDegrees = 30.0;
constexpr double kLocalWidth = 120.0;
constexpr double kLocalHeight = 60.0;

QString sourcePdf()
{
    return QStringLiteral(KDESRCDIR "data/file1.pdf");
}

QString saveWithAnnotation(Poppler::Document *document, const QString &output)
{
    std::unique_ptr<Poppler::PDFConverter> converter = document->pdfConverter();
    converter->setOutputFileName(output);
    converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
    return converter->convert() ? QString() : QStringLiteral("converter failed");
}
}

class AnnotationPersistenceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void rotatesGeomAppearance();
    void preservesRotationMetadata();
};

void AnnotationPersistenceTest::initTestCase()
{
#ifdef POPPLER_QT6_HAS_ANNOTATION_ROTATION
    QVERIFY(QFile::exists(sourcePdf()));
#else
    QSKIP("Poppler does not expose the annotation rotation API");
#endif
}

void AnnotationPersistenceTest::rotatesGeomAppearance()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString output = directory.filePath(QStringLiteral("rotated.pdf"));
    QFile::copy(sourcePdf(), output);

    std::unique_ptr<Poppler::Document> document(Poppler::Document::load(output));
    QVERIFY(document);
    std::unique_ptr<Poppler::Page> page(document->page(0));
    QVERIFY(page);

    auto *annotation = new Poppler::GeomAnnotation();
    annotation->setBoundary(QRectF(0.25, 0.25, 0.25, 0.25));
    annotation->setGeomType(Poppler::GeomAnnotation::InscribedSquare);
    annotation->setGeomInnerColor(QColor(255, 0, 0, 80));
    annotation->setStyle(Poppler::Annotation::Style());
    page->addAnnotation(annotation);
    annotation->setUserRotation(kDegrees, kLocalWidth, kLocalHeight);

    const QString error = saveWithAnnotation(document.get(), output);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    document.reset();

    std::unique_ptr<Poppler::Document> reopened(Poppler::Document::load(output));
    QVERIFY(reopened);
    std::unique_ptr<Poppler::Page> reopenedPage(reopened->page(0));
    QVERIFY(reopenedPage);

    const std::vector<std::unique_ptr<Poppler::Annotation>> annotations = reopenedPage->annotations({Poppler::Annotation::AGeom});
    QCOMPARE(annotations.size(), size_t(1));
    QVERIFY(qFuzzyCompare(annotations.front()->userRotation(), kDegrees));
    QCOMPARE(annotations.front()->userLocalSize(), QSizeF(kLocalWidth, kLocalHeight));

    const QImage rendered = reopenedPage->renderToImage();
    QVERIFY(!rendered.isNull());
}

void AnnotationPersistenceTest::preservesRotationMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString output = directory.filePath(QStringLiteral("line.pdf"));
    QFile::copy(sourcePdf(), output);

    std::unique_ptr<Poppler::Document> document(Poppler::Document::load(output));
    QVERIFY(document);
    std::unique_ptr<Poppler::Page> page(document->page(0));
    QVERIFY(page);

    auto *line = new Poppler::LineAnnotation(Poppler::LineAnnotation::StraightLine);
    line->setBoundary(QRectF(0.2, 0.2, 0.2, 0.2));
    line->setLinePoints({QPointF(0.2, 0.2), QPointF(0.4, 0.4)});
    page->addAnnotation(line);
    line->setUserRotation(-45.0, 80.0, 20.0);

    const QString error = saveWithAnnotation(document.get(), output);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    document.reset();

    std::unique_ptr<Poppler::Document> reopened(Poppler::Document::load(output));
    QVERIFY(reopened);
    std::unique_ptr<Poppler::Page> reopenedPage(reopened->page(0));
    QVERIFY(reopenedPage);

    const std::vector<std::unique_ptr<Poppler::Annotation>> annotations = reopenedPage->annotations({Poppler::Annotation::ALine});
    QCOMPARE(annotations.size(), size_t(1));
    QVERIFY(qFuzzyCompare(annotations.front()->userRotation(), -45.0));
    QCOMPARE(annotations.front()->userLocalSize(), QSizeF(80.0, 20.0));
}

QTEST_MAIN(AnnotationPersistenceTest)
#include "annotationpersistencetest.moc"
