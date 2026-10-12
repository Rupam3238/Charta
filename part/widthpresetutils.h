/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_WIDTHPRESETUTILS_H
#define OKULAR_WIDTHPRESETUTILS_H

#include <QList>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace AnnotationWidthPresets
{
inline constexpr double minimumWidth = 0.5;
inline constexpr double maximumWidth = 64.0;
inline constexpr double adjustmentStep = 0.5;

class SelectionTracker
{
public:
    bool activate(qsizetype index)
    {
        const bool editSelected = index == m_selectedIndex;
        m_selectedIndex = index;
        return editSelected;
    }

    void setSelectedIndex(qsizetype index)
    {
        m_selectedIndex = index;
    }

    qsizetype selectedIndex() const
    {
        return m_selectedIndex;
    }

private:
    qsizetype m_selectedIndex = -1;
};

inline const QStringList &defaultMenuPresetStrings()
{
    static const QStringList values {QStringLiteral("0.5"), QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2"), QStringLiteral("2.5"), QStringLiteral("3"),
                                     QStringLiteral("3.5"), QStringLiteral("4"), QStringLiteral("4.5"), QStringLiteral("5"), QStringLiteral("20"), QStringLiteral("50")};
    return values;
}

inline const QStringList &defaultToolPresetStrings()
{
    static const QStringList values {QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("20"), QStringLiteral("50")};
    return values;
}

inline bool parsePresetStrings(const QStringList &strings, QList<double> *values)
{
    if (!values || strings.isEmpty()) {
        return false;
    }

    QList<double> parsed;
    parsed.reserve(strings.size());
    for (const QString &string : strings) {
        bool ok = false;
        const double value = QLocale::c().toDouble(string.trimmed(), &ok);
        if (!ok || !std::isfinite(value) || value < minimumWidth || value > maximumWidth) {
            return false;
        }
        parsed.append(value);
    }

    std::sort(parsed.begin(), parsed.end());
    for (qsizetype index = 1; index < parsed.size(); ++index) {
        if (qFuzzyCompare(parsed[index - 1], parsed[index])) {
            return false;
        }
    }
    *values = std::move(parsed);
    return true;
}

inline QList<double> presetValuesOrDefaults(const QStringList &strings, const QStringList &defaults)
{
    QList<double> values;
    if (!parsePresetStrings(strings, &values) && !parsePresetStrings(defaults, &values)) {
        return {};
    }
    return values;
}

inline bool replacePresetString(const QStringList &strings, qsizetype index, const QString &replacement, QStringList *updated)
{
    if (!updated || index < 0 || index >= strings.size()) {
        return false;
    }
    QStringList candidate = strings;
    candidate[index] = replacement.trimmed();
    QList<double> parsed;
    if (!parsePresetStrings(candidate, &parsed)) {
        return false;
    }
    updated->clear();
    updated->reserve(parsed.size());
    for (const double value : parsed) {
        updated->append(QLocale::c().toString(value, 'g', 12));
    }
    return true;
}

inline QStringList normalizedPresetStrings(const QStringList &strings, const QStringList &defaults)
{
    const QList<double> values = presetValuesOrDefaults(strings, defaults);
    QStringList normalized;
    normalized.reserve(values.size());
    for (const double value : values) {
        normalized.append(QLocale::c().toString(value, 'g', 12));
    }
    return normalized;
}

inline double minimumNormalizedResizeDimension(double pageExtent)
{
    return pageExtent > 0.0 ? minimumWidth / pageExtent : minimumWidth;
}
}

#endif
