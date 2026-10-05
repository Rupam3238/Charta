/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_ANNOTATIONCOLORHISTORY_H
#define OKULAR_ANNOTATIONCOLORHISTORY_H

#include <QColor>
#include <QList>

namespace Okular::AnnotationColorHistory
{
QList<QColor> recentColors();
void addRecentColor(const QColor &color);
}

#endif