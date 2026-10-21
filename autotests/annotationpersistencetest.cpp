/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QImage>
#include <QMimeDatabase>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUuid>

#include <poppler-annotation.h>
#include <poppler-converter.h>
#include <poppler-qt6.h>

#include "../core/annotations.h"
#include "../core/document.h"
#include "../core/page.h"
#include "../settings_core.h"

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

QString saveDocument(Okular::Document *document, const QString &output)
{
    QString error;
    return document->saveChanges(output, &error) ? QString() : error;
}

// Writes a minimal one-page PDF with the requested /Rotate value.  The page
// intentionally has no annotations so the test can add them through the same
// proxy path the application uses.
void writePdf(const QString &fileName, int rotate)
{
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));

    QByteArray pdf("%PDF-1.5\n");
    QVector<qint64> objectOffsets(5);
    auto appendObject = [&pdf, &objectOffsets](int objectNumber, const QByteArray &contents) {
        objectOffsets[objectNumber] = pdf.size();
        pdf += QByteArray::number(objectNumber) + " 0 obj\n" + contents + "\nendobj\n";
    };

    appendObject(1, "<< /Type /Catalog /Pages 2 0 R >>");
    appendObject(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    QByteArray pageDictionary = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> /Contents 4 0 R";
    if (rotate != 0) {
        pageDictionary += " /Rotate " + QByteArray::number(rotate);
    }
    pageDictionary += " >>";
    appendObject(3, pageDictionary);
    const QByteArray contentStream("q\n1 1 1 rg\n0 0 612 792 re\nf\nQ\n");
    appendObject(4, "<< /Length " + QByteArray::number(contentStream.size()) + " >>\nstream\n" + contentStream + "endstream");

    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n0 5\n0000000000 65535 f \n";
    for (int objectNumber = 1; objectNumber < objectOffsets.size(); ++objectNumber) {
        pdf += QByteArray::number(objectOffsets[objectNumber]).rightJustified(10, '0') + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";
    QCOMPARE(file.write(pdf), qint64(pdf.size()));
}

std::unique_ptr<Okular::Document> openDocument(const QString &fileName)
{
    auto document = std::make_unique<Okular::Document>(nullptr);
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(fileName);
    if (document->openDocument(fileName, QUrl(), mime) != Okular::Document::OpenSuccess) {
        return nullptr;
    }
    return document;
}

Okular::Annotation *makeAnnotation(const QString &type, const QString &uniqueName)
{
    Okular::Annotation *annotation = nullptr;
    if (type == QLatin1String("text")) {
        auto *text = new Okular::TextAnnotation();
        text->setTextType(Okular::TextAnnotation::InPlace);
        text->setContents(QStringLiteral("rotated text"));
        text->setTextFont(QFont(QStringLiteral("Sans Serif"), 12));
        annotation = text;
    } else if (type == QLatin1String("linked-text")) {
        auto *text = new Okular::TextAnnotation();
        text->setTextType(Okular::TextAnnotation::Linked);
        text->setContents(QStringLiteral("rotated linked text"));
        text->setTextIcon(QStringLiteral("Note"));
        annotation = text;
    } else if (type == QLatin1String("stamp")) {
        auto *stamp = new Okular::StampAnnotation();
        stamp->setStampIconName(QStringLiteral("Draft"));
        annotation = stamp;
    } else if (type == QLatin1String("geom")) {
        auto *geom = new Okular::GeomAnnotation();
        geom->setGeometricalType(Okular::GeomAnnotation::InscribedSquare);
        geom->setGeometricalInnerColor(QColor(230, 20, 20));
        annotation = geom;
    } else if (type == QLatin1String("line")) {
        auto *line = new Okular::LineAnnotation();
        line->setLinePoints({Okular::NormalizedPoint(0.25, 0.35), Okular::NormalizedPoint(0.48, 0.25)});
        annotation = line;
    } else if (type == QLatin1String("ink")) {
        auto *ink = new Okular::InkAnnotation();
        ink->setInkPaths({{Okular::NormalizedPoint(0.25, 0.35), Okular::NormalizedPoint(0.36, 0.25), Okular::NormalizedPoint(0.48, 0.35)}});
        annotation = ink;
    }

    Q_ASSERT(annotation);
    annotation->setUniqueName(uniqueName);
    annotation->setBoundingRectangle(Okular::NormalizedRect(0.22, 0.22, 0.48, 0.38));
    annotation->style().setColor(QColor(230, 20, 20));
    annotation->style().setWidth(type == QLatin1String("ink") ? 6.0 : 2.0);
    return annotation;
}

double frameAngleDegrees(const Okular::AnnotationFrame &frame)
{
    double degrees = qRadiansToDegrees(frame.rotation);
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) {
        degrees += 360.0;
    }
    return degrees;
}

double angleDistance(double first, double second)
{
    double difference = std::fmod(first - second, 360.0);
    if (difference < -180.0) {
        difference += 360.0;
    } else if (difference > 180.0) {
        difference -= 360.0;
    }
    return std::fabs(difference);
}

int pageOrientationDegrees(Poppler::Page::Orientation orientation)
{
    switch (orientation) {
    case Poppler::Page::Landscape:
        return 90;
    case Poppler::Page::UpsideDown:
        return 180;
    case Poppler::Page::Seascape:
        return 270;
    case Poppler::Page::Portrait:
        return 0;
    }
    return 0;
}

Poppler::Annotation::SubType popplerSubType(const QString &type)
{
    if (type == QLatin1String("text") || type == QLatin1String("linked-text")) {
        return Poppler::Annotation::AText;
    }
    if (type == QLatin1String("stamp")) {
        return Poppler::Annotation::AStamp;
    }
    if (type == QLatin1String("geom")) {
        return Poppler::Annotation::AGeom;
    }
    if (type == QLatin1String("line")) {
        return Poppler::Annotation::ALine;
    }
    return Poppler::Annotation::AInk;
}

Okular::Annotation *findAnnotation(const Okular::Page *page, const QString &uniqueName)
{
    for (Okular::Annotation *annotation : page->annotations()) {
        if (annotation->uniqueName() == uniqueName) {
            return annotation;
        }
    }
    return nullptr;
}

void rotateAnnotation(Okular::Document *document, Okular::Annotation *annotation, double degrees)
{
    const double radians = qDegreesToRadians(degrees);
    const Okular::Document::AnnotationRotationItem item {0, annotation, radians, annotation->frame().center};
    document->rotateAnnotationsLive({item}, radians);
    document->commitAnnotationRotation({item});
}

void verifyPopplerMetadata(Poppler::Page *page, const QString &uniqueName, Poppler::Annotation::SubType type, const Okular::AnnotationFrame &frame)
{
    const std::vector<std::unique_ptr<Poppler::Annotation>> annotations = page->annotations({type});
    Poppler::Annotation *annotation = nullptr;
    for (const std::unique_ptr<Poppler::Annotation> &candidate : annotations) {
        if (candidate->uniqueName() == uniqueName) {
            annotation = candidate.get();
            break;
        }
    }
    QVERIFY(annotation);

    const int orientation = pageOrientationDegrees(page->orientation());
    const double expectedPdfDegrees = orientation - frameAngleDegrees(frame);
    QVERIFY2(angleDistance(annotation->userRotation(), expectedPdfDegrees) < 0.01, qPrintable(QStringLiteral("angle %1 expected %2").arg(annotation->userRotation()).arg(expectedPdfDegrees)));

    const QSizeF displaySize = page->pageSizeF();
    const double expectedWidth = frame.size.width() * displaySize.width();
    const double expectedHeight = frame.size.height() * displaySize.height();
    const QSizeF localSize = annotation->userLocalSize();
    QVERIFY2(std::fabs(localSize.width() - expectedWidth) < 0.01, qPrintable(QStringLiteral("local width %1 expected %2").arg(localSize.width()).arg(expectedWidth)));
    QVERIFY2(std::fabs(localSize.height() - expectedHeight) < 0.01, qPrintable(QStringLiteral("local height %1 expected %2").arg(localSize.height()).arg(expectedHeight)));
}
}


QList<double> shapeSignature(const Okular::Annotation *annotation, const Okular::Page *page)
{
    QList<double> signature;
    const double width = page->width();
    const double height = page->height();
    const auto appendPointDistances = [&](const QList<QList<Okular::NormalizedPoint>> &paths) {
        for (const QList<Okular::NormalizedPoint> &path : paths) {
            for (qsizetype i = 1; i < path.size(); ++i) {
                const double dx = (path[i].x - path[i - 1].x) * width;
                const double dy = (path[i].y - path[i - 1].y) * height;
                signature.append(std::hypot(dx, dy));
            }
        }
    };

    if (annotation->subType() == Okular::Annotation::ALine) {
        const auto *line = static_cast<const Okular::LineAnnotation *>(annotation);
        appendPointDistances({line->linePoints()});
    } else if (annotation->subType() == Okular::Annotation::AInk) {
        const auto *ink = static_cast<const Okular::InkAnnotation *>(annotation);
        appendPointDistances(ink->inkPaths());
    } else if (annotation->subType() == Okular::Annotation::AText) {
        const auto *text = static_cast<const Okular::TextAnnotation *>(annotation);
        QList<Okular::NormalizedPoint> callout;
        for (int i = 0; i < 3; ++i) {
            callout.append(text->inplaceCallout(i));
        }
        appendPointDistances({callout});
    }

    const QSizeF frameSize = Okular::AnnotationUtils::annotationFrameSizeInPagePoints(annotation);
    signature.append(frameSize.width());
    signature.append(frameSize.height());
    return signature;
}

bool signaturesEqual(const QList<double> &first, const QList<double> &second)
{
    if (first.size() != second.size()) {
        return false;
    }
    for (qsizetype i = 0; i < first.size(); ++i) {
        if (std::fabs(first[i] - second[i]) > 1e-4) {
            return false;
        }
    }
    return true;
}

class AnnotationPersistenceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void rotatesGeomAppearance();
    void preservesRotationMetadata();

    void persistenceMatrix_data();
    void persistenceMatrix();
    void repeatedSaveReopenDoesNotDrift();
    void moveAfterRotationKeepsAngleAndSize();
    void propertyChangeAfterRotationKeepsFrame();
    void fixedRotationAndDenyWriteBlockRotation();
    void rendersRotatedGeomAtExpectedPosition_data();
    void rendersRotatedGeomAtExpectedPosition();
    void freshRotationMatchesReopened_data();
    void freshRotationMatchesReopened();
    void customAppearanceInkRotationStaysInsideFrame();
};

void AnnotationPersistenceTest::initTestCase()
{
#ifdef POPPLER_QT6_HAS_ANNOTATION_ROTATION
    QVERIFY(QFile::exists(sourcePdf()));
    Okular::SettingsCore::instance(QStringLiteral("annotationpersistencetest"));
#else
    QSKIP("Poppler does not expose the annotation rotation API");
#endif
}

void AnnotationPersistenceTest::rotatesGeomAppearance()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString output = directory.filePath(QStringLiteral("rotated.pdf"));

    std::unique_ptr<Poppler::Document> document(Poppler::Document::load(sourcePdf()));
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

    std::unique_ptr<Poppler::Document> document(Poppler::Document::load(sourcePdf()));
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

void AnnotationPersistenceTest::persistenceMatrix_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<double>("angle");
    QTest::addColumn<int>("pageRotation");

    const QStringList types {QStringLiteral("text"), QStringLiteral("stamp"), QStringLiteral("geom"), QStringLiteral("line"), QStringLiteral("ink")};
    const QList<double> angles {0.0, 15.0, 45.0, 90.0, 180.0, 359.0};
    const QList<int> pageRotations {0, 90, 180, 270};

    for (const QString &type : types) {
        for (double angle : angles) {
            for (int pageRotation : pageRotations) {
                const QByteArray name = QStringLiteral("%1-%2-%3").arg(type).arg(angle).arg(pageRotation).toLatin1();
                QTest::newRow(name.constData()) << type << angle << pageRotation;
            }
        }
    }
}

void AnnotationPersistenceTest::persistenceMatrix()
{
    QFETCH(QString, type);
    QFETCH(double, angle);
    QFETCH(int, pageRotation);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    const QString output = directory.filePath(QStringLiteral("saved.pdf"));
    writePdf(source, pageRotation);

    const QString uniqueName = QStringLiteral("matrix-") + type + QStringLiteral("-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<Okular::Document> document = openDocument(source);
    QVERIFY(document);
    Okular::Annotation *annotation = makeAnnotation(type, uniqueName);
    document->addPageAnnotation(0, annotation);
    rotateAnnotation(document.get(), annotation, angle);

    const Okular::AnnotationFrame expectedFrame = annotation->frame();
    QVERIFY2(angleDistance(frameAngleDegrees(expectedFrame), angle) < 0.01, qPrintable(QStringLiteral("frame angle %1 expected %2").arg(frameAngleDegrees(expectedFrame)).arg(angle)));

    const QString saveError = saveDocument(document.get(), output);
    QVERIFY2(saveError.isEmpty(), qPrintable(saveError));
    document.reset();

    std::unique_ptr<Okular::Document> reopened = openDocument(output);
    QVERIFY(reopened);
    Okular::Annotation *reopenedAnnotation = findAnnotation(reopened->page(0), uniqueName);
    QVERIFY(reopenedAnnotation);
    const Okular::AnnotationFrame reopenedFrame = reopenedAnnotation->frame();
    QVERIFY2(angleDistance(frameAngleDegrees(reopenedFrame), frameAngleDegrees(expectedFrame)) < 0.01,
             qPrintable(QStringLiteral("reopened angle %1 expected %2").arg(frameAngleDegrees(reopenedFrame)).arg(frameAngleDegrees(expectedFrame))));
    QVERIFY2(std::fabs(reopenedFrame.size.width() - expectedFrame.size.width()) < 1e-5,
             qPrintable(QStringLiteral("reopened width %1 expected %2").arg(reopenedFrame.size.width()).arg(expectedFrame.size.width())));
    QVERIFY2(std::fabs(reopenedFrame.size.height() - expectedFrame.size.height()) < 1e-5,
             qPrintable(QStringLiteral("reopened height %1 expected %2").arg(reopenedFrame.size.height()).arg(expectedFrame.size.height())));
    QVERIFY2(std::fabs(reopenedFrame.center.x - expectedFrame.center.x) < 1e-5, qPrintable(QStringLiteral("reopened center x %1 expected %2").arg(reopenedFrame.center.x).arg(expectedFrame.center.x)));
    QVERIFY2(std::fabs(reopenedFrame.center.y - expectedFrame.center.y) < 1e-5, qPrintable(QStringLiteral("reopened center y %1 expected %2").arg(reopenedFrame.center.y).arg(expectedFrame.center.y)));

    std::unique_ptr<Poppler::Document> popplerDocument(Poppler::Document::load(output));
    QVERIFY(popplerDocument);
    std::unique_ptr<Poppler::Page> popplerPage(popplerDocument->page(0));
    QVERIFY(popplerPage);
    verifyPopplerMetadata(popplerPage.get(), uniqueName, popplerSubType(type), expectedFrame);
}

void AnnotationPersistenceTest::repeatedSaveReopenDoesNotDrift()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString input = directory.filePath(QStringLiteral("cycle0.pdf"));
    writePdf(input, 90);

    const QString uniqueName = QStringLiteral("cycle-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    double expectedAngle = 45.0;
    Okular::AnnotationFrame expectedFrame;
    bool haveExpectedFrame = false;

    for (int cycle = 1; cycle <= 5; ++cycle) {
        std::unique_ptr<Okular::Document> document = openDocument(input);
        QVERIFY2(document, qPrintable(QStringLiteral("failed to reopen %1").arg(input)));
        Okular::Annotation *annotation = findAnnotation(document->page(0), uniqueName);

        if (!annotation) {
            annotation = makeAnnotation(QStringLiteral("geom"), uniqueName);
            document->addPageAnnotation(0, annotation);
            rotateAnnotation(document.get(), annotation, expectedAngle);
        } else {
            rotateAnnotation(document.get(), annotation, 15.0);
            expectedAngle += 15.0;
        }

        const Okular::AnnotationFrame currentFrame = annotation->frame();
        if (!haveExpectedFrame) {
            expectedFrame = currentFrame;
            haveExpectedFrame = true;
        } else {
            QVERIFY2(angleDistance(frameAngleDegrees(currentFrame), expectedAngle) < 0.01,
                     qPrintable(QStringLiteral("cycle %1 angle %2 expected %3").arg(cycle).arg(frameAngleDegrees(currentFrame)).arg(expectedAngle)));
            QVERIFY2(std::fabs(currentFrame.size.width() - expectedFrame.size.width()) < 1e-5, qPrintable(QStringLiteral("cycle %1 width drifted").arg(cycle)));
            QVERIFY2(std::fabs(currentFrame.size.height() - expectedFrame.size.height()) < 1e-5, qPrintable(QStringLiteral("cycle %1 height drifted").arg(cycle)));
            QVERIFY2(std::fabs(currentFrame.center.x - expectedFrame.center.x) < 1e-5, qPrintable(QStringLiteral("cycle %1 center x drifted").arg(cycle)));
            QVERIFY2(std::fabs(currentFrame.center.y - expectedFrame.center.y) < 1e-5, qPrintable(QStringLiteral("cycle %1 center y drifted").arg(cycle)));
        }

        const QString output = directory.filePath(QStringLiteral("cycle%1.pdf").arg(cycle));
        const QString error = saveDocument(document.get(), output);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        document.reset();
        input = output;
    }

    std::unique_ptr<Okular::Document> reopened = openDocument(input);
    QVERIFY(reopened);
    Okular::Annotation *annotation = findAnnotation(reopened->page(0), uniqueName);
    QVERIFY(annotation);
    QVERIFY2(angleDistance(frameAngleDegrees(annotation->frame()), expectedAngle) < 0.01,
             qPrintable(QStringLiteral("final angle %1 expected %2").arg(frameAngleDegrees(annotation->frame())).arg(expectedAngle)));
}

void AnnotationPersistenceTest::moveAfterRotationKeepsAngleAndSize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    const QString movedOutput = directory.filePath(QStringLiteral("moved.pdf"));
    writePdf(source, 270);

    const QString uniqueName = QStringLiteral("moved-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<Okular::Document> document = openDocument(source);
    QVERIFY(document);
    Okular::Annotation *annotation = makeAnnotation(QStringLiteral("text"), uniqueName);
    document->addPageAnnotation(0, annotation);
    rotateAnnotation(document.get(), annotation, 45.0);
    const Okular::AnnotationFrame rotatedFrame = annotation->frame();

    document->translatePageAnnotation(0, annotation, Okular::NormalizedPoint(0.07, 0.05));
    const Okular::AnnotationFrame movedFrame = annotation->frame();
    QVERIFY2(angleDistance(frameAngleDegrees(movedFrame), 45.0) < 0.01, "move changed the angle");
    QVERIFY2(std::fabs(movedFrame.size.width() - rotatedFrame.size.width()) < 1e-5, "move changed the width");
    QVERIFY2(std::fabs(movedFrame.size.height() - rotatedFrame.size.height()) < 1e-5, "move changed the height");

    const QString error = saveDocument(document.get(), movedOutput);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    document.reset();

    std::unique_ptr<Okular::Document> reopened = openDocument(movedOutput);
    QVERIFY(reopened);
    Okular::Annotation *reopenedAnnotation = findAnnotation(reopened->page(0), uniqueName);
    QVERIFY(reopenedAnnotation);
    const Okular::AnnotationFrame reopenedFrame = reopenedAnnotation->frame();
    QVERIFY2(angleDistance(frameAngleDegrees(reopenedFrame), 45.0) < 0.01, "reopened angle changed after move");
    QVERIFY2(std::fabs(reopenedFrame.center.x - movedFrame.center.x) < 1e-5, "reopened center x changed after move");
    QVERIFY2(std::fabs(reopenedFrame.center.y - movedFrame.center.y) < 1e-5, "reopened center y changed after move");
}

void AnnotationPersistenceTest::propertyChangeAfterRotationKeepsFrame()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    const QString output = directory.filePath(QStringLiteral("property.pdf"));
    writePdf(source, 90);

    const QString uniqueName = QStringLiteral("property-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<Okular::Document> document = openDocument(source);
    QVERIFY(document);
    Okular::Annotation *annotation = makeAnnotation(QStringLiteral("geom"), uniqueName);
    document->addPageAnnotation(0, annotation);
    rotateAnnotation(document.get(), annotation, 30.0);
    const Okular::AnnotationFrame rotatedFrame = annotation->frame();

    document->prepareToModifyAnnotationProperties(annotation);
    annotation->style().setColor(QColor(20, 40, 230));
    annotation->style().setWidth(5.0);
    document->modifyPageAnnotationProperties(0, annotation);

    const Okular::AnnotationFrame modifiedFrame = annotation->frame();
    QVERIFY2(angleDistance(frameAngleDegrees(modifiedFrame), frameAngleDegrees(rotatedFrame)) < 0.01, "property change reset the angle");
    QVERIFY2(std::fabs(modifiedFrame.size.width() - rotatedFrame.size.width()) < 1e-5, "property change reset the width");
    QVERIFY2(std::fabs(modifiedFrame.size.height() - rotatedFrame.size.height()) < 1e-5, "property change reset the height");

    const QString error = saveDocument(document.get(), output);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    document.reset();

    std::unique_ptr<Okular::Document> reopened = openDocument(output);
    QVERIFY(reopened);
    Okular::Annotation *reopenedAnnotation = findAnnotation(reopened->page(0), uniqueName);
    QVERIFY(reopenedAnnotation);
    const Okular::AnnotationFrame reopenedFrame = reopenedAnnotation->frame();
    QVERIFY2(angleDistance(frameAngleDegrees(reopenedFrame), frameAngleDegrees(rotatedFrame)) < 0.01, "reopened angle changed after property edit");
    QCOMPARE(reopenedAnnotation->style().color(), QColor(20, 40, 230));
}


void AnnotationPersistenceTest::freshRotationMatchesReopened_data()
{
    QTest::addColumn<QString>("type");
    for (const QString &type : {QStringLiteral("text"), QStringLiteral("stamp"), QStringLiteral("geom"), QStringLiteral("line"), QStringLiteral("ink")}) {
        QTest::newRow(qPrintable(type)) << type;
    }
}

void AnnotationPersistenceTest::freshRotationMatchesReopened()
{
    QFETCH(QString, type);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    const QString unrotated = directory.filePath(QStringLiteral("unrotated.pdf"));
    writePdf(source, 0);

    std::unique_ptr<Okular::Document> freshDocument = openDocument(source);
    QVERIFY(freshDocument);
    Okular::Annotation *fresh = makeAnnotation(type, QStringLiteral("fresh"));
    freshDocument->addPageAnnotation(0, fresh);
    const QList<double> freshBefore = shapeSignature(fresh, freshDocument->page(0));
    rotateAnnotation(freshDocument.get(), fresh, 30.0);
    const Okular::AnnotationFrame freshFrame = fresh->frame();
    const QList<double> freshAfter = shapeSignature(fresh, freshDocument->page(0));

    std::unique_ptr<Okular::Document> unrotatedDocument = openDocument(source);
    QVERIFY(unrotatedDocument);
    Okular::Annotation *saved = makeAnnotation(type, QStringLiteral("saved"));
    unrotatedDocument->addPageAnnotation(0, saved);
    const QString saveError = saveDocument(unrotatedDocument.get(), unrotated);
    QVERIFY2(saveError.isEmpty(), qPrintable(saveError));
    unrotatedDocument.reset();

    std::unique_ptr<Okular::Document> reopenedDocument = openDocument(unrotated);
    QVERIFY(reopenedDocument);
    Okular::Annotation *reopened = findAnnotation(reopenedDocument->page(0), QStringLiteral("saved"));
    QVERIFY(reopened);
    const QList<double> reopenedBefore = shapeSignature(reopened, reopenedDocument->page(0));
    rotateAnnotation(reopenedDocument.get(), reopened, 30.0);
    const Okular::AnnotationFrame reopenedFrame = reopened->frame();
    const QList<double> reopenedAfter = shapeSignature(reopened, reopenedDocument->page(0));

    QVERIFY2(signaturesEqual(freshBefore, freshAfter), "fresh rotation changed geometry");
    QVERIFY2(signaturesEqual(reopenedBefore, reopenedAfter), "reopened rotation changed geometry");
    QVERIFY2(signaturesEqual(freshAfter, reopenedAfter), "fresh and reopened rotations differ");
    QVERIFY2(std::fabs(freshFrame.center.x - reopenedFrame.center.x) < 1e-5, "center x differs");
    QVERIFY2(std::fabs(freshFrame.center.y - reopenedFrame.center.y) < 1e-5, "center y differs");
    QVERIFY2(std::fabs(freshFrame.size.width() - reopenedFrame.size.width()) < 1e-5, "width differs");
    QVERIFY2(std::fabs(freshFrame.size.height() - reopenedFrame.size.height()) < 1e-5, "height differs");
}

void AnnotationPersistenceTest::customAppearanceInkRotationStaysInsideFrame()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    const QString output = directory.filePath(QStringLiteral("custom-ink-rotated.pdf"));
    writePdf(source, 0);

    const double localWidth = 100.0;
    const double localHeight = 60.0;

    std::unique_ptr<Okular::Document> document = openDocument(source);
    QVERIFY(document);
    const Okular::Page *page = document->page(0);
    QVERIFY(page);
    const double pageWidth = page->width();
    const double pageHeight = page->height();

    auto *ink = new Okular::InkAnnotation();
    ink->setUniqueName(QStringLiteral("custom-ink-rotation"));
    ink->style().setColor(QColor(230, 20, 20));
    ink->style().setWidth(2.0);
    ink->setInkPaths({{Okular::NormalizedPoint(0.3, 0.3), Okular::NormalizedPoint(0.4, 0.4)}});

    const double left = 0.25;
    const double top = 0.25;
    const double right = left + localWidth / page->width();
    const double bottom = top + localHeight / page->height();
    ink->setBoundingRectangle(Okular::NormalizedRect(left, top, right, bottom));

    // A custom appearance that fills its whole local box.  Rotating it must
    // keep it inside the rotated frame AABB; a centre-less rotation shifts the
    // stream outside the box and clips it.
    const QByteArray appearance = QByteArrayLiteral("q\n1 0 0 rg\n0 0 100 60 re\nf\nQ\n");
    ink->setAppearanceStream(appearance, QRectF(0, 0, localWidth, localHeight));

    document->addPageAnnotation(0, ink);
    rotateAnnotation(document.get(), ink, 30.0);

    const QPolygonF outline = Okular::AnnotationUtils::annotationOutline(ink, page->width(), page->height());
    QVERIFY(outline.size() >= 4);
    QVERIFY(!outline.boundingRect().isEmpty());

    const QString saveError = saveDocument(document.get(), output);
    QVERIFY2(saveError.isEmpty(), qPrintable(saveError));
    document.reset();

    std::unique_ptr<Poppler::Document> popplerDocument(Poppler::Document::load(output));
    QVERIFY(popplerDocument);
    std::unique_ptr<Poppler::Page> popplerPage(popplerDocument->page(0));
    QVERIFY(popplerPage);

    // Render at 72 dpi so one pixel equals one PDF point.
    const QImage image = popplerPage->renderToImage(72, 72);
    QVERIFY(!image.isNull());

    // annotationOutline() works in Okular page units, while the Poppler render
    // is in PDF points; convert the expected bounds into render pixels.
    const double scaleX = image.width() / pageWidth;
    const double scaleY = image.height() / pageHeight;
    const QRectF expectedBounds(outline.boundingRect().left() * scaleX,
                                outline.boundingRect().top() * scaleY,
                                outline.boundingRect().width() * scaleX,
                                outline.boundingRect().height() * scaleY);

    QRect redBounds;
    int redPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > 150 && color.green() < 120 && color.blue() < 120) {
                redBounds |= QRect(x, y, 1, 1);
                ++redPixels;
            }
        }
    }

    QVERIFY2(redPixels > 0, "rotated custom ink appearance did not render");
    QVERIFY2(std::fabs(redBounds.left() - expectedBounds.left()) <= 3, qPrintable(QStringLiteral("red left %1 expected %2").arg(redBounds.left()).arg(expectedBounds.left())));
    QVERIFY2(std::fabs(redBounds.top() - expectedBounds.top()) <= 3, qPrintable(QStringLiteral("red top %1 expected %2").arg(redBounds.top()).arg(expectedBounds.top())));
    QVERIFY2(std::fabs(redBounds.right() - expectedBounds.right()) <= 3, qPrintable(QStringLiteral("red right %1 expected %2").arg(redBounds.right()).arg(expectedBounds.right())));
    QVERIFY2(std::fabs(redBounds.bottom() - expectedBounds.bottom()) <= 3, qPrintable(QStringLiteral("red bottom %1 expected %2").arg(redBounds.bottom()).arg(expectedBounds.bottom())));
}

void AnnotationPersistenceTest::fixedRotationAndDenyWriteBlockRotation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    writePdf(source, 0);

    std::unique_ptr<Okular::Document> document = openDocument(source);
    QVERIFY(document);

    auto *fixed = new Okular::TextAnnotation();
    fixed->setTextType(Okular::TextAnnotation::InPlace);
    fixed->setContents(QStringLiteral("fixed"));
    fixed->setBoundingRectangle(Okular::NormalizedRect(0.2, 0.2, 0.3, 0.3));
    fixed->setFlags(Okular::Annotation::FixedRotation);
    document->addPageAnnotation(0, fixed);
    QVERIFY(!fixed->canBeRotated());

    auto *denyWrite = new Okular::TextAnnotation();
    denyWrite->setTextType(Okular::TextAnnotation::InPlace);
    denyWrite->setContents(QStringLiteral("deny write"));
    denyWrite->setBoundingRectangle(Okular::NormalizedRect(0.4, 0.2, 0.5, 0.3));
    denyWrite->setFlags(Okular::Annotation::DenyWrite);
    document->addPageAnnotation(0, denyWrite);
    QVERIFY(!denyWrite->canBeRotated());

    const QString error = saveDocument(document.get(), directory.filePath(QStringLiteral("blocked.pdf")));
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

void AnnotationPersistenceTest::rendersRotatedGeomAtExpectedPosition_data()
{
    QTest::addColumn<double>("angle");
    QTest::addColumn<int>("pageRotation");

    const QList<double> angles {0.0, 30.0, 45.0, 90.0, 180.0, 359.0};
    const QList<int> pageRotations {0, 90, 180, 270};
    for (double angle : angles) {
        for (int pageRotation : pageRotations) {
            const QByteArray name = QStringLiteral("%1-%2").arg(angle).arg(pageRotation).toLatin1();
            QTest::newRow(name.constData()) << angle << pageRotation;
        }
    }
}

void AnnotationPersistenceTest::rendersRotatedGeomAtExpectedPosition()
{
    QFETCH(double, angle);
    QFETCH(int, pageRotation);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = directory.filePath(QStringLiteral("source.pdf"));
    const QString output = directory.filePath(QStringLiteral("rendered.pdf"));
    writePdf(source, pageRotation);

    const QString uniqueName = QStringLiteral("rendered-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    std::unique_ptr<Okular::Document> document = openDocument(source);
    QVERIFY(document);
    Okular::Annotation *annotation = makeAnnotation(QStringLiteral("geom"), uniqueName);
    document->addPageAnnotation(0, annotation);
    rotateAnnotation(document.get(), annotation, angle);
    const Okular::AnnotationFrame frame = annotation->frame();
    const QString error = saveDocument(document.get(), output);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    document.reset();

    std::unique_ptr<Poppler::Document> popplerDocument(Poppler::Document::load(output));
    QVERIFY(popplerDocument);
    std::unique_ptr<Poppler::Page> popplerPage(popplerDocument->page(0));
    QVERIFY(popplerPage);
    const QImage image = popplerPage->renderToImage(72, 72);
    QVERIFY(!image.isNull());

    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > 150 && color.green() < 120 && color.blue() < 120) {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }
    QVERIFY2(!bounds.isEmpty(), "rotated geometry did not render");

    const QSizeF displaySize = popplerPage->pageSizeF();
    const double localWidth = frame.size.width() * displaySize.width();
    const double localHeight = frame.size.height() * displaySize.height();
    const double radians = qDegreesToRadians(frameAngleDegrees(frame));
    const double expectedWidth = std::fabs(localWidth * std::cos(radians)) + std::fabs(localHeight * std::sin(radians));
    const double expectedHeight = std::fabs(localWidth * std::sin(radians)) + std::fabs(localHeight * std::cos(radians));
    QVERIFY2(std::fabs(bounds.width() - expectedWidth) <= 3.0, qPrintable(QStringLiteral("rendered width %1 expected %2").arg(bounds.width()).arg(expectedWidth)));
    QVERIFY2(std::fabs(bounds.height() - expectedHeight) <= 3.0, qPrintable(QStringLiteral("rendered height %1 expected %2").arg(bounds.height()).arg(expectedHeight)));

    const double expectedCenterX = frame.center.x * image.width();
    const double expectedCenterY = frame.center.y * image.height();
    QVERIFY2(std::fabs((bounds.left() + bounds.right()) / 2.0 - expectedCenterX) <= 3.0,
             qPrintable(QStringLiteral("rendered center x %1 expected %2").arg((bounds.left() + bounds.right()) / 2.0).arg(expectedCenterX)));
    QVERIFY2(std::fabs((bounds.top() + bounds.bottom()) / 2.0 - expectedCenterY) <= 3.0,
             qPrintable(QStringLiteral("rendered center y %1 expected %2").arg((bounds.top() + bounds.bottom()) / 2.0).arg(expectedCenterY)));
}

QTEST_MAIN(AnnotationPersistenceTest)
#include "annotationpersistencetest.moc"
