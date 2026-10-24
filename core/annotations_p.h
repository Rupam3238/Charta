/*
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_ANNOTATIONS_P_H
#define OKULAR_ANNOTATIONS_P_H

#include "annotations.h"
#include "area.h"

// qt/kde includes
#include <QColor>
#include <QDateTime>
#include <QString>
#include <QVariant>

class QTransform;

namespace Okular
{
class PagePrivate;

class AnnotationPrivate
{
public:
    AnnotationPrivate();

    CHARTACORE_EXPORT static AnnotationPrivate *get(Annotation *a);

    virtual ~AnnotationPrivate();

    AnnotationPrivate(const AnnotationPrivate &) = delete;
    AnnotationPrivate &operator=(const AnnotationPrivate &) = delete;

    /**
     * Transforms the annotation coordinates with the transformation
     * defined by @p matrix.
     */
    void annotationTransform(const QTransform &matrix);

    virtual void transform(const QTransform &matrix);
    virtual void baseTransform(const QTransform &matrix);
    virtual void resetTransformation();
    virtual void translate(const NormalizedPoint &coord);
    virtual void adjust(const NormalizedPoint &deltaCoord1, const NormalizedPoint &deltaCoord2);
    virtual bool openDialogAfterCreation() const;
    virtual void setAnnotationProperties(const QDomNode &node);
    virtual bool canBeResized() const;
    virtual AnnotationPrivate *getNewAnnotationPrivate() = 0;

    /**
     * Annotation::setAnnotationProperties() (used by every undo/redo of a property change) replaces the whole
     * private object with a fresh one filled from XML. Session-only state that is not part of the XML
     * (generator appearance streams, pressure profiles, ...) would be lost; subclasses override this to take
     * it over from @p previous. Called after the XML has been applied to the new object.
     */
    virtual void adoptSessionStateFrom(const AnnotationPrivate *previous)
    {
        Q_UNUSED(previous)
    }

    /**
     * Maps subtype content from @p oldFrame to @p newFrame.  The default is a
     * no-op for annotations whose appearance is generated from the frame.
     */
    virtual void scaleContent(const AnnotationFrame &oldFrame, const AnnotationFrame &newFrame);

    /**
     * Rotates subtype content by @p delta radians around @p center.  The
     * center is normalized page coordinates.
     */
    virtual void rotateContent(double delta, const NormalizedPoint &center);

    bool setFrameInternal(const AnnotationFrame &frame);
    void recomputeBoundaryFromFrame();
    void updateFrameFromBoundary();
    double framePageWidth() const;
    double framePageHeight() const;

    /**
     * Determines the distance of the closest point of the annotation to the
     * given point @p x @p y @p xScale @p yScale
     * @since 0.17
     */
    virtual double distanceSqr(double x, double y, double xScale, double yScale) const;

    PagePrivate *m_page;

    QString m_author;
    QString m_contents;
    QString m_uniqueName;
    QDateTime m_modifyDate;
    QDateTime m_creationDate;

    int m_flags;
    NormalizedRect m_boundary;
    NormalizedRect m_transformedBoundary;

    // Oriented frame, stored in the unrotated page coordinate system.
    NormalizedPoint m_frameCenter;
    QSizeF m_frameSize;
    double m_rotation = 0.0;
    bool m_frameInitialized = false;

    Okular::Annotation::Style m_style;
    Okular::Annotation::Window m_window;
    Okular::Annotation *m_previousRevision = nullptr;
    QList<Okular::Annotation::Revision> m_revisions;

    Annotation::DisposeDataFunction m_disposeFunc;
    QVariant m_nativeId;
    std::shared_ptr<const void> m_nativeData;
};

}

#endif
