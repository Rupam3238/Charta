/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationcolorhistory.h"

#include "settings.h"

#include <QStringList>
#include <algorithm>

namespace Okular::AnnotationColorHistory
{
namespace
{
constexpr qsizetype MaximumRecentColors = 8;
}

bool colorsEqual(const QColor &c1, const QColor &c2)
{
    return c1.isValid() && c2.isValid() && c1.toRgb().rgba() == c2.toRgb().rgba();
}

QList<QColor> recentColors()
{
    QList<QColor> colors;
    for (const QString &value : Settings::recentAnnotationColors()) {
        const QColor color(value);
        if (color.isValid()) {
            const bool alreadyPresent = std::any_of(colors.cbegin(), colors.cend(), [&color](const QColor &c) {
                return colorsEqual(c, color);
            });
            if (!alreadyPresent) {
                colors.append(color);
                if (colors.size() == MaximumRecentColors) {
                    break;
                }
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
    colors.removeIf([&color](const QColor &c) {
        return colorsEqual(c, color);
    });
    colors.prepend(color);
    while (colors.size() > MaximumRecentColors) {
        colors.removeLast();
    }

    QStringList values;
    values.reserve(colors.size());
    for (const QColor &recentColor : colors) {
        values.append(recentColor.toRgb().name(QColor::HexArgb));
    }
    if (Settings::recentAnnotationColors() != values) {
        Settings::setRecentAnnotationColors(values);
        Settings::self()->save();
    }
}

QColor nextCycledColor(const QList<QColor> &history, const QColor &currentColor)
{
    if (history.isEmpty()) {
        return currentColor;
    }
    if (history.size() == 1) {
        return currentColor;
    }

    int currentIndex = -1;
    for (int i = 0; i < history.size(); ++i) {
        if (colorsEqual(history.at(i), currentColor)) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex == -1 || currentIndex == history.size() - 1) {
        return history.first();
    }
    return history.at(currentIndex + 1);
}

}