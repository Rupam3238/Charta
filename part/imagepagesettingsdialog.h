/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)

    Document Builder: per-page setup for the image pages of a selection.

    Page sizes are in PostScript points (1/72 in). Fit / Fill / Original map directly onto
    ImageFit; for Original the page follows the image, so the size controls are locked.
*/

#ifndef IMAGEPAGESETTINGSDIALOG_H
#define IMAGEPAGESETTINGSDIALOG_H

#include <QColor>
#include <QDialog>
#include <QSize>

#include "documentbuildermodel.h"
#include "okularpart_export.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class OKULARPART_EXPORT ImagePageSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// @p initial is usually the setup of the first selected image; @p imageSizePixels is its pixel
    /// size, used only to preview what "Original size" will produce (may be invalid).
    explicit ImagePageSettingsDialog(const ImagePageSettings &initial, const QSize &imageSizePixels, QWidget *parent = nullptr);

    ImagePageSettings settings() const;

    /// The size presets the dialog offers, in points (portrait). "Custom" is handled separately.
    struct Preset {
        const char *name;
        qreal width;
        qreal height;
    };
    static const QList<Preset> &presets();

    /// Smallest and largest accepted page dimension, in points (0.5 in .. 200 in).
    static constexpr qreal kMinDimension = 36.0;
    static constexpr qreal kMaxDimension = 14400.0;

private:
    void applyPreset(int index);
    void updateOrientationForPreset();
    void updateEnabledState();
    void chooseBackground();
    void updateBackgroundButton();

    QSize m_imageSizePixels;
    QColor m_background = Qt::white;
    int m_dpi = 96;

    QComboBox *m_presetCombo = nullptr;
    QComboBox *m_orientationCombo = nullptr;
    QDoubleSpinBox *m_widthSpin = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_marginSpin = nullptr;
    QComboBox *m_fitCombo = nullptr;
    QPushButton *m_backgroundButton = nullptr;
    QLabel *m_originalHint = nullptr;
};

#endif // IMAGEPAGESETTINGSDIALOG_H
