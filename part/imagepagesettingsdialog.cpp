/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagepagesettingsdialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>

namespace
{
constexpr int kCustomIndex = 4; // after the four named presets

int fitIndex(ImageFit fit)
{
    switch (fit) {
    case ImageFit::FillPage:
        return 1;
    case ImageFit::OriginalSize:
        return 2;
    case ImageFit::FitPage:
    default:
        return 0;
    }
}

ImageFit fitFromIndex(int index)
{
    switch (index) {
    case 1:
        return ImageFit::FillPage;
    case 2:
        return ImageFit::OriginalSize;
    default:
        return ImageFit::FitPage;
    }
}
}

const QList<ImagePageSettingsDialog::Preset> &ImagePageSettingsDialog::presets()
{
    static const QList<Preset> list = {
        {"A4", 595.0, 842.0},
        {"A3", 842.0, 1191.0},
        {"A5", 420.0, 595.0},
        {"Letter", 612.0, 792.0},
    };
    return list;
}

ImagePageSettingsDialog::ImagePageSettingsDialog(const ImagePageSettings &initial, const QSize &imageSizePixels, QWidget *parent)
    : QDialog(parent)
    , m_imageSizePixels(imageSizePixels)
{
    setObjectName(QStringLiteral("imagePageSettingsDialog"));
    setWindowTitle(i18n("Image Page Setup"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_fitCombo = new QComboBox(this);
    m_fitCombo->setObjectName(QStringLiteral("imagePageFitCombo"));
    m_fitCombo->addItem(i18n("Fit page"), int(ImageFit::FitPage));
    m_fitCombo->addItem(i18n("Fill page (crop)"), int(ImageFit::FillPage));
    m_fitCombo->addItem(i18n("Original size"), int(ImageFit::OriginalSize));
    form->addRow(i18n("Scaling:"), m_fitCombo);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->setObjectName(QStringLiteral("imagePagePresetCombo"));
    for (const Preset &preset : presets()) {
        m_presetCombo->addItem(QString::fromLatin1(preset.name));
    }
    m_presetCombo->addItem(i18n("Custom"));
    form->addRow(i18n("Page size:"), m_presetCombo);

    m_orientationCombo = new QComboBox(this);
    m_orientationCombo->setObjectName(QStringLiteral("imagePageOrientationCombo"));
    m_orientationCombo->addItem(i18n("Portrait"));
    m_orientationCombo->addItem(i18n("Landscape"));
    form->addRow(i18n("Orientation:"), m_orientationCombo);

    m_widthSpin = new QDoubleSpinBox(this);
    m_widthSpin->setObjectName(QStringLiteral("imagePageWidthSpin"));
    m_widthSpin->setRange(kMinDimension, kMaxDimension);
    m_widthSpin->setDecimals(1);
    m_widthSpin->setSuffix(i18n(" pt"));
    form->addRow(i18n("Width:"), m_widthSpin);

    m_heightSpin = new QDoubleSpinBox(this);
    m_heightSpin->setObjectName(QStringLiteral("imagePageHeightSpin"));
    m_heightSpin->setRange(kMinDimension, kMaxDimension);
    m_heightSpin->setDecimals(1);
    m_heightSpin->setSuffix(i18n(" pt"));
    form->addRow(i18n("Height:"), m_heightSpin);

    m_marginSpin = new QDoubleSpinBox(this);
    m_marginSpin->setObjectName(QStringLiteral("imagePageMarginSpin"));
    m_marginSpin->setRange(0.0, 500.0);
    m_marginSpin->setDecimals(1);
    m_marginSpin->setSuffix(i18n(" pt"));
    form->addRow(i18n("Margin:"), m_marginSpin);

    m_backgroundButton = new QPushButton(this);
    m_backgroundButton->setObjectName(QStringLiteral("imagePageBackgroundButton"));
    m_backgroundButton->setAutoFillBackground(true);
    form->addRow(i18n("Background:"), m_backgroundButton);

    m_originalHint = new QLabel(this);
    m_originalHint->setWordWrap(true);
    m_originalHint->hide();
    form->addRow(QString(), m_originalHint);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(i18n("Apply to selected images"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Seed the controls from the initial settings.
    m_background = initial.background.isValid() ? initial.background : QColor(Qt::white);
    m_marginSpin->setValue(qBound(0.0, initial.margin, 500.0));
    m_dpi = initial.dpi > 0 ? initial.dpi : 96;
    m_fitCombo->setCurrentIndex(fitIndex(initial.fit));

    const QSizeF pageSize = initial.pageSize.isValid() ? initial.pageSize : QSizeF(595.0, 842.0);
    int matchingPreset = kCustomIndex;
    for (int i = 0; i < presets().size(); ++i) {
        const Preset &preset = presets().at(i);
        const bool portrait = qAbs(pageSize.width() - preset.width) < 1.0 && qAbs(pageSize.height() - preset.height) < 1.0;
        const bool landscape = qAbs(pageSize.width() - preset.height) < 1.0 && qAbs(pageSize.height() - preset.width) < 1.0;
        if (portrait || landscape) {
            matchingPreset = i;
            m_orientationCombo->setCurrentIndex(landscape ? 1 : 0);
            break;
        }
    }
    m_presetCombo->setCurrentIndex(matchingPreset);
    m_widthSpin->setValue(qBound(kMinDimension, pageSize.width(), kMaxDimension));
    m_heightSpin->setValue(qBound(kMinDimension, pageSize.height(), kMaxDimension));

    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, &ImagePageSettingsDialog::applyPreset);
    connect(m_orientationCombo, &QComboBox::currentIndexChanged, this, &ImagePageSettingsDialog::updateOrientationForPreset);
    connect(m_fitCombo, &QComboBox::currentIndexChanged, this, &ImagePageSettingsDialog::updateEnabledState);
    connect(m_marginSpin, &QDoubleSpinBox::valueChanged, this, &ImagePageSettingsDialog::updateEnabledState);
    connect(m_backgroundButton, &QPushButton::clicked, this, &ImagePageSettingsDialog::chooseBackground);

    updateBackgroundButton();
    updateEnabledState();
}

void ImagePageSettingsDialog::applyPreset(int index)
{
    if (index >= 0 && index < presets().size()) {
        const Preset &preset = presets().at(index);
        const bool landscape = m_orientationCombo->currentIndex() == 1;
        m_widthSpin->setValue(landscape ? preset.height : preset.width);
        m_heightSpin->setValue(landscape ? preset.width : preset.height);
    }
    updateEnabledState();
}

void ImagePageSettingsDialog::updateOrientationForPreset()
{
    const int index = m_presetCombo->currentIndex();
    if (index >= 0 && index < presets().size()) {
        applyPreset(index);
    }
}

void ImagePageSettingsDialog::updateEnabledState()
{
    const ImageFit fit = fitFromIndex(m_fitCombo->currentIndex());
    const bool original = fit == ImageFit::OriginalSize;
    const bool custom = m_presetCombo->currentIndex() == kCustomIndex;

    m_fitCombo->setEnabled(true);
    m_presetCombo->setEnabled(!original);
    m_orientationCombo->setEnabled(!original && !custom);
    // Custom sizes are typed in; named presets and "Original" derive them.
    m_widthSpin->setReadOnly(!custom);
    m_heightSpin->setReadOnly(!custom);
    m_widthSpin->setEnabled(!original);
    m_heightSpin->setEnabled(!original);
    m_marginSpin->setEnabled(true);

    if (original) {
        const QSizeF size = DocumentBuilderExport::originalPageSizeForImage(m_imageSizePixels, m_marginSpin->value(), 96);
        if (size.isValid()) {
            m_originalHint->setText(i18n("The page will be %1 × %2 pt: the image at 96 dpi plus the margin.",
                                         QString::number(size.width(), 'f', 1),
                                         QString::number(size.height(), 'f', 1)));
        } else {
            m_originalHint->setText(i18n("The page will follow the image's own size."));
        }
        m_originalHint->show();
    } else {
        m_originalHint->hide();
    }
}

void ImagePageSettingsDialog::chooseBackground()
{
    const QColor chosen = QColorDialog::getColor(m_background, this, i18n("Image Background"));
    if (chosen.isValid()) {
        m_background = chosen;
        updateBackgroundButton();
    }
}

void ImagePageSettingsDialog::updateBackgroundButton()
{
    m_backgroundButton->setText(m_background.name());
    m_backgroundButton->setStyleSheet(QStringLiteral("background-color: %1; color: %2;").arg(m_background.name(), m_background.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
}

ImagePageSettings ImagePageSettingsDialog::settings() const
{
    ImagePageSettings settings;
    settings.pageSize = QSizeF(m_widthSpin->value(), m_heightSpin->value());
    settings.background = m_background;
    settings.fit = fitFromIndex(m_fitCombo->currentIndex());
    settings.margin = m_marginSpin->value();
    // dpi is kept from the image's current setup; 4.4 does not expose it as a control.
    settings.dpi = m_dpi;
    return settings;
}
