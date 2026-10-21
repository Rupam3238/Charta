/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)

    Document Builder: the handful of appearance settings the builder remembers.

    The values live in the `[DocumentBuilder]` group of the application config file, so they survive a
    restart without a dialog of their own: the Configure popup in the builder toolbar writes them
    straight through. Header-only on purpose — the parsing is trivial and keeping it out of a
    translation unit means the grid code and any future test can share one definition.
*/

#ifndef DOCUMENTBUILDERCONFIG_H
#define DOCUMENTBUILDERCONFIG_H

#include <KConfigGroup>
#include <KSharedConfig>

#include <QtGlobal>

namespace DocumentBuilderConfig
{
/// `Columns` value meaning "as many readable columns as the viewport fits".
constexpr int kAutoColumns = 0;
constexpr int kMinColumns = 2;
constexpr int kMaxColumns = 6;

/// Card size presets. The grid derives its cell size from this, the column count and the caption.
enum class ThumbnailSize {
    Small = 0,
    Medium = 1,
    Large = 2,
};

/// Multiplier applied to the derived cell height. The page thumbnail itself keeps its aspect.
inline qreal thumbnailScale(ThumbnailSize size)
{
    switch (size) {
    case ThumbnailSize::Small:
        return 0.8;
    case ThumbnailSize::Large:
        return 1.3;
    case ThumbnailSize::Medium:
    default:
        return 1.0;
    }
}

struct Settings {
    int columns = kAutoColumns; ///< kAutoColumns, or kMinColumns..kMaxColumns
    ThumbnailSize thumbnailSize = ThumbnailSize::Medium;
    bool showFileNames = true;

    bool operator==(const Settings &other) const
    {
        return columns == other.columns && thumbnailSize == other.thumbnailSize && showFileNames == other.showFileNames;
    }
    bool operator!=(const Settings &other) const
    {
        return !(*this == other);
    }
};

inline KConfigGroup builderConfigGroup()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("DocumentBuilder"));
}

/// Reads the settings, clamping anything a hand-edited config file could contain.
inline Settings loadSettings()
{
    const KConfigGroup config = builderConfigGroup();

    Settings settings;
    const int columns = config.readEntry(QStringLiteral("Columns"), kAutoColumns);
    // A value of 1 would make a column of three-metre cards; treat it as "auto" rather than obey.
    if (columns == 1 || columns < kAutoColumns || columns > kMaxColumns) {
        settings.columns = kAutoColumns;
    } else {
        settings.columns = columns;
    }

    const int size = config.readEntry(QStringLiteral("ThumbnailSize"), int(ThumbnailSize::Medium));
    if (size < int(ThumbnailSize::Small) || size > int(ThumbnailSize::Large)) {
        settings.thumbnailSize = ThumbnailSize::Medium;
    } else {
        settings.thumbnailSize = ThumbnailSize(size);
    }

    settings.showFileNames = config.readEntry(QStringLiteral("ShowFileNames"), true);
    return settings;
}

inline void saveSettings(const Settings &settings)
{
    KConfigGroup config = builderConfigGroup();
    config.writeEntry(QStringLiteral("Columns"), settings.columns);
    config.writeEntry(QStringLiteral("ThumbnailSize"), int(settings.thumbnailSize));
    config.writeEntry(QStringLiteral("ShowFileNames"), settings.showFileNames);
    config.sync();
}
}

#endif // DOCUMENTBUILDERCONFIG_H
