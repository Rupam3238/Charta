/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationcolorhistory.h"

#include "settings.h"

#include <QStringList>

namespace Okular::AnnotationColorHistory
{
namespace
{
constexpr qsizetype MaximumRecentColors = 8;
}

QList<QColor> recentColors()
{
    QList<QColor> colors;
    for (const QString &value : Settings::recentAnnotationColors()) {
        const QColor color(value);
        if (color.isValid() && !colors.contains(color)) {
            colors.append(color);
            if (colors.size() == MaximumRecentColors) {
                break;
            }
        }
    }
    return colors;
}

void addRecentColor(const QColor &color)
{
    if (!color.isValid()) {
        return;
    }

    QList<QColor> colors = recentColors();
    colors.removeAll(color);
    colors.prepend(color);
    while (colors.size() > MaximumRecentColors) {
        colors.removeLast();
    }

    QStringList values;
    values.reserve(colors.size());
    for (const QColor &recentColor : colors) {
        values.append(recentColor.name(QColor::HexArgb));
    }
    if (Settings::recentAnnotationColors() != values) {
        Settings::setRecentAnnotationColors(values);
        Settings::self()->save();
    }
}
}