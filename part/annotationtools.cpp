/*
    SPDX-FileCopyrightText: 2005 Enrico Ros <eros.kde@email.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationtools.h"

#include <cmath>

// qt / kde includes
#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>
#include <QVector>

// local includes
#include "core/annotations.h"
#include "core/page.h"

AnnotatorEngine::AnnotatorEngine(const QDomElement &engineElement)
    : m_engineElement(engineElement)
    , m_creationCompleted(false)
    , m_cursor(Qt::CrossCursor)
    , m_item(nullptr)
{
    // parse common engine attributes
    if (engineElement.hasAttribute(QStringLiteral("color"))) {
        m_engineColor = QColor(engineElement.attribute(QStringLiteral("color")));
    }

    // get the annotation element
    QDomElement annElement = m_engineElement.firstChild().toElement();
    if (!annElement.isNull() && annElement.tagName() == QLatin1String("annotation")) {
        m_annotElement = annElement;
    }
}

void AnnotatorEngine::decodeEvent(const QMouseEvent *mouseEvent, EventType *eventType, Button *button)
{
    *eventType = AnnotatorEngine::Press;
    if (mouseEvent->type() == QEvent::MouseMove) {
        *eventType = AnnotatorEngine::Move;
    } else if (mouseEvent->type() == QEvent::MouseButtonRelease) {
        *eventType = AnnotatorEngine::Release;
    }

    *button = AnnotatorEngine::None;
    const Qt::MouseButtons buttonState = (*eventType == AnnotatorEngine::Move) ? mouseEvent->buttons() : mouseEvent->button();
    if (buttonState == Qt::LeftButton) {
        *button = AnnotatorEngine::Left;
    } else if (buttonState == Qt::RightButton) {
        *button = AnnotatorEngine::Right;
    }
}

void AnnotatorEngine::decodeEvent(const QTabletEvent *tabletEvent, EventType *eventType, Button *button)
{
    qreal pressure = 1.0;
    decodeEvent(tabletEvent, eventType, button, &pressure);
}

void AnnotatorEngine::decodeEvent(const QTabletEvent *tabletEvent, EventType *eventType, Button *button, qreal *pressure)
{
    switch (tabletEvent->type()) {
    case QEvent::TabletPress:
        *button = AnnotatorEngine::Left;
        *eventType = AnnotatorEngine::Press;
        break;
    case QEvent::TabletRelease:
        *button = AnnotatorEngine::Left;
        *eventType = AnnotatorEngine::Release;
        break;
    case QEvent::TabletMove:
        *button = AnnotatorEngine::Left;
        *eventType = AnnotatorEngine::Move;
        break;
    default:
        Q_ASSERT(false);
        break;
    }

    if (pressure) {
        // Match Drawy's pressure response curve while keeping the value bounded.
        *pressure = qBound(0.0, tabletEvent->pressure() / 1.60 + 0.375, 1.0);
    }
}

AnnotatorEngine::~AnnotatorEngine()
{
}

QCursor AnnotatorEngine::cursor() const
{
    return m_cursor;
}

void AnnotatorEngine::setCursor(const QCursor &cursor)
{
    m_cursor = cursor;
}

SmoothPath::SmoothPath(const QList<Okular::NormalizedPoint> &points,
                       const QList<qreal> &segmentWidths,
                       qreal pageWidth,
                       const QPen &pen,
                       qreal opacity,
                       QPainter::CompositionMode compositionMode)
    : points(points)
    , segmentWidths(segmentWidths)
    , pageWidth(pageWidth)
    , pen(pen)
    , opacity(opacity)
    , compositionMode(compositionMode)
{
}

QPainterPath FreehandStrokeGeometry::segmentOutline(const QPointF &start, const QPointF &end, qreal width)
{
    QPainterPath outline;
    if (width <= 0.0) {
        return outline;
    }

    const qreal radius = width / 2.0;
    const QPointF direction = end - start;
    const qreal length = std::hypot(direction.x(), direction.y());
    if (qFuzzyIsNull(length)) {
        outline.addEllipse(QRectF(start.x() - radius, start.y() - radius, width, width));
        return outline;
    }

    const QPointF tangent(direction.x() / length, direction.y() / length);
    const QPointF normal(-tangent.y() * radius, tangent.x() * radius);
    const qreal controlOffset = radius * 4.0 / 3.0;
    const QPointF startLeft = start + normal;
    const QPointF endLeft = end + normal;
    const QPointF endRight = end - normal;
    const QPointF startRight = start - normal;

    outline.moveTo(startLeft);
    outline.lineTo(endLeft);
    outline.cubicTo(endLeft + tangent * controlOffset, endRight + tangent * controlOffset, endRight);
    outline.lineTo(startRight);
    outline.cubicTo(startRight - tangent * controlOffset, startLeft - tangent * controlOffset, startLeft);
    outline.closeSubpath();
    return outline;
}

static QByteArray pdfNumber(qreal value)
{
    if (qFuzzyIsNull(value)) {
        value = 0.0;
    }
    QByteArray number = QByteArray::number(value, 'f', 6);
    while (number.endsWith('0')) {
        number.chop(1);
    }
    if (number.endsWith('.')) {
        number.chop(1);
    }
    return number;
}

QByteArray FreehandStrokeGeometry::pdfAppearance(const QList<QPainterPath> &outlines, const QColor &color)
{
    QByteArray appearance = "q\n";
    appearance += pdfNumber(color.redF()) + ' ' + pdfNumber(color.greenF()) + ' ' + pdfNumber(color.blueF()) + " rg\n";

    for (const QPainterPath &outline : outlines) {
        for (int index = 0; index < outline.elementCount(); ++index) {
            const QPainterPath::Element element = outline.elementAt(index);
            if (element.type == QPainterPath::MoveToElement) {
                appearance += pdfNumber(element.x) + ' ' + pdfNumber(element.y) + " m\n";
            } else if (element.type == QPainterPath::LineToElement) {
                appearance += pdfNumber(element.x) + ' ' + pdfNumber(element.y) + " l\n";
            } else if (element.type == QPainterPath::CurveToElement && index + 2 < outline.elementCount()) {
                const QPainterPath::Element control2 = outline.elementAt(index + 1);
                const QPainterPath::Element endPoint = outline.elementAt(index + 2);
                appearance += pdfNumber(element.x) + ' ' + pdfNumber(element.y) + ' ' + pdfNumber(control2.x) + ' ' + pdfNumber(control2.y) + ' ' + pdfNumber(endPoint.x) + ' '
                    + pdfNumber(endPoint.y) + " c\n";
                index += 2;
            }
        }
        appearance += "h\nf\n";
    }

    appearance += "Q\n";
    return appearance;
}

/** SmoothPathEngine */
SmoothPathEngine::SmoothPathEngine(const QDomElement &engineElement)
    : AnnotatorEngine(engineElement)
    , compositionMode(QPainter::CompositionMode_SourceOver)
{
    // parse engine specific attributes
    if (engineElement.attribute(QStringLiteral("compositionMode"), QStringLiteral("sourceOver")) == QLatin1String("clear")) {
        compositionMode = QPainter::CompositionMode_Clear;
    }
}

QRect SmoothPathEngine::event(EventType type, Button button, Modifiers modifiers, double nX, double nY, double xScale, double yScale, const Okular::Page *page)
{
    const qreal basePenWidth = m_annotElement.attribute(QStringLiteral("width"), QStringLiteral("1")).toDouble();
    if (page) {
        m_pageWidth = page->width();
        m_pageHeight = page->height();
    }
    m_lastXScale = xScale;
    if (button != Left) {
        return QRect();
    }

    if (type == Press && points.isEmpty()) {
        const qreal pressure = qBound(0.0, modifiers.pressure, 1.0);
        m_liveStroke.clear();
        segmentWidths.clear();
        m_pressureSum = 0.0;
        m_allDefaultPressure = true;
        smoothingWindow.clear();
        pressureWindow.clear();
        smoothingSum = {};
        lastPoint = processPoint(Okular::NormalizedPoint(nX, nY), pressure);
        const qreal processedPressure = pressureWindow.last();
        totalRect.left = totalRect.right = lastPoint.x;
        totalRect.top = totalRect.bottom = lastPoint.y;
        points.append(lastPoint);
        pressures.append(processedPressure);
        m_pressureSum += processedPressure;
        m_allDefaultPressure = m_allDefaultPressure && qFuzzyCompare(processedPressure, 1.0);
        return {};
    } else if (type == Move && points.count() > 0) {
        const qreal pressure = qBound(0.0, modifiers.pressure, 1.0);
        const Okular::NormalizedPoint previousPoint = lastPoint;
        const Okular::NormalizedPoint nextPoint = processPoint(Okular::NormalizedPoint(nX, nY), pressure);
        if (qFuzzyIsNull(nextPoint.x - previousPoint.x) && qFuzzyIsNull(nextPoint.y - previousPoint.y)) {
            return {};
        }
        points.append(nextPoint);
        const qreal processedPressure = pressureWindow.last();
        pressures.append(processedPressure);
        m_pressureSum += processedPressure;
        m_allDefaultPressure = m_allDefaultPressure && qFuzzyCompare(processedPressure, 1.0);

        totalRect.left = qMin(totalRect.left, nX);
        totalRect.top = qMin(totalRect.top, nY);
        totalRect.right = qMax(nX, totalRect.right);
        totalRect.bottom = qMax(nY, totalRect.bottom);

        lastPoint = nextPoint;
        const qreal representativeWidth = representativeStrokeWidth(m_pressureSum, pressures.size(), m_allDefaultPressure, basePenWidth);
        const qreal pageWidth = qMax<qreal>(1.0, m_pageWidth);
        const qreal segmentPixelWidth = qBound<qreal>(0.5, processedPressure * representativeWidth * xScale / pageWidth, 100.0);
        const qreal segmentWidth = segmentPixelWidth * pageWidth / xScale;
        segmentWidths.append(segmentWidth);
        QPen pen(m_engineColor, segmentPixelWidth);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        const QRect segmentRect = m_liveStroke.addSegment(QPointF(previousPoint.x * xScale, previousPoint.y * yScale), QPointF(nextPoint.x * xScale, nextPoint.y * yScale), pen,
                                   m_annotElement.attribute(QStringLiteral("opacity"), QStringLiteral("1.0")).toDouble());
        return segmentRect;
    } else if (type == Release && points.count() > 0) {
        if (points.count() < 2) {
            const qreal representativeWidth = representativeStrokeWidth(m_pressureSum, pressures.size(), m_allDefaultPressure, basePenWidth);
            const qreal pageWidth = qMax<qreal>(1.0, m_pageWidth);
            const qreal segmentPixelWidth = qBound<qreal>(0.5, pressures.last() * representativeWidth * xScale / pageWidth, 100.0);
            segmentWidths.append(segmentPixelWidth * pageWidth / xScale);
            points.append(lastPoint);
            pressures.append(pressures.last());
        }
        m_creationCompleted = true;
        return totalRect.geometry((int)xScale, (int)yScale);
    }
    return QRect();
}

void SmoothPathEngine::paint(QPainter *painter, double xScale, double yScale, const QRect &clipRect)
{
    Q_UNUSED(xScale);
    Q_UNUSED(yScale);
    m_liveStroke.paint(painter, clipRect);
}

void SmoothPath::paint(QPainter *painter, double xScale, double yScale) const
{
    if (points.count() > 1) {
        painter->setCompositionMode(compositionMode);
        painter->setOpacity(opacity);
        painter->setRenderHint(QPainter::Antialiasing, true);

        for (int index = 1; index < points.size(); ++index) {
            const qreal width = qBound(0.5, segmentWidths.value(index - 1, pen.widthF()) * xScale / pageWidth, 100.0);
            const QPainterPath outline = FreehandStrokeGeometry::segmentOutline(QPointF(points[index - 1].x * xScale, points[index - 1].y * yScale),
                                                                                QPointF(points[index].x * xScale, points[index].y * yScale),
                                                                                width);
            painter->fillPath(outline, pen.brush());
        }
    }
}

QRect LiveStroke::addSegment(const QPointF &start, const QPointF &end, const QPen &pen, qreal opacity)
{
    const QPainterPath outline = FreehandStrokeGeometry::segmentOutline(start, end, pen.widthF());
    const QRect dirtyRect = outline.boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect();
    const int firstTileX = qFloor(static_cast<qreal>(dirtyRect.left()) / tileSize);
    const int lastTileX = qFloor(static_cast<qreal>(dirtyRect.right()) / tileSize);
    const int firstTileY = qFloor(static_cast<qreal>(dirtyRect.top()) / tileSize);
    const int lastTileY = qFloor(static_cast<qreal>(dirtyRect.bottom()) / tileSize);

    for (int tileY = firstTileY; tileY <= lastTileY; ++tileY) {
        for (int tileX = firstTileX; tileX <= lastTileX; ++tileX) {
            const QPoint key(tileX, tileY);
            QImage &image = m_tiles[key];
            if (image.isNull()) {
                image = QImage(tileSize, tileSize, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
            }

            const QPoint origin(tileX * tileSize, tileY * tileSize);
            QPainter tilePainter(&image);
            tilePainter.setRenderHint(QPainter::Antialiasing, true);
            tilePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            tilePainter.setOpacity(opacity);
            tilePainter.fillPath(outline.translated(-origin), pen.brush());
        }
    }

    m_bounds = m_bounds.isNull() ? dirtyRect : m_bounds.united(dirtyRect);
    return dirtyRect;
}

void LiveStroke::paint(QPainter *painter, const QRect &clipRect) const
{
    if (m_tiles.isEmpty() || !m_bounds.intersects(clipRect)) {
        return;
    }

    const int firstTileX = qFloor(static_cast<qreal>(clipRect.left()) / tileSize);
    const int lastTileX = qFloor(static_cast<qreal>(clipRect.right()) / tileSize);
    const int firstTileY = qFloor(static_cast<qreal>(clipRect.top()) / tileSize);
    const int lastTileY = qFloor(static_cast<qreal>(clipRect.bottom()) / tileSize);
    for (int tileY = firstTileY; tileY <= lastTileY; ++tileY) {
        for (int tileX = firstTileX; tileX <= lastTileX; ++tileX) {
            const QPoint key(tileX, tileY);
            const auto image = m_tiles.constFind(key);
            if (image != m_tiles.cend()) {
                painter->drawImage(QPoint(tileX * tileSize, tileY * tileSize), image.value());
            }
        }
    }
}

void LiveStroke::clear()
{
    m_tiles.clear();
    m_bounds = {};
}

QRect LiveStroke::bounds() const
{
    return m_bounds;
}

QRect LiveStroke::tileRect(const QPoint &tile)
{
    return {tile.x() * tileSize, tile.y() * tileSize, tileSize, tileSize};
}

QList<Okular::Annotation *> SmoothPathEngine::end()
{
    m_creationCompleted = false;

    if (m_annotElement.isNull()) {
        return QList<Okular::Annotation *>();
    }

    Okular::Annotation *ann = nullptr;
    const QString typeString = m_annotElement.attribute(QStringLiteral("type"));

    if (typeString == QLatin1String("Ink")) {
        Okular::InkAnnotation *ia = new Okular::InkAnnotation();
        ann = ia;
        if (m_annotElement.hasAttribute(QStringLiteral("width"))) {
            ann->style().setWidth(m_annotElement.attribute(QStringLiteral("width")).toDouble());
        }

        QList<QList<Okular::NormalizedPoint>> list = ia->inkPaths();
        list.append(points);
        ia->setInkPaths(list);
        ia->setSessionPressures({pressures}, representativeStrokeWidth(pressures, ia->style().width()));
        ia->setBoundingRectangle(totalRect);
    }

    if (!ann) {
        return QList<Okular::Annotation *>();
    }

    ann->style().setColor(m_annotElement.hasAttribute(QStringLiteral("color")) ? m_annotElement.attribute(QStringLiteral("color")) : m_engineColor);
    if (m_annotElement.hasAttribute(QStringLiteral("opacity"))) {
        ann->style().setOpacity(m_annotElement.attribute(QStringLiteral("opacity"), QStringLiteral("1.0")).toDouble());
    }

    if (auto *ink = dynamic_cast<Okular::InkAnnotation *>(ann); ink && compositionMode == QPainter::CompositionMode_SourceOver && segmentWidths.size() == points.size() - 1) {
        const qreal pageWidth = qMax<qreal>(1.0, m_pageWidth);
        const qreal pageHeight = qMax<qreal>(1.0, m_pageHeight);
        QList<QPainterPath> outlines;
        QRectF bounds;
        for (int index = 1; index < points.size(); ++index) {
            const QPointF start(points[index - 1].x * pageWidth, (1.0 - points[index - 1].y) * pageHeight);
            const QPointF end(points[index].x * pageWidth, (1.0 - points[index].y) * pageHeight);
            const QPainterPath outline = FreehandStrokeGeometry::segmentOutline(start, end, segmentWidths[index - 1]);
            outlines.append(outline);
            bounds = bounds.isNull() ? outline.boundingRect() : bounds.united(outline.boundingRect());
        }

        if (!outlines.isEmpty() && bounds.isValid()) {
            bounds.adjust(-0.25, -0.25, 0.25, 0.25);
            QList<QPainterPath> localOutlines;
            localOutlines.reserve(outlines.size());
            for (const QPainterPath &outline : outlines) {
                localOutlines.append(outline.translated(-bounds.left(), -bounds.top()));
            }

            const double left = bounds.left() / pageWidth;
            const double top = 1.0 - bounds.bottom() / pageHeight;
            const double right = bounds.right() / pageWidth;
            const double bottom = 1.0 - bounds.top() / pageHeight;
            ink->setBoundingRectangle(Okular::NormalizedRect(left, top, right, bottom));
            ink->setAppearanceStream(FreehandStrokeGeometry::pdfAppearance(localOutlines, ink->style().color()), QRectF(0, 0, bounds.width(), bounds.height()));
        }
    }

    return {ann};
}

SmoothPath SmoothPathEngine::endSmoothPath()
{
    m_creationCompleted = false;

    QColor color(m_annotElement.hasAttribute(QStringLiteral("color")) ? m_annotElement.attribute(QStringLiteral("color")) : m_engineColor);

    const int width = m_annotElement.attribute(QStringLiteral("width"), QStringLiteral("2")).toInt();
    const qreal opacity = m_annotElement.attribute(QStringLiteral("opacity"), QStringLiteral("1.0")).toDouble();

    return SmoothPath(points, segmentWidths, m_pageWidth, QPen(color, representativeStrokeWidth(pressures, width) * m_lastXScale / m_pageWidth), opacity, compositionMode);
}

QRect SmoothPathEngine::liveBounds() const
{
    return m_liveStroke.bounds();
}

Okular::NormalizedPoint SmoothPathEngine::processPoint(const Okular::NormalizedPoint &point, qreal pressure)
{
    constexpr int smoothingWindowSize = 7;
    smoothingWindow.append(point);
    smoothingSum.x += point.x;
    smoothingSum.y += point.y;
    pressureWindow.append(pressure);
    if (smoothingWindow.size() > smoothingWindowSize) {
        smoothingSum.x -= smoothingWindow.first().x;
        smoothingSum.y -= smoothingWindow.first().y;
        smoothingWindow.removeFirst();
        pressureWindow.removeFirst();
    }

    const qreal count = smoothingWindow.size();
    return Okular::NormalizedPoint(smoothingSum.x / count, smoothingSum.y / count);
}

qreal SmoothPathEngine::representativeStrokeWidth(const QList<qreal> &pressures, qreal baseWidth)
{
    if (pressures.isEmpty()) {
        return baseWidth;
    }

    double sum = 0.0;
    bool constantMousePressure = true;
    for (const qreal pressure : pressures) {
        sum += pressure;
        constantMousePressure = constantMousePressure && qFuzzyCompare(pressure, 1.0);
    }
    if (constantMousePressure) {
        return baseWidth;
    }
    return representativeStrokeWidth(sum, pressures.size(), constantMousePressure, baseWidth);
}

qreal SmoothPathEngine::representativeStrokeWidth(qreal pressureSum, qsizetype pressureCount, bool allDefaultPressure, qreal baseWidth)
{
    if (pressureCount <= 0 || allDefaultPressure) {
        return baseWidth;
    }
    const qreal averagePressure = pressureSum / pressureCount;
    return qBound(0.5, baseWidth * (0.75 + averagePressure), 12.0);
}
