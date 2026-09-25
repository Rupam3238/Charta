/*
    SPDX-FileCopyrightText: 2005 Enrico Ros <eros.kde@email.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationtools.h"

// qt / kde includes
#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>
#include <QVector>

// local includes
#include "core/annotations.h"

AnnotatorEngine::AnnotatorEngine(const QDomElement &engineElement)
    : m_engineElement(engineElement)
    , m_creationCompleted(false)
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
    return Qt::CrossCursor;
}

SmoothPath::SmoothPath(const QList<Okular::NormalizedPoint> &points, const QList<qreal> &pressures, const QPen &pen, qreal opacity, QPainter::CompositionMode compositionMode)
    : points(points)
    , pressures(pressures)
    , pen(pen)
    , opacity(opacity)
    , compositionMode(compositionMode)
{
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

QRect SmoothPathEngine::event(EventType type, Button button, Modifiers modifiers, double nX, double nY, double xScale, double yScale, const Okular::Page * /*page*/)
{
    const qreal basePenWidth = m_annotElement.attribute(QStringLiteral("width"), QStringLiteral("1")).toDouble();
    if (button != Left) {
        return QRect();
    }

    if (type == Press && points.isEmpty()) {
        const qreal pressure = qBound(0.0, modifiers.pressure, 1.0);
        lastPoint = processPoint(Okular::NormalizedPoint(nX, nY), pressure);
        totalRect.left = totalRect.right = lastPoint.x;
        totalRect.top = totalRect.bottom = lastPoint.y;
        points.append(lastPoint);
        pressures.append(pressureWindow.last());
    } else if (type == Move && points.count() > 0) {
        const qreal pressure = qBound(0.0, modifiers.pressure, 1.0);
        const Okular::NormalizedPoint nextPoint = processPoint(Okular::NormalizedPoint(nX, nY), pressure);
        points.append(nextPoint);
        pressures.append(pressureWindow.last());

        totalRect.left = qMin(totalRect.left, nX);
        totalRect.top = qMin(totalRect.top, nY);
        totalRect.right = qMax(nX, totalRect.right);
        totalRect.bottom = qMax(nY, totalRect.bottom);

        Okular::NormalizedRect incrementalRect;
        incrementalRect.left = qMin(nextPoint.x, lastPoint.x);
        incrementalRect.right = qMax(nextPoint.x, lastPoint.x);
        incrementalRect.top = qMin(nextPoint.y, lastPoint.y);
        incrementalRect.bottom = qMax(nextPoint.y, lastPoint.y);
        lastPoint = nextPoint;
        const qreal radius = qMax<qreal>(1.0, pressures.last() * basePenWidth * 2.0);
        return incrementalRect.geometry((int)xScale, (int)yScale).adjusted(-qCeil(radius), -qCeil(radius), qCeil(radius), qCeil(radius));
    } else if (type == Release && points.count() > 0) {
        if (points.count() < 2) {
            points.clear();
            pressures.clear();
        } else {
            m_creationCompleted = true;
        }
        return totalRect.geometry((int)xScale, (int)yScale);
    }
    return QRect();
}

void SmoothPathEngine::paint(QPainter *painter, double xScale, double yScale, const QRect & /*clipRect*/)
{
    const double penWidth = m_annotElement.attribute(QStringLiteral("width"), QStringLiteral("1")).toInt();
    const qreal opacity = m_annotElement.attribute(QStringLiteral("opacity"), QStringLiteral("1.0")).toDouble();

    const QList<Okular::NormalizedPoint> &processedPoints = points;
    const QList<qreal> &processedPressures = pressures;
    QPen pen(m_engineColor, representativeStrokeWidth(processedPressures, penWidth));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setColor(m_engineColor);

    const SmoothPath path(processedPoints, processedPressures, pen, opacity, compositionMode);
    path.paint(painter, xScale, yScale);
}

void SmoothPath::paint(QPainter *painter, double xScale, double yScale) const
{
    if (points.count() > 1) {
        painter->setCompositionMode(compositionMode);
        painter->setOpacity(opacity);
        painter->setRenderHint(QPainter::Antialiasing, true);

        for (int index = 1; index < points.size(); ++index) {
            const qreal pressure = pressures.value(index, 1.0);
            QPen segmentPen = pen;
            segmentPen.setWidthF(qBound(0.5, pressure * pen.widthF(), 100.0));
            painter->setPen(segmentPen);
            painter->drawLine(QPointF(points[index - 1].x * xScale, points[index - 1].y * yScale),
                              QPointF(points[index].x * xScale, points[index].y * yScale));
        }
    }
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

    return {ann};
}

SmoothPath SmoothPathEngine::endSmoothPath()
{
    m_creationCompleted = false;

    QColor color(m_annotElement.hasAttribute(QStringLiteral("color")) ? m_annotElement.attribute(QStringLiteral("color")) : m_engineColor);

    const int width = m_annotElement.attribute(QStringLiteral("width"), QStringLiteral("2")).toInt();
    const qreal opacity = m_annotElement.attribute(QStringLiteral("opacity"), QStringLiteral("1.0")).toDouble();

    return SmoothPath(points, pressures, QPen(color, width), opacity, compositionMode);
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
    const qreal averagePressure = sum / pressures.size();
    return qBound(0.5, baseWidth * (0.75 + averagePressure), 12.0);
}
