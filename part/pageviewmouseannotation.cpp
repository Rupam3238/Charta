/*
    SPDX-FileCopyrightText: 2017 Tobias Deiminger <haxtibal@t-online.de>
    SPDX-FileCopyrightText: 2004-2005 Enrico Ros <eros.kde@email.it>
    SPDX-FileCopyrightText: 2004-2006 Albert Astals Cid <aacid@kde.org>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    With portions of code from kpdf/kpdf_pagewidget.cc by:
    SPDX-FileCopyrightText: 2002 Wilco Greven <greven@kde.org>
    SPDX-FileCopyrightText: 2003 Christophe Devriese <Christophe.Devriese@student.kuleuven.ac.be>
    SPDX-FileCopyrightText: 2003 Laurent Montel <montel@kde.org>
    SPDX-FileCopyrightText: 2003 Dirk Mueller <mueller@kde.org>
    SPDX-FileCopyrightText: 2004 James Ots <kde@jamesots.com>
    SPDX-FileCopyrightText: 2011 Jiri Baum - NICTA <jiri@baum.com.au>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pageviewmouseannotation.h"

#include <qevent.h>
#include <qpainter.h>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>
#include <qtooltip.h>

#include <cmath>
#include <limits>

#include "core/document.h"
#include "core/page.h"
#include "gui/guiutils.h"
#include "pageview.h"
#include "widthpresetutils.h"
#include "videowidget.h"

static const int handleSize = 10;
static const int handleLongSize = 20;
static const int handleShortSize = 6;
static const int handlePadding = handleLongSize / 2;
static const int handleHitExpansion = 4;
static const int rotateHandleOffset = 12;
static const int rotateHandleSize = 14;

static QTransform pageRotationMatrixFor(Okular::Rotation rotation)
{
    QTransform matrix;
    matrix.rotate(static_cast<int>(rotation) * 90);
    switch (rotation) {
    case Okular::Rotation90:
        matrix.translate(0, -1);
        break;
    case Okular::Rotation180:
        matrix.translate(-1, -1);
        break;
    case Okular::Rotation270:
        matrix.translate(-1, 0);
        break;
    default:
        break;
    }
    return matrix;
}

static double normalizedAngleDelta(double delta)
{
    while (delta > M_PI) {
        delta -= 2.0 * M_PI;
    }
    while (delta <= -M_PI) {
        delta += 2.0 * M_PI;
    }
    return delta;
}

static QCursor rotationCursor()
{
    static QCursor cursor;
    if (!cursor.pixmap().isNull()) {
        return cursor;
    }

    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF arcRect(4.5, 4.5, 15.0, 15.0);
    painter.setPen(QPen(Qt::white, 4.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, 60 * 16, 270 * 16);
    painter.setPen(QPen(QColor(35, 35, 35), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, 60 * 16, 270 * 16);

    QPainterPath arrow;
    arrow.moveTo(18.5, 4.0);
    arrow.lineTo(21.0, 9.0);
    arrow.lineTo(15.5, 8.0);
    arrow.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(35, 35, 35));
    painter.drawPath(arrow);
    painter.end();

    cursor = QCursor(pixmap, 12, 12);
    return cursor;
}
static QPointF annotationPagePoint(const Okular::NormalizedPoint &point, double width, double height)
{
    return QPointF(point.x * width, point.y * height);
}

static QPainterPath annotationLassoGeometry(const Okular::Annotation *annotation, PageViewItem *pageViewItem)
{
    const double width = pageViewItem->uncroppedWidth();
    const double height = pageViewItem->uncroppedHeight();
    const QRect bounds = Okular::AnnotationUtils::annotationGeometry(annotation, width, height).normalized();
    QPainterPath geometry;
    qreal strokeWidth = qMax<qreal>(1.0, annotation->style().width() * width / pageViewItem->page()->width());

    switch (annotation->subType()) {
    case Okular::Annotation::AInk: {
        const auto *ink = static_cast<const Okular::InkAnnotation *>(annotation);
        const auto paths = ink->transformedInkPaths();
        const auto pressures = ink->sessionPressures();
        const qreal pressureStrokeWidth = ink->sessionPressureStrokeWidth() > 0.0 ? ink->sessionPressureStrokeWidth() : annotation->style().width();
        for (qsizetype pathIndex = 0; pathIndex < paths.size(); ++pathIndex) {
            const auto &points = paths.at(pathIndex);
            if (points.isEmpty()) {
                continue;
            }
            if (points.size() == 1) {
                const QPointF point = annotationPagePoint(points.constFirst(), width, height);
                geometry.addEllipse(point, strokeWidth / 2.0, strokeWidth / 2.0);
                continue;
            }
            QPainterPath centerline;
            centerline.moveTo(annotationPagePoint(points.constFirst(), width, height));
            for (qsizetype pointIndex = 1; pointIndex < points.size(); ++pointIndex) {
                centerline.lineTo(annotationPagePoint(points.at(pointIndex), width, height));
            }
            const auto pressurePath = pressures.value(pathIndex);
            if (pressurePath.size() == points.size() && pressurePath.size() > 1) {
                for (qsizetype pointIndex = 1; pointIndex < points.size(); ++pointIndex) {
                    QPainterPath segment;
                    segment.moveTo(annotationPagePoint(points.at(pointIndex - 1), width, height));
                    segment.lineTo(annotationPagePoint(points.at(pointIndex), width, height));
                    QPainterPathStroker stroker;
                    stroker.setWidth(qMax<qreal>(0.5, pressurePath.at(pointIndex) * pressureStrokeWidth * width / pageViewItem->page()->width()));
                    stroker.setCapStyle(Qt::RoundCap);
                    geometry = geometry.united(stroker.createStroke(segment));
                }
            } else {
                QPainterPathStroker stroker;
                stroker.setWidth(strokeWidth);
                stroker.setCapStyle(Qt::RoundCap);
                stroker.setJoinStyle(Qt::RoundJoin);
                geometry = geometry.united(stroker.createStroke(centerline));
            }
        }
        return geometry;
    }
    case Okular::Annotation::ALine: {
        const auto *line = static_cast<const Okular::LineAnnotation *>(annotation);
        const auto points = line->transformedLinePoints();
        if (!points.isEmpty()) {
            QPainterPath centerline;
            centerline.moveTo(annotationPagePoint(points.constFirst(), width, height));
            for (qsizetype pointIndex = 1; pointIndex < points.size(); ++pointIndex) {
                centerline.lineTo(annotationPagePoint(points.at(pointIndex), width, height));
            }
            if (line->lineClosed()) {
                centerline.closeSubpath();
            }
            if (points.size() == 2) {
                const QPointF start = annotationPagePoint(points.constFirst(), width, height);
                const QPointF end = annotationPagePoint(points.constLast(), width, height);
                const QPointF delta = end - start;
                const qreal length = std::hypot(delta.x(), delta.y());
                if (length > 0.0) {
                    const QPointF direction = delta / length;
                    const QPointF perpendicular(-direction.y(), direction.x());
                    const qreal pageScale = width / pageViewItem->page()->width();
                    const qreal size = qMin<qreal>(6.0 * line->style().width() * pageScale, length / 2.0);
                    const auto addEndStyle = [&centerline, &direction, &perpendicular, size](QPointF tip, Okular::LineAnnotation::TermStyle style, qreal insideDirection) {
                        const QPointF along = direction * insideDirection;
                        switch (style) {
                        case Okular::LineAnnotation::Square: {
                            const QPointF base = tip + along * size;
                            centerline.moveTo(tip + perpendicular * (size / 2.0));
                            centerline.lineTo(base + perpendicular * (size / 2.0));
                            centerline.lineTo(base - perpendicular * (size / 2.0));
                            centerline.lineTo(tip - perpendicular * (size / 2.0));
                            centerline.closeSubpath();
                            break;
                        }
                        case Okular::LineAnnotation::Circle:
                            centerline.addEllipse(tip + along * (size / 2.0), size / 2.0, size / 2.0);
                            break;
                        case Okular::LineAnnotation::Diamond: {
                            const QPointF middle = tip + along * (size / 2.0);
                            const QPointF far = tip + along * size;
                            centerline.moveTo(tip);
                            centerline.lineTo(middle + perpendicular * (size / 2.0));
                            centerline.lineTo(far);
                            centerline.lineTo(middle - perpendicular * (size / 2.0));
                            centerline.closeSubpath();
                            break;
                        }
                        case Okular::LineAnnotation::OpenArrow:
                        case Okular::LineAnnotation::ROpenArrow:
                        case Okular::LineAnnotation::ClosedArrow:
                        case Okular::LineAnnotation::RClosedArrow: {
                            const QPointF base = tip + along * size;
                            centerline.moveTo(base + perpendicular * (size / 2.0));
                            centerline.lineTo(tip);
                            centerline.lineTo(base - perpendicular * (size / 2.0));
                            if (style == Okular::LineAnnotation::ClosedArrow || style == Okular::LineAnnotation::RClosedArrow) {
                                centerline.closeSubpath();
                            }
                            break;
                        }
                        case Okular::LineAnnotation::Butt:
                            centerline.moveTo(tip + perpendicular * (size / 2.0));
                            centerline.lineTo(tip - perpendicular * (size / 2.0));
                            break;
                        case Okular::LineAnnotation::Slash: {
                            const QPointF offset = along * (std::cos(M_PI / 3.0) * size / 2.0);
                            centerline.moveTo(tip + offset + perpendicular * (size / 2.0));
                            centerline.lineTo(tip - offset - perpendicular * (size / 2.0));
                            break;
                        }
                        case Okular::LineAnnotation::None:
                            break;
                        }
                    };
                    addEndStyle(start, line->lineStartStyle(), 1.0);
                    const qreal endDirection = line->lineEndStyle() == Okular::LineAnnotation::ROpenArrow || line->lineEndStyle() == Okular::LineAnnotation::RClosedArrow ? 1.0 : -1.0;
                    addEndStyle(end, line->lineEndStyle(), endDirection);
                }
            }
            if (line->lineInnerColor().isValid()) {
                geometry = centerline;
            }
            QPainterPathStroker stroker;
            stroker.setWidth(strokeWidth);
            stroker.setCapStyle(Qt::SquareCap);
            stroker.setJoinStyle(Qt::MiterJoin);
            geometry = geometry.united(stroker.createStroke(centerline));
            return geometry;
        }
        break;
    }
    case Okular::Annotation::AGeom: {
        const auto *shape = static_cast<const Okular::GeomAnnotation *>(annotation);
        if (!qFuzzyIsNull(annotation->frame().rotation)) {
            const QSizeF localSize = Okular::AnnotationUtils::annotationFrameSizeInPagePoints(annotation);
            if (localSize.width() <= 0 || localSize.height() <= 0) {
                break;
            }
            QPainterPath localPath;
            const QRectF localRect(0, 0, localSize.width(), localSize.height());
            if (shape->geometricalType() == Okular::GeomAnnotation::InscribedCircle) {
                localPath.addEllipse(localRect);
            } else {
                localPath.addRect(localRect);
            }
            geometry = Okular::AnnotationUtils::annotationFrameTransform(annotation, width, height).map(localPath);
            if (!shape->geometricalInnerColor().isValid()) {
                QPainterPathStroker stroker;
                stroker.setWidth(strokeWidth);
                geometry = stroker.createStroke(geometry);
            }
            return geometry;
        }

        if (shape->geometricalType() == Okular::GeomAnnotation::InscribedCircle) {
            geometry.addEllipse(bounds);
        } else {
            geometry.addRect(bounds);
        }
        if (!shape->geometricalInnerColor().isValid()) {
            QPainterPathStroker stroker;
            stroker.setWidth(strokeWidth);
            geometry = stroker.createStroke(geometry);
        }
        return geometry;
    }
    case Okular::Annotation::AHighlight: {
        const auto *highlight = static_cast<const Okular::HighlightAnnotation *>(annotation);
        for (const auto &quad : highlight->highlightQuads()) {
            QPolygonF points;
            for (int index = 0; index < 4; ++index) {
                points << annotationPagePoint(quad.transformedPoint(index), width, height);
            }
            QPainterPath quadPath;
            quadPath.addPolygon(points);
            if (highlight->highlightType() == Okular::HighlightAnnotation::Underline || highlight->highlightType() == Okular::HighlightAnnotation::StrikeOut) {
                QPainterPath linePath;
                if (highlight->highlightType() == Okular::HighlightAnnotation::Underline) {
                    linePath.moveTo((3.0 * points.at(0) + points.at(3)) / 4.0);
                    linePath.lineTo((3.0 * points.at(1) + points.at(2)) / 4.0);
                } else {
                    linePath.moveTo((points.at(0) + points.at(3)) / 2.0);
                    linePath.lineTo((points.at(1) + points.at(2)) / 2.0);
                }
                QPainterPathStroker stroker;
                stroker.setWidth(2.0);
                geometry = geometry.united(stroker.createStroke(linePath));
            } else if (highlight->highlightType() == Okular::HighlightAnnotation::Squiggly) {
                QPainterPath squigglyPath;
                squigglyPath.moveTo(points.at(0));
                squigglyPath.lineTo(points.at(1));
                squigglyPath.lineTo((points.at(1) + points.at(2)) / 2.0);
                squigglyPath.lineTo((points.at(0) + points.at(3)) / 2.0);
                squigglyPath.closeSubpath();
                geometry = geometry.united(squigglyPath);
            } else {
                geometry = geometry.united(quadPath);
            }
        }
        return geometry;
    }
    default:
        if (!qFuzzyIsNull(annotation->frame().rotation)) {
            const QPolygonF outline = Okular::AnnotationUtils::annotationOutline(annotation, width, height);
            if (!outline.isEmpty()) {
                geometry.addPolygon(outline);
                geometry.closeSubpath();
                return geometry;
            }
        }
        break;
    }

    geometry.addRect(bounds);
    return geometry;
}

bool AnnotationDescription::isValid() const
{
    return (annotation != nullptr);
}

bool AnnotationDescription::isContainedInPage(const Okular::Document *document, int pageNumber) const
{
    if (AnnotationDescription::pageNumber == pageNumber) {
        /* Don't access page via pageViewItem here. pageViewItem might have been deleted. */
        const Okular::Page *page = document->page(pageNumber);
        if (page != nullptr) {
            if (page->annotations().contains(annotation)) {
                return true;
            }
        }
    }
    return false;
}

void AnnotationDescription::invalidate()
{
    annotation = nullptr;
    pageViewItem = nullptr;
    pageNumber = -1;
}

AnnotationDescription::AnnotationDescription(PageViewItem *newPageViewItem, const QPoint eventPos)
{
    const Okular::AnnotationObjectRect *annObjRect = nullptr;
    if (newPageViewItem) {
        const QRect &uncroppedPage = newPageViewItem->uncroppedGeometry();
        /* find out normalized mouse coords inside current item (nX and nY will be in the range of 0..1). */
        const double nX = newPageViewItem->absToPageX(eventPos.x());
        const double nY = newPageViewItem->absToPageY(eventPos.y());
        annObjRect = static_cast<const Okular::AnnotationObjectRect *>(newPageViewItem->page()->objectRect(Okular::ObjectRect::OAnnotation, nX, nY, uncroppedPage.width(), uncroppedPage.height()));
    }

    if (annObjRect) {
        annotation = annObjRect->annotation();
        pageViewItem = newPageViewItem;
        pageNumber = pageViewItem->pageNumber();
    } else {
        invalidate();
    }
}

MouseAnnotation::MouseAnnotation(PageView *parent, Okular::Document *document)
    : QObject(parent)
    , m_document(document)
    , m_pageView(parent)
    , m_state(StateInactive)
    , m_handle(RH_None)
{
    m_moveTimer.setSingleShot(true);
    m_moveTimer.setTimerType(Qt::PreciseTimer);
    m_moveTimer.setInterval(8);
    connect(&m_moveTimer, &QTimer::timeout, this, &MouseAnnotation::flushPendingMove);

    m_resizeHandleList << RH_Left << RH_Right << RH_Top << RH_Bottom << RH_TopLeft << RH_TopRight << RH_BottomLeft << RH_BottomRight;
}

MouseAnnotation::~MouseAnnotation()
{
}

void MouseAnnotation::routeMousePressEvent(PageViewItem *pageViewItem, const QPoint eventPos, bool selectOnHit)
{
    if (selectOnHit && m_areaSelectedAnnotations.size() > 1) {
        const AnnotationDescription &firstSelected = m_areaSelectedAnnotations.constFirst();
        if (pageViewItem->pageNumber() == firstSelected.pageNumber) {
            const QPoint pageOrigin = firstSelected.pageViewItem->uncroppedGeometry().topLeft();
            const QPoint pagePosition = eventPos - pageOrigin;
            m_handle = getSelectionHandleAt(pagePosition);
            if (m_handle != RH_None && m_handle != RH_Content) {
                m_mousePosition = pagePosition;
                setState(StateFocused, firstSelected);
                return;
            }
            if (!selectedBoundingRect().isNull() && selectedBoundingRect().contains(pagePosition)) {
                m_mousePosition = pagePosition;
                const bool canMoveSelection = std::all_of(m_areaSelectedAnnotations.cbegin(), m_areaSelectedAnnotations.cend(), [](const AnnotationDescription &selected) { return selected.annotation->canBeMoved(); });
                if (canMoveSelection) {
                    m_handle = RH_Content;
                    setState(StateMoving, firstSelected);
                }
                return;
            }
        }
    }

    /* Is there a selected annotation? */
    if (m_focusedAnnotation.isValid()) {
        m_mousePosition = eventPos - pageViewItem->uncroppedGeometry().topLeft();
        m_handle = m_areaSelectedAnnotations.size() > 1 && selectOnHit ? RH_None : getHandleAt(m_mousePosition, m_focusedAnnotation);
        if (m_handle != RH_None) {
            /* Returning here means, the selection-rectangle gets control, unconditionally.
             * Even if it overlaps with another annotation. */
            return;
        }
    }

    AnnotationDescription ad(pageViewItem, eventPos);
    /* qDebug() << "routeMousePressEvent: eventPos = " << eventPos; */
    if (ad.isValid()) {
        if (ad.annotation->subType() == Okular::Annotation::AMovie || ad.annotation->subType() == Okular::Annotation::AScreen || ad.annotation->subType() == Okular::Annotation::AFileAttachment ||
            ad.annotation->subType() == Okular::Annotation::ARichMedia) {
            /* qDebug() << "routeMousePressEvent: trigger action for AMovie/AScreen/AFileAttachment"; */
            processAction(ad);
        } else {
            /* qDebug() << "routeMousePressEvent: select for modification"; */
            m_mousePosition = eventPos - pageViewItem->uncroppedGeometry().topLeft();
            m_handle = getHandleAt(m_mousePosition, ad);
            if (selectOnHit) {
                const bool alreadySelected = std::any_of(m_areaSelectedAnnotations.cbegin(), m_areaSelectedAnnotations.cend(), [&ad](const AnnotationDescription &selected) {
                    return selected.annotation == ad.annotation;
                });
                if (!alreadySelected) {
                    m_areaSelectedAnnotations.clear();
                    m_areaSelectedAnnotations.append(ad);
                    m_handle = getHandleAt(m_mousePosition, ad);
                } else if (m_areaSelectedAnnotations.size() > 1) {
                    m_handle = ad.annotation->canBeMoved() ? RH_Content : RH_None;
                } else {
                    m_handle = getHandleAt(m_mousePosition, ad);
                }
                setState(StateFocused, ad);
            } else if (m_handle != RH_None) {
                setState(StateFocused, ad);
            }
        }
    } else {
        /* qDebug() << "routeMousePressEvent: no annotation under mouse, enter StateInactive"; */
        setState(StateInactive, ad);
    }
}

void MouseAnnotation::selectAnnotation(PageViewItem *pageViewItem, Okular::Annotation *annotation)
{
    if (!pageViewItem || !annotation) {
        return;
    }

    m_areaSelectedAnnotations.clear();
    AnnotationDescription selected;
    selected.annotation = annotation;
    selected.pageViewItem = pageViewItem;
    selected.pageNumber = pageViewItem->pageNumber();
    m_handle = RH_None;
    setState(StateFocused, selected);
    m_pageView->viewport()->update();
}

void MouseAnnotation::selectAnnotations(const QList<AnnotationDescription> &annotations)
{
    m_areaSelectedAnnotations = annotations;
    if (m_areaSelectedAnnotations.isEmpty()) {
        setState(StateInactive, {});
    } else {
        m_handle = RH_None;
        setState(StateFocused, m_areaSelectedAnnotations.constFirst());
    }
    m_pageView->viewport()->update();
}

QRect MouseAnnotation::selectedBoundingRect() const
{
    if (m_areaSelectedAnnotations.size() < 2) {
        return {};
    }

    const int pageNumber = m_areaSelectedAnnotations.constFirst().pageNumber;
    QRect selectionRect;
    for (const AnnotationDescription &selected : m_areaSelectedAnnotations) {
        if (!selected.isValid() || selected.pageNumber != pageNumber) {
            return {};
        }
        const QRect annotationRect = Okular::AnnotationUtils::annotationGeometry(selected.annotation, selected.pageViewItem->uncroppedWidth(), selected.pageViewItem->uncroppedHeight());
        selectionRect = selectionRect.isNull() ? annotationRect : selectionRect.united(annotationRect);
    }
    return selectionRect;
}

bool MouseAnnotation::selectionCanBeRotated() const
{
    return m_areaSelectedAnnotations.size() > 1 && !selectedBoundingRect().isNull()
        && std::all_of(m_areaSelectedAnnotations.cbegin(), m_areaSelectedAnnotations.cend(), [](const AnnotationDescription &description) {
               return description.isValid() && description.annotation->canBeRotated();
           });
}

QRect MouseAnnotation::getGroupRotateHandleRect(ResizeHandle corner) const
{
    const QRect boundingRect = selectedBoundingRect();
    if (boundingRect.isNull()) {
        return {};
    }

    QPointF cornerPoint;
    switch (corner) {
    case RH_TopLeft:
        cornerPoint = boundingRect.topLeft();
        break;
    case RH_TopRight:
        cornerPoint = boundingRect.topRight();
        break;
    case RH_BottomRight:
        cornerPoint = boundingRect.bottomRight();
        break;
    case RH_BottomLeft:
        cornerPoint = boundingRect.bottomLeft();
        break;
    default:
        return {};
    }

    const QPointF center = boundingRect.center();
    QPointF direction = cornerPoint - center;
    const double length = std::hypot(direction.x(), direction.y());
    if (length > 1e-9) {
        direction /= length;
    } else {
        direction = QPointF(1, 1) / std::sqrt(2.0);
    }
    const QPointF handleCenter = cornerPoint + direction * rotateHandleOffset;
    return QRect(qRound(handleCenter.x()) - rotateHandleSize / 2, qRound(handleCenter.y()) - rotateHandleSize / 2, rotateHandleSize, rotateHandleSize);
}

QRect MouseAnnotation::getGroupRotateHandleHitRect(ResizeHandle corner) const
{
    return getGroupRotateHandleRect(corner).adjusted(-handleHitExpansion, -handleHitExpansion, handleHitExpansion, handleHitExpansion);
}

Okular::NormalizedPoint MouseAnnotation::displayPointToFramePoint(const QPointF &pageItemPoint) const
{
    if (m_areaSelectedAnnotations.isEmpty()) {
        return Okular::NormalizedPoint();
    }

    const AnnotationDescription &first = m_areaSelectedAnnotations.constFirst();
    const double width = first.pageViewItem->uncroppedWidth();
    const double height = first.pageViewItem->uncroppedHeight();
    Okular::NormalizedPoint normalized(pageItemPoint.x() / width, pageItemPoint.y() / height);
    normalized.transform(pageRotationMatrixFor(first.pageViewItem->page()->rotation()).inverted());
    return normalized;
}

void MouseAnnotation::updateHover(PageViewItem *pageViewItem, const QPoint eventPos)
{
    const ResizeHandle previousHandle = m_handle;
    const AnnotationDescription previousMouseOver = m_mouseOverAnnotation;

    if (m_areaSelectedAnnotations.size() > 1) {
        const AnnotationDescription &firstSelected = m_areaSelectedAnnotations.constFirst();
        const QPoint pageOrigin = firstSelected.pageViewItem->uncroppedGeometry().topLeft();
        const QPoint pagePosition = eventPos - pageOrigin;
        m_mousePosition = pagePosition;
        const bool onSelectedPage = pageViewItem && pageViewItem->pageNumber() == firstSelected.pageNumber;
        m_handle = onSelectedPage ? getSelectionHandleAt(pagePosition) : RH_None;
    } else if (m_focusedAnnotation.isValid() && pageViewItem) {
        m_mousePosition = eventPos - pageViewItem->uncroppedGeometry().topLeft();
        m_handle = getHandleAt(m_mousePosition, m_focusedAnnotation);
    } else {
        m_handle = RH_None;
    }

    const AnnotationDescription mouseOver(pageViewItem, eventPos);
    if (mouseOver.isValid()) {
        m_mouseOverAnnotation = mouseOver;
    } else {
        m_mouseOverAnnotation.invalidate();
    }

    if (previousHandle != m_handle || !(previousMouseOver == m_mouseOverAnnotation)) {
        m_pageView->updateCursor();
    }
}

void MouseAnnotation::routeMouseReleaseEvent(PageViewItem *pageViewItem, const QPoint eventPos)
{
    if (isModified()) {
        flushPendingMove();
        /* qDebug() << "routeMouseReleaseEvent: finish command"; */
        const bool blockViewportMovement = !m_pageView->allowsAnnotationViewportMovement();
        if (blockViewportMovement) {
            m_document->beginAnnotationViewportMovementBlock();
        }
        finishCommand();
        if (blockViewportMovement) {
            m_document->endAnnotationViewportMovementBlock();
        }
        setState(StateFocused, m_focusedAnnotation);
    }
    updateHover(pageViewItem, eventPos);
    /*
    else
    {
        qDebug() << "routeMouseReleaseEvent: ignore";
    }
    */
}

void MouseAnnotation::routeMouseMoveEvent(PageViewItem *pageViewItem, const QPoint eventPos, bool leftButtonPressed, Qt::KeyboardModifiers modifiers)
{
    if (!pageViewItem && (leftButtonPressed ? !m_focusedAnnotation.isValid() : m_areaSelectedAnnotations.size() < 2 && !m_focusedAnnotation.isValid())) {
        /* qDebug() << "routeMouseMoveEvent: no pageViewItem provided, ignore"; */
        return;
    }

    if (leftButtonPressed) {
        if (isFocused()) {
            /* On first move event after annotation is selected, enter modification state */
            if (m_handle == RH_Content) {
                /* qDebug() << "routeMouseMoveEvent: handle " << m_handle << ", enter StateMoving"; */
                setState(StateMoving, m_focusedAnnotation);
            } else if (m_handle == RH_Rotate) {
                setState(StateRotating, m_focusedAnnotation);
            } else if (m_handle != RH_None) {
                /* qDebug() << "routeMouseMoveEvent: handle " << m_handle << ", enter StateResizing"; */
                setState(StateResizing, m_focusedAnnotation);
            }
        }

        if (isModified()) {
            m_pendingMovePos = eventPos;
            m_pendingMoveModifiers = modifiers;
            m_hasPendingMove = true;
            if (!m_moveTimer.isActive()) {
                m_moveTimer.start();
            }
        }
    } else {
        updateHover(pageViewItem, eventPos);
    }
}

void MouseAnnotation::selectAnnotationsInRect(PageViewItem *pageViewItem, const QRect &selectionRect)
{
    if (m_focusedAnnotation.isValid()) {
        setState(StateInactive, {});
    }
    m_areaSelectedAnnotations.clear();

    if (pageViewItem && !selectionRect.isNull()) {
        const QRect normalizedSelection = selectionRect.normalized();
        for (Okular::Annotation *annotation : pageViewItem->page()->annotations()) {
            if (Okular::AnnotationUtils::annotationGeometry(annotation, pageViewItem->uncroppedWidth(), pageViewItem->uncroppedHeight()).normalized().intersects(normalizedSelection)) {
                AnnotationDescription selected;
                selected.annotation = annotation;
                selected.pageViewItem = pageViewItem;
                selected.pageNumber = pageViewItem->pageNumber();
                m_areaSelectedAnnotations.append(selected);
            }
        }

        if (m_areaSelectedAnnotations.size() == 1) {
            setState(StateFocused, m_areaSelectedAnnotations.constFirst());
        }
    }

    m_pageView->viewport()->update();
}

void MouseAnnotation::selectAnnotationsInLasso(PageViewItem *pageViewItem, const QPolygonF &selectionPath)
{
    if (!pageViewItem || selectionPath.size() < 3) {
        selectAnnotations({});
        return;
    }

    QPainterPath region;
    region.addPolygon(selectionPath);
    region.closeSubpath();
    QPainterPathStroker boundaryStroker;
    boundaryStroker.setWidth(1.5);
    region = region.united(boundaryStroker.createStroke(region));

    QList<AnnotationDescription> selected;
    for (Okular::Annotation *annotation : pageViewItem->page()->annotations()) {
        const QPainterPath geometry = annotationLassoGeometry(annotation, pageViewItem);
        if (region.intersects(geometry)) {
            AnnotationDescription description;
            description.annotation = annotation;
            description.pageViewItem = pageViewItem;
            description.pageNumber = pageViewItem->pageNumber();
            selected.append(description);
        }
    }
    selectAnnotations(selected);
}

void MouseAnnotation::routeKeyPressEvent(const QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        cancel();
        m_areaSelectedAnnotations.clear();
        break;
    case Qt::Key_Delete:
        deleteSelectedAnnotations();
        break;
    }
}

void MouseAnnotation::deleteSelectedAnnotations()
{
    const QList<AnnotationDescription> selected = selectedAnnotations();
    if (selected.isEmpty()) {
        return;
    }

    const bool blockViewportMovement = !m_pageView->allowsAnnotationViewportMovement();
    if (blockViewportMovement) {
        m_document->beginAnnotationViewportMovementBlock();
    }
    if (selected.size() > 1) {
        cancel();
        m_areaSelectedAnnotations.clear();
        QHash<int, QList<Okular::Annotation *>> annotationsByPage;
        for (const AnnotationDescription &description : selected) {
            if (description.isValid() && description.isContainedInPage(m_document, description.pageNumber) && m_document->canRemovePageAnnotation(description.annotation)) {
                annotationsByPage[description.pageNumber].append(description.annotation);
            }
        }
        for (auto it = annotationsByPage.cbegin(); it != annotationsByPage.cend(); ++it) {
            m_document->removePageAnnotations(it.key(), it.value());
        }
        m_pageView->viewport()->update();
    } else if (!selected.isEmpty()) {
        AnnotationDescription adToBeDeleted = m_focusedAnnotation;
        cancel();
        if (m_document->canRemovePageAnnotation(adToBeDeleted.annotation)) {
            m_document->removePageAnnotation(adToBeDeleted.pageNumber, adToBeDeleted.annotation);
        }
    }
    if (blockViewportMovement) {
        m_document->endAnnotationViewportMovementBlock();
    }
}

QList<AnnotationDescription> MouseAnnotation::selectedAnnotations() const
{
    if (!m_areaSelectedAnnotations.isEmpty()) {
        return m_areaSelectedAnnotations;
    }
    if (m_focusedAnnotation.isValid()) {
        return {m_focusedAnnotation};
    }
    return {};
}

void MouseAnnotation::routeTooltipEvent(const QHelpEvent *helpEvent)
{
    /* qDebug() << "MouseAnnotation::routeTooltipEvent, event " << helpEvent; */
    if (m_mouseOverAnnotation.isValid() && m_mouseOverAnnotation.annotation->subType() != Okular::Annotation::AWidget) {
        /* get boundingRect in uncropped page coordinates */
        QRect boundingRect = Okular::AnnotationUtils::annotationGeometry(m_mouseOverAnnotation.annotation, m_mouseOverAnnotation.pageViewItem->uncroppedWidth(), m_mouseOverAnnotation.pageViewItem->uncroppedHeight());

        /* uncropped page to content area */
        boundingRect.translate(m_mouseOverAnnotation.pageViewItem->uncroppedGeometry().topLeft());
        /* content area to viewport */
        boundingRect.translate(-m_pageView->contentAreaPosition());

        const QString tip = GuiUtils::prettyToolTip(m_mouseOverAnnotation.annotation);
        QToolTip::showText(helpEvent->globalPos(), tip, m_pageView->viewport(), boundingRect);
    }
}

void MouseAnnotation::routePaint(QPainter *painter, const QRect paintRect)
{
    /* QPainter draws relative to the origin of uncropped viewport. */
    static const QColor fillColor = QColor::fromHsvF(0, 0, 0.75, 0.66);

    if (m_areaSelectedAnnotations.size() > 1) {
        const AnnotationDescription &firstSelected = m_areaSelectedAnnotations.constFirst();
        const QRect boundingRect = selectedBoundingRect();
        const QPoint origin = firstSelected.pageViewItem->uncroppedGeometry().topLeft();
        int padding = handlePadding;
        if (selectionCanBeRotated()) {
            padding = qMax(padding, rotateHandleOffset + rotateHandleSize / 2 + handleHitExpansion + 2);
        }
        if (paintRect.intersects(boundingRect.translated(origin).adjusted(-padding, -padding, padding, padding))) {
            painter->save();
            painter->translate(origin);
            painter->setPen(QPen(fillColor, 2, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
            painter->drawRect(boundingRect);
            if (!isModified() && selectionCanBeResized()) {
                painter->setPen(QPen(QColor(55, 76, 90), 1));
                painter->setBrush(Qt::white);
                for (const ResizeHandle &handle : std::as_const(m_resizeHandleList)) {
                    const QRect handleRect = getHandleRect(handle, boundingRect);
                    if (handle == RH_Top || handle == RH_Bottom || handle == RH_Left || handle == RH_Right) {
                        painter->drawRoundedRect(handleRect, handleShortSize / 2.0, handleShortSize / 2.0);
                    } else {
                        painter->drawEllipse(handleRect);
                    }
                }
            }
            if (!isModified() && selectionCanBeRotated()) {
                painter->setPen(QPen(QColor(55, 76, 90), 1));
                painter->setBrush(Qt::NoBrush);
                const ResizeHandle corners[] = { RH_TopLeft, RH_TopRight, RH_BottomRight, RH_BottomLeft };
                for (const ResizeHandle corner : corners) {
                    const QRect handleRect = getGroupRotateHandleRect(corner);
                    if (!handleRect.isNull()) {
                        painter->drawEllipse(handleRect.adjusted(2, 2, -2, -2));
                        painter->drawArc(handleRect.adjusted(1, 1, -1, -1), 35 * 16, 250 * 16);
                    }
                }
            }
            if (isRotating()) {
                const QString label = QStringLiteral("%1°").arg(qRadiansToDegrees(m_rotationDisplayAngle), 0, 'f', 1);
                const QPointF labelCenter = m_rotationCenterDisplay + QPointF(0, -boundingRect.height() / 2.0 - 28.0);
                const QRectF labelRect(labelCenter.x() - 38, labelCenter.y() - 11, 76, 22);
                painter->setPen(QPen(QColor(55, 76, 90), 1));
                painter->setBrush(QColor(255, 255, 255, 230));
                painter->drawRoundedRect(labelRect, 5, 5);
                painter->setPen(Qt::black);
                painter->drawText(labelRect, Qt::AlignCenter, label);
            }
            painter->restore();
        }
        return;
    }

    for (const AnnotationDescription &selected : std::as_const(m_areaSelectedAnnotations)) {
        if (!selected.isValid() || (isFocused() && selected.annotation == m_focusedAnnotation.annotation)) {
            continue;
        }
        const QRect boundingRect = Okular::AnnotationUtils::annotationGeometry(selected.annotation, selected.pageViewItem->uncroppedWidth(), selected.pageViewItem->uncroppedHeight());
        const QPoint origin = selected.pageViewItem->uncroppedGeometry().topLeft();
        if (!paintRect.intersects(boundingRect.translated(origin).adjusted(-handlePadding, -handlePadding, handlePadding, handlePadding))) {
            continue;
        }
        painter->save();
        painter->translate(origin);
        painter->setPen(QPen(fillColor, 2, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
        painter->drawRect(boundingRect);
        painter->restore();
    }

    if (!isFocused() && !isModified()) {
        return;
    }
    /*
     * Get annotation bounding rectangle in uncropped page coordinates.
     * Distinction between AnnotationUtils::annotationGeometry() and AnnotationObjectRect::boundingRect() is,
     * that boundingRect would enlarge the QRect to a minimum size of 14 x 14.
     * This is useful for getting focus an a very small annotation,
     * but for drawing and modification we want the real size.
     */
    const QRect boundingRect = Okular::AnnotationUtils::annotationGeometry(m_focusedAnnotation.annotation, m_focusedAnnotation.pageViewItem->uncroppedWidth(), m_focusedAnnotation.pageViewItem->uncroppedHeight());

    if (!paintRect.intersects(boundingRect.translated(m_focusedAnnotation.pageViewItem->uncroppedGeometry().topLeft()).adjusted(-handlePadding, -handlePadding, handlePadding, handlePadding))) {
        /* Our selection rectangle is not in a region that needs to be (re-)drawn. */
        return;
    }

    painter->save();
    painter->translate(m_focusedAnnotation.pageViewItem->uncroppedGeometry().topLeft());
    painter->setPen(QPen(fillColor, 2, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
    const bool rotatedFrame = !qFuzzyIsNull(m_focusedAnnotation.annotation->frame().rotation);
    if (rotatedFrame) {
        const QPolygonF outline = focusedAnnotationOutline();
        if (outline.size() >= 4) {
            painter->drawPolygon(outline);
        } else {
            painter->drawRect(boundingRect);
        }
    } else {
        painter->drawRect(boundingRect);
    }

    if (isFocused() && m_focusedAnnotation.annotation->canBeResized()) {
        painter->setPen(QPen(QColor(55, 76, 90), 1));
        painter->setBrush(Qt::white);
        for (const ResizeHandle &handle : std::as_const(m_resizeHandleList)) {
            const QRect handleRect = getHandleRect(handle, m_focusedAnnotation);
            if (handle == RH_Top || handle == RH_Bottom || handle == RH_Left || handle == RH_Right) {
                painter->drawRoundedRect(handleRect, handleShortSize / 2.0, handleShortSize / 2.0);
            } else {
                painter->drawEllipse(handleRect);
            }
        }
    }

    if (isFocused() && m_focusedAnnotation.annotation->canBeRotated()) {
        painter->setPen(QPen(QColor(55, 76, 90), 1));
        painter->setBrush(Qt::NoBrush);
        const ResizeHandle corners[] = { RH_TopLeft, RH_TopRight, RH_BottomRight, RH_BottomLeft };
        for (const ResizeHandle corner : corners) {
            const QRect handleRect = getRotateHandleRect(corner, m_focusedAnnotation);
            if (!handleRect.isNull()) {
                painter->drawEllipse(handleRect.adjusted(2, 2, -2, -2));
                painter->drawArc(handleRect.adjusted(1, 1, -1, -1), 35 * 16, 250 * 16);
            }
        }
    }

    if (isRotating()) {
        const QRectF outlineBounds = Okular::AnnotationUtils::annotationOutline(m_focusedAnnotation.annotation, m_focusedAnnotation.pageViewItem->uncroppedWidth(), m_focusedAnnotation.pageViewItem->uncroppedHeight()).boundingRect();
        const QString label = QStringLiteral("%1°").arg(qRadiansToDegrees(m_rotationDisplayAngle), 0, 'f', 1);
        const QPointF labelCenter = outlineBounds.center() + QPointF(0, -outlineBounds.height() / 2.0 - 28.0);
        const QRectF labelRect(labelCenter.x() - 38, labelCenter.y() - 11, 76, 22);
        painter->setPen(QPen(QColor(55, 76, 90), 1));
        painter->setBrush(QColor(255, 255, 255, 230));
        painter->drawRoundedRect(labelRect, 5, 5);
        painter->setPen(Qt::black);
        painter->drawText(labelRect, Qt::AlignCenter, label);
    }
    painter->restore();
}

Okular::Annotation *MouseAnnotation::annotation() const
{
    if (m_focusedAnnotation.isValid()) {
        return m_focusedAnnotation.annotation;
    }
    return nullptr;
}

int MouseAnnotation::pageNumber() const
{
    if (m_focusedAnnotation.isValid()) {
        return m_focusedAnnotation.pageNumber;
    }
    return -1;
}

bool MouseAnnotation::isActive() const
{
    return (m_state != StateInactive);
}

bool MouseAnnotation::isMouseOver() const
{
    return (m_mouseOverAnnotation.isValid() || m_handle != RH_None);
}

bool MouseAnnotation::isFocused() const
{
    return (m_state == StateFocused);
}

bool MouseAnnotation::isMoved() const
{
    return (m_state == StateMoving);
}

bool MouseAnnotation::isResized() const
{
    return (m_state == StateResizing);
}

bool MouseAnnotation::isRotating() const
{
    return (m_state == StateRotating);
}

bool MouseAnnotation::isModified() const
{
    return (m_state == StateMoving || m_state == StateResizing || m_state == StateRotating);
}

QCursor MouseAnnotation::cursor() const
{
    if (m_handle == RH_Rotate) {
        return rotationCursor();
    }

    if (m_areaSelectedAnnotations.size() > 1) {
        if (m_handle == RH_Content) {
            return isMoved() ? Qt::ClosedHandCursor : Qt::OpenHandCursor;
        }
        if (m_handle == RH_None) {
            return Qt::CrossCursor;
        }
    }

    if (m_handle != RH_None) {
        if (isMoved()) {
            return Qt::SizeAllCursor;
        } else if (isFocused() || isResized() || m_areaSelectedAnnotations.size() > 1) {
            switch (m_handle) {
            case RH_Top:
                return Qt::SizeVerCursor;
            case RH_TopRight:
                return Qt::SizeBDiagCursor;
            case RH_Right:
                return Qt::SizeHorCursor;
            case RH_BottomRight:
                return Qt::SizeFDiagCursor;
            case RH_Bottom:
                return Qt::SizeVerCursor;
            case RH_BottomLeft:
                return Qt::SizeBDiagCursor;
            case RH_Left:
                return Qt::SizeHorCursor;
            case RH_TopLeft:
                return Qt::SizeFDiagCursor;
            case RH_Content:
                return Qt::SizeAllCursor;
            default:
                return Qt::OpenHandCursor;
            }
        }
    } else if (m_mouseOverAnnotation.isValid()) {
        /* Mouse is over annotation, but the annotation is not yet selected. */
        if (m_mouseOverAnnotation.annotation->subType() == Okular::Annotation::AMovie) {
            return Qt::PointingHandCursor;
        } else if (m_mouseOverAnnotation.annotation->subType() == Okular::Annotation::ARichMedia) {
            return Qt::PointingHandCursor;
        } else if (m_mouseOverAnnotation.annotation->subType() == Okular::Annotation::AScreen) {
            if (GuiUtils::renditionMovieFromScreenAnnotation(static_cast<const Okular::ScreenAnnotation *>(m_mouseOverAnnotation.annotation)) != nullptr) {
                return Qt::PointingHandCursor;
            }
        } else if (m_mouseOverAnnotation.annotation->subType() == Okular::Annotation::AFileAttachment) {
            return Qt::PointingHandCursor;
        } else {
            return Qt::ArrowCursor;
        }
    }

    /* There's no none cursor, so we still have to return something. */
    return Qt::ArrowCursor;
}

void MouseAnnotation::notifyAnnotationChanged(int pageNumber)
{
    const AnnotationDescription emptyAd;

    if (m_focusedAnnotation.isValid() && !m_focusedAnnotation.isContainedInPage(m_document, pageNumber)) {
        setState(StateInactive, emptyAd);
    }

    if (m_mouseOverAnnotation.isValid() && !m_mouseOverAnnotation.isContainedInPage(m_document, pageNumber)) {
        m_mouseOverAnnotation = emptyAd;
        m_pageView->updateCursor();
    }

    m_areaSelectedAnnotations.removeIf([this, pageNumber](const AnnotationDescription &selected) { return !selected.isContainedInPage(m_document, pageNumber); });
}

void MouseAnnotation::updateAnnotationPointers()
{
    if (m_focusedAnnotation.annotation) {
        m_focusedAnnotation.annotation = m_document->page(m_focusedAnnotation.pageNumber)->annotation(m_focusedAnnotation.annotation->uniqueName());
    }

    if (m_mouseOverAnnotation.annotation) {
        m_mouseOverAnnotation.annotation = m_document->page(m_mouseOverAnnotation.pageNumber)->annotation(m_mouseOverAnnotation.annotation->uniqueName());
    }

    for (AnnotationDescription &selected : m_areaSelectedAnnotations) {
        if (selected.annotation) {
            selected.annotation = m_document->page(selected.pageNumber)->annotation(selected.annotation->uniqueName());
        }
    }
}

void MouseAnnotation::cancel()
{
    m_moveTimer.stop();
    m_hasPendingMove = false;
    if (isActive()) {
        if (isRotating()) {
            m_rotationCancelRequested = true;
        }
        finishCommand();
        setState(StateInactive, m_focusedAnnotation);
    }
}

void MouseAnnotation::reset()
{
    cancel();
    m_focusedAnnotation.invalidate();
    m_mouseOverAnnotation.invalidate();
    m_areaSelectedAnnotations.clear();
}

/* Handle state changes for the focused annotation. */
void MouseAnnotation::setState(MouseAnnotationState state, const AnnotationDescription &ad)
{
    /* qDebug() << "setState: requested " << state; */
    if (m_focusedAnnotation.isValid()) {
        /* If there was a annotation before, request also repaint for the previous area. */
        updateViewport(m_focusedAnnotation);
    }

    if (!ad.isValid()) {
        /* qDebug() << "No annotation provided, forcing state inactive." << state; */
        state = StateInactive;
    } else if ((state == StateMoving && !ad.annotation->canBeMoved()) || (state == StateResizing && !ad.annotation->canBeResized())
               || (state == StateRotating && !ad.annotation->canBeRotated())) {
        /* qDebug() << "Annotation does not support requested state, forcing state selected." << state; */
        state = StateInactive;
    }

    switch (state) {
    case StateMoving:
        m_liveMoveStarted = false;
        m_moveTotalDelta = Okular::NormalizedPoint();
        m_focusedAnnotation = ad;
        if (m_areaSelectedAnnotations.size() > 1 && !m_selectionMoveUndoMacroOpen) {
            m_document->beginUndoMacro(i18n("move annotations"));
            m_selectionMoveUndoMacroOpen = true;
            m_selectionMoveChanged = false;
        }
        for (const AnnotationDescription &selected : selectedAnnotations()) {
            selected.annotation->setFlags(selected.annotation->flags() | Okular::Annotation::BeingMoved);
        }
        updateViewport(m_focusedAnnotation);
        break;
    case StateRotating:
        m_focusedAnnotation = ad;
        m_rotationApplied = 0.0;
        m_rotationDisplayAngle = 0.0;
        m_rotationCancelRequested = false;
        {
            QPointF center;
            if (m_areaSelectedAnnotations.size() > 1) {
                center = QPointF(selectedBoundingRect().center());
                m_groupCenter = displayPointToFramePoint(center);
            } else {
                const QRectF outlineBounds = Okular::AnnotationUtils::annotationOutline(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight()).boundingRect();
                center = outlineBounds.center();
                m_groupCenter = ad.annotation->frame().center;
            }
            m_rotationCenterDisplay = center;
            m_rotationStartMouseAngle = std::atan2(m_mousePosition.y() - center.y(), m_mousePosition.x() - center.x());
        }
        for (const AnnotationDescription &selected : selectedAnnotations()) {
            selected.annotation->setFlags(selected.annotation->flags() | Okular::Annotation::BeingRotated);
        }
        m_document->beginAnnotationLiveMove(moveItems());
        updateViewport(m_focusedAnnotation);
        break;
    case StateResizing:
        m_liveResizeStarted = false;
        m_resizeTotals.clear();
        m_focusedAnnotation = ad;
        m_resizeStartMousePosition = m_mousePosition;
        m_resizeStartRects.clear();
        m_resizeStartBoundingRect = {};
        m_frameResizeActive = false;
        m_liveFrameResizeStarted = false;
        m_groupFrameResizeActive = false;
        m_liveGroupFrameResizeStarted = false;
        m_groupResizeStartFrames.clear();
        m_groupResizeStartCenters.clear();
        if (selectedAnnotations().size() == 1 && ad.annotation->canBeResized() && !qFuzzyIsNull(ad.annotation->frame().rotation)) {
            m_frameResizeActive = true;
            m_resizeStartFrame = ad.annotation->frame();
            m_resizeStartTransform = Okular::AnnotationUtils::annotationFrameTransform(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight());
            const QTransform inverse = m_resizeStartTransform.inverted();
            m_resizeStartLocalMouse = inverse.map(QPointF(m_mousePosition));
        }
        for (const AnnotationDescription &selected : selectedAnnotations()) {
            const Okular::NormalizedRect rect = selected.annotation->boundingRectangle();
            m_resizeStartRects.append(rect);
            m_resizeStartBoundingRect = m_resizeStartBoundingRect.isNull() ? rect : m_resizeStartBoundingRect | rect;
        }
        if (m_areaSelectedAnnotations.size() > 1) {
            for (const AnnotationDescription &selected : std::as_const(m_areaSelectedAnnotations)) {
                if (qFuzzyIsNull(selected.annotation->frame().rotation)) {
                    continue;
                }
                m_groupFrameResizeActive = true;
                break;
            }
            m_groupFrameResizeActive = m_groupFrameResizeActive && selectionCanBeResized();
            if (!m_groupFrameResizeActive) {
                if (!m_selectionResizeUndoMacroOpen) {
                    m_document->beginUndoMacro(i18n("resize annotations"));
                    m_selectionResizeUndoMacroOpen = true;
                }
            } else {
                const QRect groupRect = selectedBoundingRect();
                const double width = ad.pageViewItem->uncroppedWidth();
                const double height = ad.pageViewItem->uncroppedHeight();
                m_resizeStartBoundingRect = Okular::NormalizedRect(groupRect.left() / width, groupRect.top() / height, groupRect.right() / width, groupRect.bottom() / height);
                m_groupResizeStartFrames.reserve(m_areaSelectedAnnotations.size());
                m_groupResizeStartCenters.reserve(m_areaSelectedAnnotations.size());
                for (const AnnotationDescription &selected : std::as_const(m_areaSelectedAnnotations)) {
                    m_groupResizeStartFrames.append(selected.annotation->frame());
                    const QTransform transform = Okular::AnnotationUtils::annotationFrameTransform(selected.annotation, selected.pageViewItem->uncroppedWidth(), selected.pageViewItem->uncroppedHeight());
                    m_groupResizeStartCenters.append(transform.map(QPointF(0, 0)));
                }
            }
            for (const AnnotationDescription &selected : std::as_const(m_areaSelectedAnnotations)) {
                selected.annotation->setFlags(selected.annotation->flags() | Okular::Annotation::BeingResized);
            }
        } else {
            m_focusedAnnotation.annotation->setFlags(m_focusedAnnotation.annotation->flags() | Okular::Annotation::BeingResized);
        }
        if (m_frameResizeActive) {
            m_document->beginAnnotationLiveMove(moveItems());
            m_liveFrameResizeStarted = true;
        } else if (m_groupFrameResizeActive) {
            m_document->beginAnnotationLiveMove(moveItems());
            m_liveGroupFrameResizeStarted = true;
        }
        updateViewport(m_focusedAnnotation);
        break;
    case StateFocused:
        m_focusedAnnotation = ad;
        m_resizeStartRects.clear();
        m_resizeStartBoundingRect = {};
        m_focusedAnnotation.annotation->setFlags(m_focusedAnnotation.annotation->flags() & ~(Okular::Annotation::BeingMoved | Okular::Annotation::BeingResized | Okular::Annotation::BeingRotated));
        updateViewport(m_focusedAnnotation);
        break;
    case StateInactive:
    default:
        for (const AnnotationDescription &selected : selectedAnnotations()) {
            if (selected.annotation) {
                selected.annotation->setFlags(selected.annotation->flags() & ~(Okular::Annotation::BeingMoved | Okular::Annotation::BeingResized | Okular::Annotation::BeingRotated));
            }
        }
        m_focusedAnnotation.invalidate();
        m_handle = RH_None;
        m_resizeStartRects.clear();
        m_resizeStartBoundingRect = {};
    }

    /* qDebug() << "setState: enter " << state; */
    m_state = state;
    m_pageView->updateCursor();
}

/* Get the rectangular boundary of the given annotation, enlarged for space needed by resize handles.
 * Returns a QRect in page view item coordinates. */
QRect MouseAnnotation::getFullBoundingRect(const AnnotationDescription &ad) const
{
    QRect boundingRect;
    if (ad.isValid()) {
        boundingRect = Okular::AnnotationUtils::annotationGeometry(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight());
        int padding = handlePadding;
        if (ad.annotation->canBeRotated()) {
            padding = qMax(padding, rotateHandleOffset + rotateHandleSize / 2 + handleHitExpansion + 2);
        }
        boundingRect = boundingRect.adjusted(-padding, -padding, padding, padding);
    }
    return boundingRect;
}

/* Apply the command determined by m_state to the currently focused annotation. */
void MouseAnnotation::performCommand(const QPoint newPos, Qt::KeyboardModifiers modifiers)
{
    const QRect &pageViewItemRect = m_focusedAnnotation.pageViewItem->uncroppedGeometry();
    const QPoint resizeMousePosition = isResized() ? m_resizeStartMousePosition : m_mousePosition;
    QPointF mouseDelta(newPos - pageViewItemRect.topLeft() - resizeMousePosition);
    QPointF normalizedRotatedMouseDelta(rotateInRect(QPointF(mouseDelta.x() / pageViewItemRect.width(), mouseDelta.y() / pageViewItemRect.height()), m_focusedAnnotation.pageViewItem->page()->rotation()));

    if (isMoved()) {
        Okular::NormalizedPoint delta(normalizedRotatedMouseDelta.x(), normalizedRotatedMouseDelta.y());
        const QList<AnnotationDescription> selected = selectedAnnotations();
        Okular::NormalizedRect groupRect;
        bool hasGroupRect = false;
        for (const AnnotationDescription &description : selected) {
            const Okular::NormalizedRect rect = description.annotation->boundingRectangle();
            groupRect = hasGroupRect ? groupRect | rect : rect;
            hasGroupRect = true;
        }
        if (delta.x < 0 && groupRect.left + delta.x < 0) {
            delta.x = -groupRect.left;
        }
        if (delta.x > 0 && groupRect.right + delta.x > 1) {
            delta.x = 1 - groupRect.right;
        }
        if (delta.y < 0 && groupRect.top + delta.y < 0) {
            delta.y = -groupRect.top;
        }
        if (delta.y > 0 && groupRect.bottom + delta.y > 1) {
            delta.y = 1 - groupRect.bottom;
        }
        if (!qFuzzyIsNull(delta.x) || !qFuzzyIsNull(delta.y)) {
            const QList<Okular::Document::AnnotationMoveItem> items = moveItems();
            if (!m_liveMoveStarted) {
                m_document->beginAnnotationLiveMove(items);
                m_liveMoveStarted = true;
            }
            // Moves the in-memory annotations and notifies once per page: no undo command and no generator
            // round trip per mouse event. The single undo entry is created in finishCommand().
            m_document->translateAnnotationsLive(items, delta);
            m_moveTotalDelta.x += delta.x;
            m_moveTotalDelta.y += delta.y;
            if (selected.size() > 1) {
                m_selectionMoveChanged = true;
            }
        }

    } else if (isRotating()) {
        const QPointF center = m_rotationCenterDisplay;
        const QPointF mousePagePoint = QPointF(newPos - pageViewItemRect.topLeft());
        const double currentAngle = std::atan2(mousePagePoint.y() - center.y(), mousePagePoint.x() - center.x());
        double totalDelta = normalizedAngleDelta(currentAngle - m_rotationStartMouseAngle);
        if (modifiers & Qt::ShiftModifier) {
            const double step = M_PI / 12.0;
            totalDelta = std::round(totalDelta / step) * step;
        }
        m_rotationDisplayAngle = totalDelta;
        applyLiveRotation(totalDelta);
    } else if (isResized()) {
        if (m_frameResizeActive) {
            const QPointF pageMousePosition = QPointF(newPos - pageViewItemRect.topLeft());
            const QPointF localMouse = m_resizeStartTransform.inverted().map(pageMousePosition);
            const bool preserveAspectRatio = !(modifiers & Qt::ShiftModifier);
            const Okular::AnnotationFrame targetFrame = frameAfterResize(localMouse, preserveAspectRatio);
            applyLiveFrameResize(m_focusedAnnotation, targetFrame);
            return;
        }

        if (m_groupFrameResizeActive) {
            const double width = pageViewItemRect.width();
            const double height = pageViewItemRect.height();
            const QRectF startRect(m_resizeStartBoundingRect.left * width,
                                   m_resizeStartBoundingRect.top * height,
                                   (m_resizeStartBoundingRect.right - m_resizeStartBoundingRect.left) * width,
                                   (m_resizeStartBoundingRect.bottom - m_resizeStartBoundingRect.top) * height);
            const QPointF mouseDelta = QPointF(newPos - pageViewItemRect.topLeft() - m_resizeStartMousePosition);
            const bool movesLeft = m_handle & RH_Left;
            const bool movesRight = m_handle & RH_Right;
            const bool movesTop = m_handle & RH_Top;
            const bool movesBottom = m_handle & RH_Bottom;
            if (!(movesLeft || movesRight || movesTop || movesBottom)) {
                return;
            }

            const double minimumWidth = qMin(startRect.width(), qMax(8.0, AnnotationWidthPresets::minimumNormalizedResizeDimensionForPagePoints(width) * width));
            const double minimumHeight = qMin(startRect.height(), qMax(8.0, AnnotationWidthPresets::minimumNormalizedResizeDimensionForPagePoints(height) * height));
            double left = startRect.left();
            double right = startRect.right();
            double top = startRect.top();
            double bottom = startRect.bottom();
            if (movesLeft) {
                left = qBound(0.0, left + mouseDelta.x(), right - minimumWidth);
            } else if (movesRight) {
                right = qBound(left + minimumWidth, right + mouseDelta.x(), width);
            }
            if (movesTop) {
                top = qBound(0.0, top + mouseDelta.y(), bottom - minimumHeight);
            } else if (movesBottom) {
                bottom = qBound(top + minimumHeight, bottom + mouseDelta.y(), height);
            }

            const double scaleX = (movesLeft || movesRight) && startRect.width() > 1e-9 ? (right - left) / startRect.width() : 1.0;
            const double scaleY = (movesTop || movesBottom) && startRect.height() > 1e-9 ? (bottom - top) / startRect.height() : 1.0;
            double scale = 1.0;
            if (movesLeft || movesRight) {
                if (movesTop || movesBottom) {
                    scale = qMax(scaleX, scaleY);
                } else {
                    scale = scaleX;
                }
            } else {
                scale = scaleY;
            }
            if (scale <= 0.0 || !std::isfinite(scale)) {
                return;
            }

            QRectF targetRect = startRect;
            if (movesLeft) {
                targetRect.setLeft(startRect.right() - startRect.width() * scale);
            } else if (movesRight) {
                targetRect.setRight(startRect.left() + startRect.width() * scale);
            } else {
                targetRect.setLeft(startRect.center().x() - startRect.width() * scale / 2.0);
                targetRect.setRight(startRect.center().x() + startRect.width() * scale / 2.0);
            }
            if (movesTop) {
                targetRect.setTop(startRect.bottom() - startRect.height() * scale);
            } else if (movesBottom) {
                targetRect.setBottom(startRect.top() + startRect.height() * scale);
            } else {
                targetRect.setTop(startRect.center().y() - startRect.height() * scale / 2.0);
                targetRect.setBottom(startRect.center().y() + startRect.height() * scale / 2.0);
            }
            applyLiveGroupFrameResize(targetRect, true);
            return;
        }

        QPointF delta1, delta2;
        handleToAdjust(normalizedRotatedMouseDelta, delta1, delta2, m_handle, m_focusedAnnotation.pageViewItem->page()->rotation());
        const bool preserveAspectRatio = !(modifiers & Qt::ShiftModifier);
        if (m_areaSelectedAnnotations.size() > 1) {
            const Okular::NormalizedRect targetGroupRect = resizedRect(m_resizeStartBoundingRect, delta1, delta2, preserveAspectRatio);
            const double oldWidth = m_resizeStartBoundingRect.right - m_resizeStartBoundingRect.left;
            const double oldHeight = m_resizeStartBoundingRect.bottom - m_resizeStartBoundingRect.top;
            const double scaleX = oldWidth > 0.000001 ? (targetGroupRect.right - targetGroupRect.left) / oldWidth : 1.0;
            const double scaleY = oldHeight > 0.000001 ? (targetGroupRect.bottom - targetGroupRect.top) / oldHeight : 1.0;

            QList<Okular::Document::AnnotationAdjustItem> steps;
            steps.reserve(m_areaSelectedAnnotations.size());
            for (qsizetype index = 0; index < m_areaSelectedAnnotations.size(); ++index) {
                const AnnotationDescription &description = m_areaSelectedAnnotations.at(index);
                const Okular::NormalizedRect rect = description.annotation->boundingRectangle();
                const Okular::NormalizedRect startRect = m_resizeStartRects.at(index);
                const double targetLeft = targetGroupRect.left + (startRect.left - m_resizeStartBoundingRect.left) * scaleX;
                const double targetRight = targetGroupRect.left + (startRect.right - m_resizeStartBoundingRect.left) * scaleX;
                const double targetTop = targetGroupRect.top + (startRect.top - m_resizeStartBoundingRect.top) * scaleY;
                const double targetBottom = targetGroupRect.top + (startRect.bottom - m_resizeStartBoundingRect.top) * scaleY;
                Okular::Document::AnnotationAdjustItem step;
                step.page = description.pageNumber;
                step.annotation = description.annotation;
                step.delta1 = Okular::NormalizedPoint(targetLeft - rect.left, targetTop - rect.top);
                step.delta2 = Okular::NormalizedPoint(targetRight - rect.right, targetBottom - rect.bottom);
                steps.append(step);
            }
            applyLiveResize(steps);
        } else {
            const Okular::NormalizedRect rect = m_focusedAnnotation.annotation->boundingRectangle();
            const Okular::NormalizedRect targetRect = resizedRect(m_resizeStartBoundingRect, delta1, delta2, preserveAspectRatio);
            Okular::Document::AnnotationAdjustItem step;
            step.page = m_focusedAnnotation.pageNumber;
            step.annotation = m_focusedAnnotation.annotation;
            step.delta1 = Okular::NormalizedPoint(targetRect.left - rect.left, targetRect.top - rect.top);
            step.delta2 = Okular::NormalizedPoint(targetRect.right - rect.right, targetRect.bottom - rect.bottom);
            applyLiveResize({step});
        }
    }
}

void MouseAnnotation::applyLiveResize(const QList<Okular::Document::AnnotationAdjustItem> &steps)
{
    bool anyChange = false;
    for (const auto &step : steps) {
        anyChange = anyChange || !qFuzzyIsNull(step.delta1.x) || !qFuzzyIsNull(step.delta1.y) || !qFuzzyIsNull(step.delta2.x) || !qFuzzyIsNull(step.delta2.y);
    }
    if (!anyChange) {
        return;
    }

    if (!m_liveResizeStarted) {
        QList<Okular::Document::AnnotationMoveItem> items;
        for (const auto &step : steps) {
            items.append({step.page, step.annotation});
        }
        m_document->beginAnnotationLiveMove(items); // hides the native versions, one page re-render
        m_resizeTotals = steps;
        for (auto &total : m_resizeTotals) {
            total.delta1 = Okular::NormalizedPoint();
            total.delta2 = Okular::NormalizedPoint();
        }
        m_liveResizeStarted = true;
    }

    // The selection does not change while resizing, so steps and totals line up by index.
    m_document->adjustAnnotationsLive(steps);
    for (qsizetype i = 0; i < steps.size() && i < m_resizeTotals.size(); ++i) {
        m_resizeTotals[i].delta1.x += steps[i].delta1.x;
        m_resizeTotals[i].delta1.y += steps[i].delta1.y;
        m_resizeTotals[i].delta2.x += steps[i].delta2.x;
        m_resizeTotals[i].delta2.y += steps[i].delta2.y;
    }
}

void MouseAnnotation::applyLiveRotation(double totalDelta)
{
    const double step = totalDelta - m_rotationApplied;
    if (qFuzzyIsNull(step)) {
        return;
    }

    const QList<Okular::Document::AnnotationRotationItem> items = rotationItems();
    m_document->rotateAnnotationsLive(items, step);
    m_rotationApplied = totalDelta;
    updateViewportForSelection();
}

QList<Okular::Document::AnnotationRotationItem> MouseAnnotation::rotationItems() const
{
    QList<Okular::Document::AnnotationRotationItem> items;
    for (const AnnotationDescription &description : selectedAnnotations()) {
        if (!description.isValid()) {
            continue;
        }
        Okular::Document::AnnotationRotationItem item;
        item.page = description.pageNumber;
        item.annotation = description.annotation;
        item.center = m_groupCenter;
        item.delta = m_rotationApplied;
        items.append(item);
    }
    return items;
}

static QPointF rotateClockwise(const QPointF &vector, double angle)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return QPointF(vector.x() * c - vector.y() * s, vector.x() * s + vector.y() * c);
}

Okular::AnnotationFrame MouseAnnotation::frameAfterResize(const QPointF &localMouse, bool preserveAspectRatio) const
{
    const Okular::Page *page = m_focusedAnnotation.pageViewItem ? m_focusedAnnotation.pageViewItem->page() : nullptr;
    const bool swapped = page && (static_cast<int>(page->rotation()) % 2);
    const double pageWidth = page ? (swapped ? page->height() : page->width()) : 1.0;
    const double pageHeight = page ? (swapped ? page->width() : page->height()) : 1.0;

    const double startWidth = m_resizeStartFrame.size.width() * pageWidth;
    const double startHeight = m_resizeStartFrame.size.height() * pageHeight;
    const bool movesLeft = m_resizeStartLocalMouse.x() < -1e-6;
    const bool movesRight = m_resizeStartLocalMouse.x() > 1e-6;
    const bool movesTop = m_resizeStartLocalMouse.y() < -1e-6;
    const bool movesBottom = m_resizeStartLocalMouse.y() > 1e-6;

    const double anchorX = movesLeft ? startWidth / 2.0 : -startWidth / 2.0;
    const double anchorY = movesTop ? startHeight / 2.0 : -startHeight / 2.0;

    double targetWidth = (movesLeft || movesRight) ? qAbs(anchorX - localMouse.x()) : startWidth;
    double targetHeight = (movesTop || movesBottom) ? qAbs(anchorY - localMouse.y()) : startHeight;

    const double minimumWidth = AnnotationWidthPresets::minimumNormalizedResizeDimensionForPagePoints(pageWidth) * pageWidth;
    const double minimumHeight = AnnotationWidthPresets::minimumNormalizedResizeDimensionForPagePoints(pageHeight) * pageHeight;
    targetWidth = qMax(targetWidth, qMin(minimumWidth, startWidth));
    targetHeight = qMax(targetHeight, qMin(minimumHeight, startHeight));

    if (preserveAspectRatio && startWidth > 1e-9 && startHeight > 1e-9) {
        double scale = 1.0;
        if ((movesLeft || movesRight) && (movesTop || movesBottom)) {
            scale = qMax(targetWidth / startWidth, targetHeight / startHeight);
        } else if (movesLeft || movesRight) {
            scale = targetWidth / startWidth;
        } else if (movesTop || movesBottom) {
            scale = targetHeight / startHeight;
        }
        const double minimumScale = qMax(minimumWidth / startWidth, minimumHeight / startHeight);
        scale = qMax(scale, minimumScale);
        targetWidth = startWidth * scale;
        targetHeight = startHeight * scale;
    }

    double centerX = 0.0;
    if (movesLeft) {
        centerX = anchorX - targetWidth / 2.0;
    } else if (movesRight) {
        centerX = anchorX + targetWidth / 2.0;
    }
    double centerY = 0.0;
    if (movesTop) {
        centerY = anchorY - targetHeight / 2.0;
    } else if (movesBottom) {
        centerY = anchorY + targetHeight / 2.0;
    }

    const QPointF rotatedCenter = rotateClockwise(QPointF(centerX, centerY), m_resizeStartFrame.rotation);
    const QPointF pageCenter(m_resizeStartFrame.center.x * pageWidth, m_resizeStartFrame.center.y * pageHeight);

    Okular::AnnotationFrame target;
    target.center = Okular::NormalizedPoint((pageCenter.x() + rotatedCenter.x()) / pageWidth, (pageCenter.y() + rotatedCenter.y()) / pageHeight);
    target.size = QSizeF(targetWidth / pageWidth, targetHeight / pageHeight);
    target.rotation = m_resizeStartFrame.rotation;
    return target;
}

void MouseAnnotation::applyLiveFrameResize(const AnnotationDescription &description, const Okular::AnnotationFrame &targetFrame)
{
    Okular::Document::AnnotationFrameResizeItem item;
    item.page = description.pageNumber;
    item.annotation = description.annotation;
    item.startFrame = m_resizeStartFrame;
    item.targetFrame = targetFrame;
    m_document->resizeAnnotationFramesLive({item});
    updateViewportForSelection();
}

void MouseAnnotation::applyLiveGroupFrameResize(const QRectF &targetGroupRect, bool uniform)
{
    Q_UNUSED(uniform)
    if (m_areaSelectedAnnotations.isEmpty() || m_groupResizeStartFrames.size() != m_areaSelectedAnnotations.size()) {
        return;
    }

    const AnnotationDescription &first = m_areaSelectedAnnotations.constFirst();
    const double width = first.pageViewItem->uncroppedWidth();
    const double height = first.pageViewItem->uncroppedHeight();
    const QRectF startRect(m_resizeStartBoundingRect.left * width,
                           m_resizeStartBoundingRect.top * height,
                           (m_resizeStartBoundingRect.right - m_resizeStartBoundingRect.left) * width,
                           (m_resizeStartBoundingRect.bottom - m_resizeStartBoundingRect.top) * height);
    if (startRect.width() < 1e-9 || startRect.height() < 1e-9) {
        return;
    }

    const double scaleX = targetGroupRect.width() / startRect.width();
    const double scaleY = targetGroupRect.height() / startRect.height();
    const double anchorX = qAbs(targetGroupRect.left() - startRect.left()) > 1e-6 ? startRect.right() : startRect.left();
    const double anchorY = qAbs(targetGroupRect.top() - startRect.top()) > 1e-6 ? startRect.bottom() : startRect.top();

    QList<Okular::Document::AnnotationFrameResizeItem> items;
    items.reserve(m_areaSelectedAnnotations.size());
    for (qsizetype index = 0; index < m_areaSelectedAnnotations.size(); ++index) {
        const AnnotationDescription &description = m_areaSelectedAnnotations.at(index);
        const Okular::AnnotationFrame &startFrame = m_groupResizeStartFrames.at(index);
        const QPointF startCenter = m_groupResizeStartCenters.at(index);
        const QPointF targetCenter(anchorX + (startCenter.x() - anchorX) * scaleX, anchorY + (startCenter.y() - anchorY) * scaleY);

        Okular::Document::AnnotationFrameResizeItem item;
        item.page = description.pageNumber;
        item.annotation = description.annotation;
        item.startFrame = startFrame;
        item.targetFrame.center = displayPointToFramePoint(targetCenter);
        item.targetFrame.size = QSizeF(startFrame.size.width() * scaleX, startFrame.size.height() * scaleY);
        item.targetFrame.rotation = startFrame.rotation;
        items.append(item);
    }
    m_document->resizeAnnotationFramesLive(items);
    updateViewportForSelection();
}

/* Apply the most recent queued mouse position. */
void MouseAnnotation::flushPendingMove()
{
    m_moveTimer.stop();
    if (!m_hasPendingMove) {
        return;
    }
    m_hasPendingMove = false;
    if (isModified() && m_focusedAnnotation.isValid()) {
        applyMoveStep(m_pendingMovePos, m_pendingMoveModifiers);
    }
}

void MouseAnnotation::applyMoveStep(const QPoint eventPos, Qt::KeyboardModifiers modifiers)
{
    updateViewportForSelection();
    const bool blockViewportMovement = !m_pageView->allowsAnnotationViewportMovement();
    if (blockViewportMovement) {
        m_document->beginAnnotationViewportMovementBlock();
    }
    performCommand(eventPos, modifiers);
    if (blockViewportMovement) {
        m_document->endAnnotationViewportMovementBlock();
    }
    m_mousePosition = eventPos - m_focusedAnnotation.pageViewItem->uncroppedGeometry().topLeft();
    updateViewportForSelection();
}

bool MouseAnnotation::isDragInProgress() const
{
    return isModified() && !m_finishing;
}

/* Finalize a command in progress for the currently focused annotation. */
void MouseAnnotation::finishCommand()
{
    m_finishing = true;
    struct FinishGuard {
        bool &flag;
        ~FinishGuard() { flag = false; }
    } finishGuard{m_finishing};
    /*
     * Note:
     * Translate-/resizePageAnnotation causes PopplerAnnotationProxy::notifyModification,
     * where modify flag needs to be already cleared. So it is important to call
     * setFlags before translatePageAnnotation-/adjustPageAnnotation.
     */
    if (isRotating()) {
        const QList<AnnotationDescription> selected = selectedAnnotations();
        for (const AnnotationDescription &description : selected) {
            description.annotation->setFlags(description.annotation->flags() & ~Okular::Annotation::BeingRotated);
        }

        const bool belowThreshold = qAbs(m_rotationApplied) < qDegreesToRadians(0.25);
        if (m_rotationCancelRequested || belowThreshold) {
            if (!qFuzzyIsNull(m_rotationApplied)) {
                m_document->rotateAnnotationsLive(rotationItems(), -m_rotationApplied);
            }
            QList<Okular::Document::AnnotationRotationItem> items = rotationItems();
            for (auto &item : items) {
                item.delta = 0.0;
            }
            m_document->commitAnnotationRotation(items);
        } else {
            QList<Okular::Document::AnnotationRotationItem> items = rotationItems();
            for (auto &item : items) {
                item.delta = m_rotationApplied;
            }
            m_document->commitAnnotationRotation(items);
        }

        m_rotationApplied = 0.0;
        m_rotationDisplayAngle = 0.0;
        m_rotationCancelRequested = false;
    } else if (isMoved()) {
        const QList<AnnotationDescription> selected = selectedAnnotations();
        for (const AnnotationDescription &description : selected) {
            description.annotation->setFlags(description.annotation->flags() & ~(Okular::Annotation::BeingMoved | Okular::Annotation::BeingRotated));
        }
        if (m_liveMoveStarted) {
            m_document->commitAnnotationMove(moveItems(), m_moveTotalDelta);
        }
        m_liveMoveStarted = false;
        m_moveTotalDelta = Okular::NormalizedPoint();
        if (m_selectionMoveUndoMacroOpen) {
            m_document->endUndoMacro();
            m_selectionMoveUndoMacroOpen = false;
            m_selectionMoveChanged = false;
        }
    } else if (isResized()) {
        const QList<AnnotationDescription> selected = selectedAnnotations();
        for (const AnnotationDescription &description : selected) {
            description.annotation->setFlags(description.annotation->flags() & ~(Okular::Annotation::BeingResized | Okular::Annotation::BeingRotated));
        }
        if (m_groupFrameResizeActive) {
            if (m_liveGroupFrameResizeStarted && m_groupResizeStartFrames.size() == selected.size()) {
                QList<Okular::Document::AnnotationFrameResizeItem> items;
                items.reserve(selected.size());
                for (qsizetype index = 0; index < selected.size(); ++index) {
                    Okular::Document::AnnotationFrameResizeItem item;
                    item.page = selected.at(index).pageNumber;
                    item.annotation = selected.at(index).annotation;
                    item.startFrame = m_groupResizeStartFrames.at(index);
                    item.targetFrame = selected.at(index).annotation->frame();
                    items.append(item);
                }
                m_document->commitAnnotationFrameResize(items);
            }
        } else if (m_frameResizeActive) {
            if (m_liveFrameResizeStarted) {
                Okular::Document::AnnotationFrameResizeItem item;
                item.page = m_focusedAnnotation.pageNumber;
                item.annotation = m_focusedAnnotation.annotation;
                item.startFrame = m_resizeStartFrame;
                item.targetFrame = m_focusedAnnotation.annotation->frame();
                m_document->commitAnnotationFrameResize({item});
            }
        } else {
            if (m_liveResizeStarted) {
                m_document->commitAnnotationAdjust(m_resizeTotals);
            }
        }
        m_liveResizeStarted = false;
        m_liveFrameResizeStarted = false;
        m_liveGroupFrameResizeStarted = false;
        m_frameResizeActive = false;
        m_groupFrameResizeActive = false;
        m_groupResizeStartFrames.clear();
        m_groupResizeStartCenters.clear();
        m_resizeTotals.clear();
        if (selected.size() > 1 && m_selectionResizeUndoMacroOpen) {
            m_document->endUndoMacro();
            m_selectionResizeUndoMacroOpen = false;
        }
    }
}

/* Tell viewport widget that the rectangular of the given annotation needs to be repainted. */
void MouseAnnotation::updateViewport(const AnnotationDescription &ad) const
{
    const QRect &changedPageViewItemRect = getFullBoundingRect(ad);
    if (changedPageViewItemRect.isValid()) {
        m_pageView->viewport()->update(changedPageViewItemRect.translated(ad.pageViewItem->uncroppedGeometry().topLeft()).translated(-m_pageView->contentAreaPosition()));
    }
}

QList<Okular::Document::AnnotationMoveItem> MouseAnnotation::moveItems() const
{
    QList<Okular::Document::AnnotationMoveItem> items;
    const QList<AnnotationDescription> selected = selectedAnnotations();
    items.reserve(selected.size());
    for (const AnnotationDescription &description : selected) {
        if (description.isValid()) {
            items.append({description.pageNumber, description.annotation});
        }
    }
    return items;
}

void MouseAnnotation::updateViewportForSelection() const
{
    if (isRotating()) {
        m_pageView->viewport()->update();
        return;
    }

    const QList<AnnotationDescription> selected = selectedAnnotations();
    for (const AnnotationDescription &description : selected) {
        updateViewport(description);
    }

    if (m_areaSelectedAnnotations.size() > 1) {
        // Frame and handles of the group live on the frame's border, so invalidate a band around it instead of the
        // whole (possibly page-sized) interior.
        const QRect frame = selectedBoundingRect();
        if (!frame.isNull()) {
            const QPoint offset = m_areaSelectedAnnotations.constFirst().pageViewItem->uncroppedGeometry().topLeft() - m_pageView->contentAreaPosition();
            int padding = handlePadding;
            if (selectionCanBeRotated()) {
                padding = qMax(padding, rotateHandleOffset + rotateHandleSize / 2 + handleHitExpansion + 2);
            }
            const QRegion band = QRegion(frame.adjusted(-padding, -padding, padding, padding)).subtracted(QRegion(frame.adjusted(padding, padding, -padding, -padding)));
            m_pageView->viewport()->update(band.translated(offset));
        }
    }
}

/* eventPos: Mouse position in uncropped page coordinates.
   ad: The annotation to get the handle for. */
MouseAnnotation::ResizeHandle MouseAnnotation::getHandleAt(const QPoint eventPos, const AnnotationDescription &ad) const
{
    ResizeHandle selected = RH_None;

    if (ad.annotation->canBeRotated()) {
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        const ResizeHandle corners[] = { RH_TopLeft, RH_TopRight, RH_BottomRight, RH_BottomLeft };
        for (const ResizeHandle corner : corners) {
            const QRect rect = getRotateHandleHitRect(corner, ad);
            if (rect.isNull() || !rect.contains(eventPos)) {
                continue;
            }
            const QPoint delta = rect.center() - eventPos;
            const qint64 distance = qint64(delta.x()) * delta.x() + qint64(delta.y()) * delta.y();
            if (distance < bestDistance) {
                bestDistance = distance;
                selected = RH_Rotate;
            }
        }
        if (selected == RH_Rotate) {
            return selected;
        }
    }

    if (ad.annotation->canBeResized()) {
        // Handles may overlap on tiny selections.  Pick the handle whose
        // center is closest to the cursor instead of OR-ing them into an
        // impossible combined handle.
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        for (const ResizeHandle &handle : m_resizeHandleList) {
            const QRect rect = getHandleHitRect(handle, ad);
            if (rect.isNull() || !rect.contains(eventPos)) {
                continue;
            }
            const QPoint delta = rect.center() - eventPos;
            const qint64 distance = qint64(delta.x()) * delta.x() + qint64(delta.y()) * delta.y();
            if (distance < bestDistance) {
                bestDistance = distance;
                selected = handle;
            }
        }
        if (selected != RH_None) {
            return selected;
        }
    }

    if (ad.annotation->canBeMoved()) {
        const QRect boundingRect = Okular::AnnotationUtils::annotationGeometry(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight());
        if (!boundingRect.isNull() && boundingRect.contains(eventPos)) {
            return RH_Content;
        }
    }

    return selected;
}

MouseAnnotation::ResizeHandle MouseAnnotation::getSelectionHandleAt(const QPoint eventPos) const
{
    const QRect boundingRect = selectedBoundingRect();
    if (boundingRect.isNull()) {
        return RH_None;
    }

    ResizeHandle selected = RH_None;

    if (selectionCanBeRotated()) {
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        const ResizeHandle corners[] = { RH_TopLeft, RH_TopRight, RH_BottomRight, RH_BottomLeft };
        for (const ResizeHandle corner : corners) {
            const QRect hitRect = getGroupRotateHandleHitRect(corner);
            if (hitRect.isNull() || !hitRect.contains(eventPos)) {
                continue;
            }
            const QPoint delta = hitRect.center() - eventPos;
            const qint64 distance = qint64(delta.x()) * delta.x() + qint64(delta.y()) * delta.y();
            if (distance < bestDistance) {
                bestDistance = distance;
                selected = RH_Rotate;
            }
        }
        if (selected == RH_Rotate) {
            return selected;
        }
    }

    if (selectionCanBeResized()) {
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        for (const ResizeHandle &handle : std::as_const(m_resizeHandleList)) {
            const QRect hitRect = getHandleHitRect(handle, boundingRect);
            if (hitRect.isNull() || !hitRect.contains(eventPos)) {
                continue;
            }
            const QPoint delta = hitRect.center() - eventPos;
            const qint64 distance = qint64(delta.x()) * delta.x() + qint64(delta.y()) * delta.y();
            if (distance < bestDistance) {
                bestDistance = distance;
                selected = handle;
            }
        }
        if (selected != RH_None) {
            return selected;
        }
    }
    if (!boundingRect.isNull() && boundingRect.contains(eventPos)
        && std::all_of(m_areaSelectedAnnotations.cbegin(), m_areaSelectedAnnotations.cend(), [](const AnnotationDescription &description) { return description.annotation->canBeMoved(); })) {
        return RH_Content;
    }
    return selected;
}

QPolygonF MouseAnnotation::focusedAnnotationOutline() const
{
    if (!m_focusedAnnotation.isValid()) {
        return {};
    }
    return Okular::AnnotationUtils::annotationOutline(m_focusedAnnotation.annotation, m_focusedAnnotation.pageViewItem->uncroppedWidth(), m_focusedAnnotation.pageViewItem->uncroppedHeight());
}

static QPointF handlePointOnOutline(const QPolygonF &outline, MouseAnnotation::ResizeHandle handle)
{
    if (outline.size() < 4) {
        return QPointF();
    }

    switch (handle) {
    case MouseAnnotation::RH_TopLeft:
        return outline.at(0);
    case MouseAnnotation::RH_TopRight:
        return outline.at(1);
    case MouseAnnotation::RH_BottomRight:
        return outline.at(2);
    case MouseAnnotation::RH_BottomLeft:
        return outline.at(3);
    case MouseAnnotation::RH_Top:
        return (outline.at(0) + outline.at(1)) / 2.0;
    case MouseAnnotation::RH_Right:
        return (outline.at(1) + outline.at(2)) / 2.0;
    case MouseAnnotation::RH_Bottom:
        return (outline.at(2) + outline.at(3)) / 2.0;
    case MouseAnnotation::RH_Left:
        return (outline.at(3) + outline.at(0)) / 2.0;
    default:
        return outline.boundingRect().center();
    }
}

QRect MouseAnnotation::getRotateHandleRect(ResizeHandle corner, const AnnotationDescription &ad) const
{
    const QPolygonF outline = Okular::AnnotationUtils::annotationOutline(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight());
    if (outline.size() < 4) {
        return {};
    }

    const QPointF cornerPoint = handlePointOnOutline(outline, corner);
    QPointF direction = cornerPoint - outline.boundingRect().center();
    const double length = std::hypot(direction.x(), direction.y());
    if (length > 1e-9) {
        direction /= length;
    } else {
        direction = QPointF(1, 1) / std::sqrt(2.0);
    }
    const QPointF handleCenter = cornerPoint + direction * rotateHandleOffset;
    return QRect(qRound(handleCenter.x()) - rotateHandleSize / 2, qRound(handleCenter.y()) - rotateHandleSize / 2, rotateHandleSize, rotateHandleSize);
}

QRect MouseAnnotation::getRotateHandleHitRect(ResizeHandle corner, const AnnotationDescription &ad) const
{
    return getRotateHandleRect(corner, ad).adjusted(-handleHitExpansion, -handleHitExpansion, handleHitExpansion, handleHitExpansion);
}

/* Get the rectangle for a specified resizie handle. */
QRect MouseAnnotation::getHandleRect(ResizeHandle handle, const AnnotationDescription &ad) const
{
    if (!qFuzzyIsNull(ad.annotation->frame().rotation)) {
        const QPolygonF outline = Okular::AnnotationUtils::annotationOutline(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight());
        if (outline.size() >= 4 && handle != RH_Content && handle != RH_Rotate) {
            const QPointF point = handlePointOnOutline(outline, handle);
            const int width = handle == RH_Top || handle == RH_Bottom ? handleLongSize : handle == RH_Left || handle == RH_Right ? handleShortSize : handleSize;
            const int height = handle == RH_Left || handle == RH_Right ? handleLongSize : handle == RH_Top || handle == RH_Bottom ? handleShortSize : handleSize;
            return QRect(qRound(point.x()) - width / 2, qRound(point.y()) - height / 2, width, height);
        }
    }

    const QRect boundingRect = Okular::AnnotationUtils::annotationGeometry(ad.annotation, ad.pageViewItem->uncroppedWidth(), ad.pageViewItem->uncroppedHeight());
    return getHandleRect(handle, boundingRect);
}

QRect MouseAnnotation::getHandleRect(ResizeHandle handle, const QRect &boundingRect) const
{
    const int width = handle == RH_Top || handle == RH_Bottom ? handleLongSize : handle == RH_Left || handle == RH_Right ? handleShortSize : handleSize;
    const int height = handle == RH_Left || handle == RH_Right ? handleLongSize : handle == RH_Top || handle == RH_Bottom ? handleShortSize : handleSize;
    const int centerX = (handle & RH_Left) ? boundingRect.left() : (handle & RH_Right) ? boundingRect.right() : boundingRect.left() + boundingRect.width() / 2;
    const int centerY = (handle & RH_Top) ? boundingRect.top() : (handle & RH_Bottom) ? boundingRect.bottom() : boundingRect.top() + boundingRect.height() / 2;
    return QRect(centerX - width / 2, centerY - height / 2, width, height);
}

QRect MouseAnnotation::getHandleHitRect(ResizeHandle handle, const AnnotationDescription &ad) const
{
    return getHandleRect(handle, ad).adjusted(-handleHitExpansion, -handleHitExpansion, handleHitExpansion, handleHitExpansion);
}

QRect MouseAnnotation::getHandleHitRect(ResizeHandle handle, const QRect &boundingRect) const
{
    return getHandleRect(handle, boundingRect).adjusted(-handleHitExpansion, -handleHitExpansion, handleHitExpansion, handleHitExpansion);
}

Okular::NormalizedRect MouseAnnotation::resizedRect(const Okular::NormalizedRect &startRect, const QPointF &delta1, const QPointF &delta2, bool preserveAspectRatio) const
{
    const double width = startRect.right - startRect.left;
    const double height = startRect.bottom - startRect.top;
    const Okular::Page *page = m_focusedAnnotation.pageViewItem ? m_focusedAnnotation.pageViewItem->page() : nullptr;
    const double pageWidth = page ? page->width() : 1.0;
    const double pageHeight = page ? page->height() : 1.0;
    const double minimumWidth = AnnotationWidthPresets::minimumNormalizedResizeDimensionForPagePoints(pageWidth);
    const double minimumHeight = AnnotationWidthPresets::minimumNormalizedResizeDimensionForPagePoints(pageHeight);
    const double minWidth = width > 0.0 ? qMin(minimumWidth, width) : minimumWidth;
    const double minHeight = height > 0.0 ? qMin(minimumHeight, height) : minimumHeight;

    const double maximumLeft = qMax(0.0, startRect.right - minWidth);
    double left = qBound(0.0, startRect.left + delta1.x(), maximumLeft);
    const double minimumRight = qMin(1.0, left + minWidth);
    double right = qBound(minimumRight, startRect.right + delta2.x(), 1.0);
    const double maximumTop = qMax(0.0, startRect.bottom - minHeight);
    double top = qBound(0.0, startRect.top + delta1.y(), maximumTop);
    const double minimumBottom = qMin(1.0, top + minHeight);
    double bottom = qBound(minimumBottom, startRect.bottom + delta2.y(), 1.0);

    if (!preserveAspectRatio || width <= 0.0 || height <= 0.0) {
        return Okular::NormalizedRect(left, top, right, bottom);
    }

    const ResizeHandle effectiveHandle = page ? rotateHandle(m_handle, page->rotation()) : m_handle;
    const bool movesLeft = effectiveHandle & RH_Left;
    const bool movesRight = effectiveHandle & RH_Right;
    const bool movesTop = effectiveHandle & RH_Top;
    const bool movesBottom = effectiveHandle & RH_Bottom;
    const double minimumScale = qMax(minWidth / width, minHeight / height);
    double scale = 1.0;
    double maximumScale = std::numeric_limits<double>::max();

    if ((movesLeft || movesRight) && (movesTop || movesBottom)) {
        const double anchorX = movesLeft ? startRect.right : startRect.left;
        const double anchorY = movesTop ? startRect.bottom : startRect.top;
        const double movingX = movesLeft ? left : right;
        const double movingY = movesTop ? top : bottom;
        scale = qMax(qAbs(movingX - anchorX) / width, qAbs(movingY - anchorY) / height);
        maximumScale = qMin(movesLeft ? anchorX / width : (1.0 - anchorX) / width,
                            movesTop ? anchorY / height : (1.0 - anchorY) / height);
        maximumScale = qMax(minimumScale, maximumScale);
        scale = qBound(minimumScale, scale, maximumScale);
        left = movesLeft ? anchorX - width * scale : startRect.left;
        right = movesRight ? anchorX + width * scale : startRect.right;
        top = movesTop ? anchorY - height * scale : startRect.top;
        bottom = movesBottom ? anchorY + height * scale : startRect.bottom;
    } else if (movesLeft || movesRight) {
        const double anchorX = movesLeft ? startRect.right : startRect.left;
        const double centerY = (startRect.top + startRect.bottom) / 2.0;
        const double movingX = movesLeft ? left : right;
        scale = qAbs(movingX - anchorX) / width;
        maximumScale = qMin(movesLeft ? anchorX / width : (1.0 - anchorX) / width,
                            qMin(2.0 * centerY / height, 2.0 * (1.0 - centerY) / height));
        maximumScale = qMax(minimumScale, maximumScale);
        scale = qBound(minimumScale, scale, maximumScale);
        left = movesLeft ? anchorX - width * scale : startRect.left;
        right = movesRight ? anchorX + width * scale : startRect.right;
        top = centerY - height * scale / 2.0;
        bottom = centerY + height * scale / 2.0;
    } else if (movesTop || movesBottom) {
        const double anchorY = movesTop ? startRect.bottom : startRect.top;
        const double centerX = (startRect.left + startRect.right) / 2.0;
        const double movingY = movesTop ? top : bottom;
        scale = qAbs(movingY - anchorY) / height;
        maximumScale = qMin(movesTop ? anchorY / height : (1.0 - anchorY) / height,
                            qMin(2.0 * centerX / width, 2.0 * (1.0 - centerX) / width));
        maximumScale = qMax(minimumScale, maximumScale);
        scale = qBound(minimumScale, scale, maximumScale);
        top = movesTop ? anchorY - height * scale : startRect.top;
        bottom = movesBottom ? anchorY + height * scale : startRect.bottom;
        left = centerX - width * scale / 2.0;
        right = centerX + width * scale / 2.0;
    }

    return Okular::NormalizedRect(left, top, right, bottom);
}

bool MouseAnnotation::selectionCanBeResized() const
{
    return m_areaSelectedAnnotations.size() > 1
        && std::all_of(m_areaSelectedAnnotations.cbegin(), m_areaSelectedAnnotations.cend(), [](const AnnotationDescription &description) { return description.annotation->canBeResized(); });
}

/* Convert a resize handle delta into two adjust delta coordinates. */
void MouseAnnotation::handleToAdjust(const QPointF dIn, QPointF &dOut1, QPointF &dOut2, MouseAnnotation::ResizeHandle handle, Okular::Rotation rotation)
{
    const MouseAnnotation::ResizeHandle rotatedHandle = MouseAnnotation::rotateHandle(handle, rotation);
    dOut1.rx() = (rotatedHandle & MouseAnnotation::RH_Left) ? dIn.x() : 0;
    dOut1.ry() = (rotatedHandle & MouseAnnotation::RH_Top) ? dIn.y() : 0;
    dOut2.rx() = (rotatedHandle & MouseAnnotation::RH_Right) ? dIn.x() : 0;
    dOut2.ry() = (rotatedHandle & MouseAnnotation::RH_Bottom) ? dIn.y() : 0;
}

QPointF MouseAnnotation::rotateInRect(const QPointF rotated, Okular::Rotation rotation)
{
    QPointF ret;

    switch (rotation) {
    case Okular::Rotation90:
        ret = QPointF(rotated.y(), -rotated.x());
        break;
    case Okular::Rotation180:
        ret = QPointF(-rotated.x(), -rotated.y());
        break;
    case Okular::Rotation270:
        ret = QPointF(-rotated.y(), rotated.x());
        break;
    case Okular::Rotation0: /* no modifications */
    default:                /* other cases */
        ret = rotated;
    }

    return ret;
}

MouseAnnotation::ResizeHandle MouseAnnotation::rotateHandle(MouseAnnotation::ResizeHandle handle, Okular::Rotation rotation)
{
    unsigned int rotatedHandle = 0;
    switch (rotation) {
    case Okular::Rotation90:
        /* bit rotation: #1 => #4, #2 => #1, #3 => #2, #4 => #3 */
        rotatedHandle = (handle << 3 | handle >> (4 - 3)) & RH_AllHandles;
        break;
    case Okular::Rotation180:
        /* bit rotation: #1 => #3, #2 => #4, #3 => #1, #4 => #2 */
        rotatedHandle = (handle << 2 | handle >> (4 - 2)) & RH_AllHandles;
        break;
    case Okular::Rotation270:
        /* bit rotation: #1 => #2, #2 => #3, #3 => #4, #4 => #1 */
        rotatedHandle = (handle << 1 | handle >> (4 - 1)) & RH_AllHandles;
        break;
    case Okular::Rotation0: /* no modifications */
    default:                /* other cases */
        rotatedHandle = handle;
        break;
    }
    return (MouseAnnotation::ResizeHandle)rotatedHandle;
}

/* Start according action for AMovie/ARichMedia/AScreen/AFileAttachment.
 * It was formerly (before mouse annotation refactoring) called on mouse release event.
 * Now it's called on mouse press. Should we keep the former behavior? */
void MouseAnnotation::processAction(const AnnotationDescription &ad)
{
    if (ad.isValid()) {
        Okular::Annotation *ann = ad.annotation;
        PageViewItem *pageItem = ad.pageViewItem;

        if (ann->subType() == Okular::Annotation::AMovie) {
            VideoWidget *vw = pageItem->videoWidgets().value(static_cast<Okular::MovieAnnotation *>(ann)->movie());
            vw->show();
            vw->play();
        } else if (ann->subType() == Okular::Annotation::ARichMedia) {
            VideoWidget *vw = pageItem->videoWidgets().value(static_cast<Okular::RichMediaAnnotation *>(ann)->movie());
            vw->show();
            vw->play();
        } else if (ann->subType() == Okular::Annotation::AScreen) {
            m_document->processAction(static_cast<Okular::ScreenAnnotation *>(ann)->action());
        } else if (ann->subType() == Okular::Annotation::AFileAttachment) {
            const Okular::FileAttachmentAnnotation *fileAttachAnnot = static_cast<Okular::FileAttachmentAnnotation *>(ann);
            GuiUtils::saveEmbeddedFile(fileAttachAnnot->embeddedFile(), m_pageView);
        }
    }
}

#include "moc_pageviewmouseannotation.cpp"
