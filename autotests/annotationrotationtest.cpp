/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "../settings_core.h"
#include "core/annotations.h"
#include "core/area.h"
#include "core/document.h"
#include "core/page.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpression>

#include <cmath>
#include <limits>

namespace
{
constexpr double tolerance = 1e-8;

QPointF pagePoint(const Okular::NormalizedPoint &point, const Okular::Page *page)
{
    return QPointF(point.x * page->width(), point.y * page->height());
}

Okular::NormalizedRect expectedAabb(const Okular::AnnotationFrame &frame, const Okular::Page *page)
{
    const double pageWidth = page->width();
    const double pageHeight = page->height();
    const double width = frame.size.width() * pageWidth;
    const double height = frame.size.height() * pageHeight;
    const QPointF center(frame.center.x * pageWidth, frame.center.y * pageHeight);
    const double c = std::cos(frame.rotation);
    const double s = std::sin(frame.rotation);

    double left = std::numeric_limits<double>::max();
    double top = std::numeric_limits<double>::max();
    double right = std::numeric_limits<double>::lowest();
    double bottom = std::numeric_limits<double>::lowest();
    for (const QPointF &local : {QPointF(-width / 2.0, -height / 2.0),
                                 QPointF(width / 2.0, -height / 2.0),
                                 QPointF(width / 2.0, height / 2.0),
                                 QPointF(-width / 2.0, height / 2.0)}) {
        const QPointF point = center + QPointF(local.x() * c - local.y() * s, local.x() * s + local.y() * c);
        left = qMin(left, point.x());
        top = qMin(top, point.y());
        right = qMax(right, point.x());
        bottom = qMax(bottom, point.y());
    }
    return Okular::NormalizedRect(left / pageWidth, top / pageHeight, right / pageWidth, bottom / pageHeight);
}

void compareRect(const Okular::NormalizedRect &actual, const Okular::NormalizedRect &expected, const char *message)
{
    QVERIFY2(std::fabs(actual.left - expected.left) < tolerance, message);
    QVERIFY2(std::fabs(actual.top - expected.top) < tolerance, message);
    QVERIFY2(std::fabs(actual.right - expected.right) < tolerance, message);
    QVERIFY2(std::fabs(actual.bottom - expected.bottom) < tolerance, message);
}

void comparePoint(const Okular::NormalizedPoint &actual, const Okular::NormalizedPoint &expected, const char *message)
{
    QVERIFY2(std::fabs(actual.x - expected.x) < tolerance, message);
    QVERIFY2(std::fabs(actual.y - expected.y) < tolerance, message);
}
}

class AnnotationRotationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testLineResizeAndFixedSize();
    void testFrameAabbAtAllQuadrants();
    void testMovePreservesAngleAndSize();
    void testResizePreservesAngleAndScalesPoints();
    void testRotateFrameRotatesContent();
    void testGeometryXmlRoundTrip();
    void testRotatedHitTestUsesOutline();
    void testLegacySetterResetsWithWarning();

private:
    Okular::LineAnnotation *addLine(const QList<Okular::NormalizedPoint> &points, const Okular::NormalizedRect &boundary);

    Okular::Document *m_document = nullptr;
    const Okular::Page *m_page = nullptr;
};

void AnnotationRotationTest::initTestCase()
{
    Okular::SettingsCore::instance(QStringLiteral("annotationrotationtest"));
    m_document = new Okular::Document(nullptr);
}

void AnnotationRotationTest::cleanupTestCase()
{
    delete m_document;
}

void AnnotationRotationTest::init()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/file1.pdf");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);
    QCOMPARE(m_document->openDocument(testFile, QUrl(), mime), Okular::Document::OpenSuccess);
    m_page = m_document->page(0);
    QVERIFY(m_page);
}

void AnnotationRotationTest::cleanup()
{
    m_document->closeDocument();
    m_page = nullptr;
}

Okular::LineAnnotation *AnnotationRotationTest::addLine(const QList<Okular::NormalizedPoint> &points, const Okular::NormalizedRect &boundary)
{
    auto *line = new Okular::LineAnnotation;
    line->setLinePoints(points);
    line->setBoundingRectangle(boundary);
    m_document->addPageAnnotation(0, line);
    return line;
}

void AnnotationRotationTest::testLineResizeAndFixedSize()
{
    auto *line = addLine({Okular::NormalizedPoint(0.2, 0.2), Okular::NormalizedPoint(0.4, 0.4)}, Okular::NormalizedRect(0.1, 0.1, 0.5, 0.5));
    QVERIFY(line->canBeResized());

    line->adjust(Okular::NormalizedPoint(0.1, 0.1), Okular::NormalizedPoint(0.0, 0.0));
    const QList<Okular::NormalizedPoint> points = line->linePoints();
    QCOMPARE(points.size(), 2);
    comparePoint(points.at(0), Okular::NormalizedPoint(0.275, 0.275), "line point 0 must follow the adjusted boundary");
    comparePoint(points.at(1), Okular::NormalizedPoint(0.425, 0.425), "line point 1 must follow the adjusted boundary");
    QCOMPARE(line->boundingRectangle().left, 0.2);
    QCOMPARE(line->boundingRectangle().top, 0.2);

    line->adjust(Okular::NormalizedPoint(-0.1, -0.1), Okular::NormalizedPoint(0.0, 0.0));
    comparePoint(line->linePoints().at(0), Okular::NormalizedPoint(0.2, 0.2), "undoing the boundary adjustment must restore the points");

    line->setFlags(line->flags() | Okular::Annotation::FixedSize);
    QVERIFY(!line->canBeResized());
    line->setFlags(line->flags() & ~Okular::Annotation::FixedSize);
    QVERIFY(line->canBeResized());
}

void AnnotationRotationTest::testFrameAabbAtAllQuadrants()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.5), Okular::NormalizedPoint(0.6, 0.5)}, Okular::NormalizedRect(0.4, 0.5, 0.6, 0.5));

    for (int degrees = 0; degrees < 360; degrees += 15) {
        Okular::AnnotationFrame frame;
        frame.center = Okular::NormalizedPoint(0.5, 0.5);
        frame.size = QSizeF(0.2, 0.1);
        frame.rotation = qDegreesToRadians(static_cast<double>(degrees));
        QVERIFY(line->setFrame(frame));

        QVERIFY2(std::fabs(line->frame().rotation - frame.rotation) < tolerance, "the frame angle must be preserved");
        compareRect(line->boundingRectangle(), expectedAabb(line->frame(), m_page), "frame AABB must be derived in page units");
        QVERIFY2(std::fabs((line->boundingRectangle().left + line->boundingRectangle().right) / 2.0 - 0.5) < tolerance, "AABB center x");
        QVERIFY2(std::fabs((line->boundingRectangle().top + line->boundingRectangle().bottom) / 2.0 - 0.5) < tolerance, "AABB center y");
    }
}

void AnnotationRotationTest::testMovePreservesAngleAndSize()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.5), Okular::NormalizedPoint(0.6, 0.5)}, Okular::NormalizedRect(0.4, 0.5, 0.6, 0.5));
    Okular::AnnotationFrame frame;
    frame.center = Okular::NormalizedPoint(0.5, 0.5);
    frame.size = QSizeF(0.2, 0.05);
    frame.rotation = qDegreesToRadians(37.0);
    QVERIFY(line->setFrame(frame));

    const Okular::AnnotationFrame before = line->frame();
    const QList<Okular::NormalizedPoint> beforePoints = line->linePoints();
    line->moveFrame(Okular::NormalizedPoint(0.1, -0.05));
    const Okular::AnnotationFrame after = line->frame();

    QVERIFY2(std::fabs(after.center.x - (before.center.x + 0.1)) < tolerance, "moveFrame x");
    QVERIFY2(std::fabs(after.center.y - (before.center.y - 0.05)) < tolerance, "moveFrame y");
    QVERIFY2(std::fabs(after.rotation - before.rotation) < tolerance, "moveFrame must preserve angle");
    QVERIFY2(std::fabs(after.size.width() - before.size.width()) < tolerance, "moveFrame must preserve width");
    QVERIFY2(std::fabs(after.size.height() - before.size.height()) < tolerance, "moveFrame must preserve height");
    comparePoint(line->linePoints().at(0), Okular::NormalizedPoint(beforePoints.at(0).x + 0.1, beforePoints.at(0).y - 0.05), "moveFrame must move content");
}

void AnnotationRotationTest::testResizePreservesAngleAndScalesPoints()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.4), Okular::NormalizedPoint(0.6, 0.6)}, Okular::NormalizedRect(0.4, 0.4, 0.6, 0.6));
    Okular::AnnotationFrame frame;
    frame.center = Okular::NormalizedPoint(0.5, 0.5);
    frame.size = QSizeF(0.2, 0.2);
    frame.rotation = qDegreesToRadians(45.0);
    QVERIFY(line->setFrame(frame));

    Okular::AnnotationFrame target = line->frame();
    target.center = Okular::NormalizedPoint(0.55, 0.55);
    target.size = QSizeF(0.1, 0.1);
    const QList<Okular::NormalizedPoint> beforePoints = line->linePoints();
    QVERIFY(line->resizeFrame(target));
    QVERIFY2(std::fabs(line->frame().rotation - frame.rotation) < tolerance, "resizeFrame must preserve angle");
    QVERIFY2(std::fabs(line->frame().size.width() - 0.1) < tolerance, "resizeFrame width");

    // The line points are diagonal in the local frame; halving the frame
    // around the new center must halve their local offsets.
    const QPointF oldCenter = pagePoint(Okular::NormalizedPoint(0.5, 0.5), m_page);
    const QPointF newCenter = pagePoint(Okular::NormalizedPoint(0.55, 0.55), m_page);
    for (int i = 0; i < beforePoints.size(); ++i) {
        const QPointF relative = pagePoint(beforePoints.at(i), m_page) - oldCenter;
        const double c = std::cos(frame.rotation);
        const double s = std::sin(frame.rotation);
        const QPointF local(relative.x() * c + relative.y() * s, -relative.x() * s + relative.y() * c);
        const QPointF expectedPage = newCenter + QPointF((local.x() * 0.5) * c - (local.y() * 0.5) * s, (local.x() * 0.5) * s + (local.y() * 0.5) * c);
        comparePoint(line->linePoints().at(i), Okular::NormalizedPoint(expectedPage.x() / m_page->width(), expectedPage.y() / m_page->height()), "resizeFrame must scale content in the local frame");
    }

    Okular::AnnotationFrame differentAngle = target;
    differentAngle.rotation += qDegreesToRadians(10.0);
    QVERIFY(!line->resizeFrame(differentAngle));
}

void AnnotationRotationTest::testRotateFrameRotatesContent()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.5), Okular::NormalizedPoint(0.6, 0.5)}, Okular::NormalizedRect(0.4, 0.5, 0.6, 0.5));
    Okular::AnnotationFrame frame;
    frame.center = Okular::NormalizedPoint(0.5, 0.5);
    frame.size = QSizeF(0.2, 0.0);
    frame.rotation = 0.0;
    QVERIFY(line->setFrame(frame));

    const Okular::NormalizedPoint pivot(0.5, 0.5);
    QVERIFY(line->rotateFrame(M_PI_2, pivot));
    QVERIFY2(std::fabs(line->frame().rotation - M_PI_2) < tolerance, "rotateFrame angle");
    QVERIFY2(std::fabs(line->frame().size.width() - 0.2) < tolerance, "rotateFrame size");

    // A horizontal line becomes vertical.  In normalized page units the
    // length changes by pageWidth / pageHeight, so compare in page points.
    QCOMPARE(line->linePoints().size(), 2);
    QVERIFY2(std::fabs(pagePoint(line->linePoints().at(0), m_page).x() - pagePoint(line->linePoints().at(1), m_page).x()) < tolerance, "rotated line must be vertical");
    QVERIFY2(std::fabs(pagePoint(line->linePoints().at(0), m_page).y() - pagePoint(line->linePoints().at(1), m_page).y()) > 1.0, "rotated line must have length");
    QVERIFY2(std::fabs(std::fabs(pagePoint(line->linePoints().at(0), m_page).y() - pagePoint(line->linePoints().at(1), m_page).y()) - 0.2 * m_page->width()) < tolerance, "rotated line length must be preserved");
}

void AnnotationRotationTest::testGeometryXmlRoundTrip()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.5), Okular::NormalizedPoint(0.6, 0.5)}, Okular::NormalizedRect(0.4, 0.4, 0.6, 0.6));
    Okular::AnnotationFrame frame;
    frame.center = Okular::NormalizedPoint(0.52, 0.47);
    frame.size = QSizeF(0.18, 0.11);
    frame.rotation = qDegreesToRadians(123.0);
    QVERIFY(line->setFrame(frame));

    QDomDocument document;
    QDomElement annotationElement = document.createElement(QStringLiteral("annotation"));
    Okular::AnnotationUtils::storeAnnotation(line, annotationElement, document);
    QVERIFY(annotationElement.firstChildElement(QStringLiteral("base")).firstChildElement(QStringLiteral("geometry")).isElement());

    Okular::Annotation *restored = Okular::AnnotationUtils::createAnnotation(annotationElement);
    QVERIFY(restored);
    const Okular::AnnotationFrame restoredFrame = restored->frame();
    QVERIFY2(std::fabs(restoredFrame.center.x - frame.center.x) < tolerance, "XML center x");
    QVERIFY2(std::fabs(restoredFrame.center.y - frame.center.y) < tolerance, "XML center y");
    QVERIFY2(std::fabs(restoredFrame.size.width() - frame.size.width()) < tolerance, "XML size width");
    QVERIFY2(std::fabs(restoredFrame.size.height() - frame.size.height()) < tolerance, "XML size height");
    QVERIFY2(std::fabs(restoredFrame.rotation - frame.rotation) < tolerance, "XML rotation");
    delete restored;

    // Unrotated annotations must remain XML-compatible: no geometry node.
    auto *plain = new Okular::LineAnnotation;
    plain->setLinePoints({Okular::NormalizedPoint(0.1, 0.1), Okular::NormalizedPoint(0.2, 0.2)});
    plain->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.2, 0.2));
    QDomDocument plainDocument;
    QDomElement plainElement = plainDocument.createElement(QStringLiteral("annotation"));
    Okular::AnnotationUtils::storeAnnotation(plain, plainElement, plainDocument);
    QVERIFY(!plainElement.firstChildElement(QStringLiteral("base")).firstChildElement(QStringLiteral("geometry")).isElement());
    delete plain;

    // The polygon tool uses a closed LineAnnotation with a transparent inner
    // color.  Keep the alpha in Ocular XML so it survives a save/reopen and
    // remains unfilled in the PDF proxy.
    auto *polygon = new Okular::LineAnnotation;
    polygon->setLinePoints({Okular::NormalizedPoint(0.1, 0.1), Okular::NormalizedPoint(0.5, 0.1), Okular::NormalizedPoint(0.3, 0.4)});
    polygon->setLineClosed(true);
    polygon->setLineInnerColor(QColor(0, 0, 0, 0));
    polygon->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.5, 0.4));
    QDomDocument polygonDocument;
    QDomElement polygonElement = polygonDocument.createElement(QStringLiteral("annotation"));
    Okular::AnnotationUtils::storeAnnotation(polygon, polygonElement, polygonDocument);
    const QDomElement lineElement = polygonElement.firstChildElement(QStringLiteral("line"));
    QCOMPARE(lineElement.attribute(QStringLiteral("innerColor")), QStringLiteral("#00000000"));
    Okular::Annotation *restoredPolygon = Okular::AnnotationUtils::createAnnotation(polygonElement);
    QVERIFY(restoredPolygon);
    auto *restoredLine = static_cast<Okular::LineAnnotation *>(restoredPolygon);
    QVERIFY(restoredLine->lineInnerColor().isValid());
    QCOMPARE(restoredLine->lineInnerColor().alpha(), 0);
    QVERIFY(restoredLine->lineClosed());
    delete restoredPolygon;
    delete polygon;
}

void AnnotationRotationTest::testRotatedHitTestUsesOutline()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.5), Okular::NormalizedPoint(0.6, 0.5)}, Okular::NormalizedRect(0.4, 0.5, 0.6, 0.5));
    Okular::AnnotationFrame frame;
    frame.center = Okular::NormalizedPoint(0.5, 0.5);
    frame.size = QSizeF(0.2, 0.2);
    frame.rotation = qDegreesToRadians(45.0);
    QVERIFY(line->setFrame(frame));

    const double width = m_page->width();
    const double height = m_page->height();
    Okular::AnnotationObjectRect objectRect(line);
    QVERIFY(objectRect.contains(0.5, 0.5, width, height));
    // The AABB corner is outside the diamond-shaped rotated outline.
    const Okular::NormalizedRect aabb = line->boundingRectangle();
    QVERIFY(!objectRect.contains(aabb.left + 0.001, aabb.top + 0.001, width, height));
}

void AnnotationRotationTest::testLegacySetterResetsWithWarning()
{
    auto *line = addLine({Okular::NormalizedPoint(0.4, 0.5), Okular::NormalizedPoint(0.6, 0.5)}, Okular::NormalizedRect(0.4, 0.5, 0.6, 0.5));
    Okular::AnnotationFrame frame;
    frame.center = Okular::NormalizedPoint(0.5, 0.5);
    frame.size = QSizeF(0.2, 0.1);
    frame.rotation = qDegreesToRadians(20.0);
    QVERIFY(line->setFrame(frame));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral(".*rotated frame.*")));
    line->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.3, 0.3));
    QVERIFY2(qFuzzyIsNull(line->frame().rotation), "legacy setter is explicitly an unrotated initialization");
    QVERIFY2(std::fabs(line->frame().size.width() - 0.2) < tolerance, "legacy setter replaces the frame size");
}

QTEST_MAIN(AnnotationRotationTest)

#include "annotationrotationtest.moc"
