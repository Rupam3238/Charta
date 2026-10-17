/*
    SPDX-FileCopyrightText: 2024 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "newpagedialog.h"

// qt/kde includes
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <cmath>
#include <utility>

namespace Okular
{

namespace
{
bool nearlyEqual(double a, double b)
{
    return std::abs(a - b) < 0.5;
}

// The background color is remembered between dialogs (and sessions): a new page starts with the color used last
KConfigGroup newPageConfigGroup()
{
    return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("okularrc")), QStringLiteral("New Page Dialog"));
}
}

class NewPageDialog::Private
{
public:
    QComboBox *positionCombo = nullptr;
    QComboBox *presetCombo = nullptr;
    QDoubleSpinBox *widthSpinBox = nullptr;
    QDoubleSpinBox *heightSpinBox = nullptr;
    QRadioButton *portraitRadio = nullptr;
    QRadioButton *landscapeRadio = nullptr;
    QPushButton *colorButton = nullptr;
    QSpinBox *countSpinBox = nullptr;

    QColor backgroundColor = Qt::white;
    bool hasReference = false;
    // set while the controls are changed by the code, to ignore their signals
    bool updating = false;
};

NewPageDialog::NewPageDialog(QWidget *parent, const QSizeF &referenceSize, InsertPosition position)
    : QDialog(parent)
    , d(new Private)
{
    setWindowTitle(i18n("Add New Page"));
    setModal(true);

    const QColor rememberedColor = newPageConfigGroup().readEntry("BackgroundColor", QColor(Qt::white));
    if (rememberedColor.isValid()) {
        d->backgroundColor = rememberedColor;
    }

    d->hasReference = referenceSize.isValid() && referenceSize.width() >= 10.0 && referenceSize.height() >= 10.0;

    auto *mainLayout = new QVBoxLayout(this);

    // Position group: where the new pages go relative to the page the dialog was opened for
    auto *positionGroup = new QGroupBox(i18n("Position"), this);
    auto *positionLayout = new QHBoxLayout(positionGroup);
    positionLayout->addWidget(new QLabel(i18n("Insert the new pages:"), positionGroup));
    d->positionCombo = new QComboBox(positionGroup);
    d->positionCombo->addItem(i18n("After this page"), static_cast<int>(InsertAfter));
    d->positionCombo->addItem(i18n("Before this page"), static_cast<int>(InsertBefore));
    d->positionCombo->setCurrentIndex(position == InsertBefore ? 1 : 0);
    positionLayout->addWidget(d->positionCombo);
    positionLayout->addStretch();
    mainLayout->addWidget(positionGroup);

    // Page size group
    auto *sizeGroup = new QGroupBox(i18n("Page Size"), this);
    auto *sizeLayout = new QGridLayout(sizeGroup);

    sizeLayout->addWidget(new QLabel(i18n("Format:"), sizeGroup), 0, 0);
    d->presetCombo = new QComboBox(sizeGroup);
    if (d->hasReference) {
        d->presetCombo->addItem(i18n("Same as the current page"), QVariant::fromValue(referenceSize));
    }
    // presets are stored in portrait orientation, in points
    d->presetCombo->addItem(i18n("A4 (210 × 297 mm)"), QVariant::fromValue(QSizeF(595.0, 842.0)));
    d->presetCombo->addItem(i18n("A3 (297 × 420 mm)"), QVariant::fromValue(QSizeF(842.0, 1191.0)));
    d->presetCombo->addItem(i18n("A5 (148 × 210 mm)"), QVariant::fromValue(QSizeF(420.0, 595.0)));
    d->presetCombo->addItem(i18n("Letter (8.5 × 11 in)"), QVariant::fromValue(QSizeF(612.0, 792.0)));
    d->presetCombo->addItem(i18n("Legal (8.5 × 14 in)"), QVariant::fromValue(QSizeF(612.0, 1008.0)));
    d->presetCombo->addItem(i18n("Custom"), QVariant::fromValue(QSizeF()));
    sizeLayout->addWidget(d->presetCombo, 0, 1);

    sizeLayout->addWidget(new QLabel(i18n("Width (pt):"), sizeGroup), 1, 0);
    d->widthSpinBox = new QDoubleSpinBox(sizeGroup);
    d->widthSpinBox->setRange(10.0, 5000.0);
    d->widthSpinBox->setSingleStep(10.0);
    d->widthSpinBox->setDecimals(1);
    sizeLayout->addWidget(d->widthSpinBox, 1, 1);

    sizeLayout->addWidget(new QLabel(i18n("Height (pt):"), sizeGroup), 2, 0);
    d->heightSpinBox = new QDoubleSpinBox(sizeGroup);
    d->heightSpinBox->setRange(10.0, 5000.0);
    d->heightSpinBox->setSingleStep(10.0);
    d->heightSpinBox->setDecimals(1);
    sizeLayout->addWidget(d->heightSpinBox, 2, 1);

    sizeLayout->addWidget(new QLabel(i18n("Orientation:"), sizeGroup), 3, 0);
    auto *orientationLayout = new QHBoxLayout();
    d->portraitRadio = new QRadioButton(i18n("Portrait"), sizeGroup);
    d->landscapeRadio = new QRadioButton(i18n("Landscape"), sizeGroup);
    orientationLayout->addWidget(d->portraitRadio);
    orientationLayout->addWidget(d->landscapeRadio);
    orientationLayout->addStretch();
    sizeLayout->addLayout(orientationLayout, 3, 1);

    mainLayout->addWidget(sizeGroup);

    // Appearance group
    auto *appearanceGroup = new QGroupBox(i18n("Appearance"), this);
    auto *colorLayout = new QHBoxLayout(appearanceGroup);
    colorLayout->addWidget(new QLabel(i18n("Background color:"), appearanceGroup));
    d->colorButton = new QPushButton(appearanceGroup);
    d->colorButton->setFixedSize(24, 24);
    updateColorButton();
    colorLayout->addWidget(d->colorButton);
    colorLayout->addStretch();
    mainLayout->addWidget(appearanceGroup);

    // Page count
    auto *countGroup = new QGroupBox(i18n("Page Count"), this);
    auto *countLayout = new QHBoxLayout(countGroup);
    countLayout->addWidget(new QLabel(i18n("Number of pages:"), countGroup));
    d->countSpinBox = new QSpinBox(countGroup);
    d->countSpinBox->setRange(1, 100);
    d->countSpinBox->setValue(1);
    countLayout->addWidget(d->countSpinBox);
    countLayout->addStretch();
    mainLayout->addWidget(countGroup);

    // Buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // Initial size: the reference page, or A4 portrait
    d->portraitRadio->setChecked(true);
    applyPreset(0); // index 0 is the reference page if there is one, A4 otherwise

    // Connections
    connect(d->presetCombo, &QComboBox::activated, this, [this](int index) { applyPreset(index); });
    connect(d->widthSpinBox, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!d->updating) {
            syncControlsFromSize();
        }
    });
    connect(d->heightSpinBox, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!d->updating) {
            syncControlsFromSize();
        }
    });
    connect(d->portraitRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && !d->updating) {
            applyOrientation(false);
        }
    });
    connect(d->landscapeRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && !d->updating) {
            applyOrientation(true);
        }
    });
    connect(d->colorButton, &QPushButton::clicked, this, [this]() {
        const QColor color = QColorDialog::getColor(d->backgroundColor, this);
        if (color.isValid()) {
            d->backgroundColor = color;
            updateColorButton();
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

NewPageDialog::~NewPageDialog()
{
    delete d;
}

void NewPageDialog::applyPreset(int index)
{
    const QSizeF size = d->presetCombo->itemData(index).toSizeF();
    if (!size.isValid()) {
        return; // "Custom": keep what the user typed
    }

    double width = size.width();
    double height = size.height();
    const bool isReference = d->hasReference && index == 0;
    if (!isReference) {
        // the standard formats follow the chosen orientation
        const bool wantLandscape = d->landscapeRadio->isChecked();
        if (wantLandscape != (width > height)) {
            std::swap(width, height);
        }
    }

    d->updating = true;
    d->presetCombo->setCurrentIndex(index);
    d->widthSpinBox->setValue(width);
    d->heightSpinBox->setValue(height);
    (width > height ? d->landscapeRadio : d->portraitRadio)->setChecked(true);
    d->updating = false;
}

void NewPageDialog::applyOrientation(bool landscape)
{
    double width = d->widthSpinBox->value();
    double height = d->heightSpinBox->value();
    if (landscape != (width > height)) {
        std::swap(width, height);
    }

    d->updating = true;
    d->widthSpinBox->setValue(width);
    d->heightSpinBox->setValue(height);
    d->updating = false;
    syncControlsFromSize();
}

// Called when the width or the height have been edited: updates the orientation and the format shown
void NewPageDialog::syncControlsFromSize()
{
    const double width = d->widthSpinBox->value();
    const double height = d->heightSpinBox->value();

    int match = d->presetCombo->count() - 1; // "Custom"
    for (int i = 0; i < d->presetCombo->count() - 1; ++i) {
        const QSizeF size = d->presetCombo->itemData(i).toSizeF();
        if ((nearlyEqual(size.width(), width) && nearlyEqual(size.height(), height)) || (!(d->hasReference && i == 0) && nearlyEqual(size.width(), height) && nearlyEqual(size.height(), width))) {
            match = i;
            break;
        }
    }

    d->updating = true;
    d->presetCombo->setCurrentIndex(match);
    (width > height ? d->landscapeRadio : d->portraitRadio)->setChecked(true);
    d->updating = false;
}

void NewPageDialog::updateColorButton()
{
    const QString css = QStringLiteral("background-color: %1; border: 1px solid black;").arg(d->backgroundColor.name());
    d->colorButton->setStyleSheet(css);
}

void NewPageDialog::accept()
{
    // take into account a value that has been typed but not committed yet
    d->widthSpinBox->interpretText();
    d->heightSpinBox->interpretText();
    d->countSpinBox->interpretText();

    KConfigGroup group = newPageConfigGroup();
    group.writeEntry("BackgroundColor", d->backgroundColor);
    group.sync();

    QDialog::accept();
}

NewPageDialog::NewPageData NewPageDialog::data() const
{
    NewPageData data;
    data.width = d->widthSpinBox->value();
    data.height = d->heightSpinBox->value();
    data.backgroundColor = d->backgroundColor;
    data.count = d->countSpinBox->value();
    data.insertBefore = d->positionCombo->currentData().toInt() == static_cast<int>(InsertBefore);
    return data;
}

} // namespace Okular
