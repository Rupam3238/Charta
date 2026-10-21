/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#ifndef OKULAR_ANNOTATIONCOLORHISTORY_H
#define OKULAR_ANNOTATIONCOLORHISTORY_H

#include <QColor>
#include <QList>

#include "okularpart_export.h"

namespace Okular::AnnotationColorHistory
{
OKULARPART_EXPORT QList<QColor> recentColors();
OKULARPART_EXPORT void addRecentColor(const QColor &color);
OKULARPART_EXPORT bool colorsEqual(const QColor &c1, const QColor &c2);
OKULARPART_EXPORT QColor nextCycledColor(const QList<QColor> &history, const QColor &currentColor);
}

#endif