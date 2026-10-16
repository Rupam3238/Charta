/*
    SPDX-FileCopyrightText: 2024 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "newpagedialog.h"

// qt/kde includes
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <KLocalizedString>

// local includes
#include "settings.h"

namespace Okular
{

class NewPageDialog::Private
{
public:
    Private()
        : widthSpinBox(nullptr)
        , heightSpinBox(nullptr)
        , colorButton(nullptr)
        , orientationButton(nullptr)
        , countSpinBox(nullptr)
    {
        width = 595.0;  // A4 width in points
        height = 842.0; // A4 height in points
        backgroundColor = Qt::white;
        count = 1;
    }

    QDoubleSpinBox *widthSpinBox;
    QDoubleSpinBox *heightSpinBox;
    QPushButton *colorButton;
    QPushButton *orientationButton;
    QSpinBox *countSpinBox;

    double width;
    double height;
    QColor backgroundColor;
    int count;
};

NewPageDialog::NewPageDialog(QWidget *parent)
    : QDialog(parent)
    , d(new Private)
{
    setWindowTitle(i18n("Add New Page"));
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);

    // Page size group
    auto *sizeGroup = new QGroupBox(i18n("Page Size"), this);
    auto *sizeLayout = new QGridLayout(sizeGroup);

    sizeLayout->addWidget(new QLabel(i18n("Width (pt):"), this), 0, 0);
    d->widthSpinBox = new QDoubleSpinBox(this);
    d->widthSpinBox->setRange(10.0, 2000.0);
    d->widthSpinBox->setSingleStep(10.0);
    d->widthSpinBox->setValue(595.0); // A4 width
    sizeLayout->addWidget(d->widthSpinBox, 0, 1);

    sizeLayout->addWidget(new QLabel(i18n("Height (pt):"), this), 1, 0);
    d->heightSpinBox = new QDoubleSpinBox(this);
    d->heightSpinBox->setRange(10.0, 2000.0);
    d->heightSpinBox->setSingleStep(10.0);
    d->heightSpinBox->setValue(842.0); // A4 height
    sizeLayout->addWidget(d->heightSpinBox, 1, 1);

    mainLayout->addWidget(sizeGroup);

    // Appearance group
    auto *appearanceGroup = new QGroupBox(i18n("Appearance"), this);
    auto *appearanceLayout = new QVBoxLayout(appearanceGroup);

    // Background color
    auto *colorLayout = new QHBoxLayout();
    colorLayout->addWidget(new QLabel(i18n("Background Color:"), this));
    d->colorButton = new QPushButton(this);
    d->colorButton->setFixedSize(24, 24);
    updateColorButton();
    colorLayout->addWidget(d->colorButton);
    colorLayout->addStretch();
    appearanceLayout->addLayout(colorLayout);

    // Orientation toggle
    auto *orientationLayout = new QHBoxLayout();
    orientationLayout->addWidget(new QLabel(i18n("Orientation:"), this));
    d->orientationButton = new QPushButton(this);
    d->orientationButton->setText(i18n("Portrait"));
    orientationLayout->addWidget(d->orientationButton);
    orientationLayout->addStretch();
    appearanceLayout->addLayout(orientationLayout);

    mainLayout->addWidget(appearanceGroup);

    // Page count
    auto *countGroup = new QGroupBox(i18n("Page Count"), this);
    auto *countLayout = new QHBoxLayout(countGroup);
    countLayout->addWidget(new QLabel(i18n("Number of pages:"), this));
    d->countSpinBox = new QSpinBox(this);
    d->countSpinBox->setRange(1, 100);
    d->countSpinBox->setValue(1);
    countLayout->addWidget(d->countSpinBox);
    countLayout->addStretch();
    mainLayout->addWidget(countGroup);

    // Buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // Connections
    connect(d->widthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) { d->width = value; });
    connect(d->heightSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) { d->height = value; });
    connect(d->colorButton, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(d->backgroundColor, this);
        if (color.isValid()) {
            d->backgroundColor = color;
            updateColorButton();
        }
    });
    connect(d->orientationButton, &QPushButton::clicked, this, [this]() {
        if (d->orientationButton->text() == i18n("Portrait")) {
            d->orientationButton->setText(i18n("Landscape"));
            std::swap(d->width, d->height);
            d->widthSpinBox->setValue(d->width);
            d->heightSpinBox->setValue(d->height);
        } else {
            d->orientationButton->setText(i18n("Portrait"));
            std::swap(d->width, d->height);
            d->widthSpinBox->setValue(d->width);
            d->heightSpinBox->setValue(d->height);
        }
    });
    connect(d->countSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) { d->count = value; });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

NewPageDialog::~NewPageDialog()
{
    delete d;
}

void NewPageDialog::updateColorButton()
{
    QString css = QStringLiteral("background-color: %1; border: 1px solid black;")
                      .arg(d->backgroundColor.name());
    d->colorButton->setStyleSheet(css);
}

NewPageDialog::NewPageData NewPageDialog::data() const
{
    NewPageData data;
    data.width = d->width;
    data.height = d->height;
    data.backgroundColor = d->backgroundColor;
    data.count = d->count;
    return data;
}

} // namespace Okular