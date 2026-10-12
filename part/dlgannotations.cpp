/*
    SPDX-FileCopyrightText: 2006 Pino Toscano <toscano.pino@tiscali.it>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dlgannotations.h"

#include "widgetannottools.h"
#include "settings.h"
#include "widthpresetutils.h"

#include <KEditListWidget>
#include <KLocalizedString>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
class WidthPresetDialog final : public QDialog
{
public:
    explicit WidthPresetDialog(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle(i18nc("@title:window", "Annotation Width Presets"));
        auto *layout = new QVBoxLayout(this);
        auto *description = new QLabel(i18nc("@info", "Width presets are shared by all drawing tools, including the Eraser. Values must be unique and between %1 and %2 px.",
                                              AnnotationWidthPresets::minimumWidth,
                                              AnnotationWidthPresets::maximumWidth),
                                       this);
        description->setWordWrap(true);
        layout->addWidget(description);

        m_menuPresets = createPresetList(i18nc("@title:group", "Line-width menu presets (shared by all tools)"),
                                         Okular::Settings::annotationWidthMenuPresets(),
                                         AnnotationWidthPresets::defaultMenuPresetStrings());
        m_toolPresets = createPresetList(i18nc("@title:group", "Tool-settings quick buttons (shared by all tools)"),
                                         Okular::Settings::toolSettingsWidthPresets(),
                                         AnnotationWidthPresets::defaultToolPresetStrings());
        layout->addWidget(m_menuPresets->parentWidget());
        layout->addWidget(m_toolPresets->parentWidget());

        m_errorLabel = new QLabel(this);
        m_errorLabel->setWordWrap(true);
        m_errorLabel->setStyleSheet(QStringLiteral("color: palette(highlight);"));
        layout->addWidget(m_errorLabel);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QPushButton *restoreDefaults = buttons->addButton(i18nc("@action:button", "Restore Defaults"), QDialogButtonBox::ResetRole);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] { savePresets(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(restoreDefaults, &QPushButton::clicked, this, [this] {
            m_menuPresets->setItems(AnnotationWidthPresets::defaultMenuPresetStrings());
            m_toolPresets->setItems(AnnotationWidthPresets::defaultToolPresetStrings());
            m_errorLabel->clear();
        });
        resize(sizeHint());
    }

private:
    KEditListWidget *createPresetList(const QString &title, const QStringList &configured, const QStringList &defaults)
    {
        auto *group = new QGroupBox(title, this);
        auto *layout = new QVBoxLayout(group);
        auto *list = new KEditListWidget(group);
        list->setCheckAtEntering(true);
        auto *validator = new QDoubleValidator(AnnotationWidthPresets::minimumWidth, AnnotationWidthPresets::maximumWidth, 3, list->lineEdit());
        validator->setLocale(QLocale::c());
        validator->setNotation(QDoubleValidator::StandardNotation);
        list->lineEdit()->setValidator(validator);
        list->lineEdit()->setPlaceholderText(i18nc("@info:placeholder", "Width in px"));
        list->setItems(AnnotationWidthPresets::normalizedPresetStrings(configured, defaults));
        layout->addWidget(list);
        group->setLayout(layout);
        return list;
    }

    void savePresets()
    {
        QList<double> menuValues;
        QList<double> toolValues;
        if (!AnnotationWidthPresets::parsePresetStrings(m_menuPresets->items(), &menuValues)
            || !AnnotationWidthPresets::parsePresetStrings(m_toolPresets->items(), &toolValues)) {
            m_errorLabel->setText(i18nc("@info", "Enter one or more unique numeric widths from %1 to %2 px in each list.",
                                        AnnotationWidthPresets::minimumWidth,
                                        AnnotationWidthPresets::maximumWidth));
            return;
        }

        Okular::Settings::setAnnotationWidthMenuPresets(AnnotationWidthPresets::normalizedPresetStrings(m_menuPresets->items(), AnnotationWidthPresets::defaultMenuPresetStrings()));
        Okular::Settings::setToolSettingsWidthPresets(AnnotationWidthPresets::normalizedPresetStrings(m_toolPresets->items(), AnnotationWidthPresets::defaultToolPresetStrings()));
        Okular::Settings::self()->save();
        accept();
    }

    KEditListWidget *m_menuPresets = nullptr;
    KEditListWidget *m_toolPresets = nullptr;
    QLabel *m_errorLabel = nullptr;
};
}

DlgAnnotations::DlgAnnotations(QWidget *parent)
    : QWidget(parent)
{
    QFormLayout *layout = new QFormLayout(this);

    // BEGIN Annotation toolbar: Combo box to set the annotation toolbar associated to annotation action in tool menu
    QComboBox *primaryAnnotationToolBar = new QComboBox(this);
    primaryAnnotationToolBar->addItem(i18nc("item:inlistbox Config dialog, general page", "Full Annotation Toolbar"));
    primaryAnnotationToolBar->addItem(i18nc("item:inlistbox Config dialog, general page", "Quick Annotation Toolbar"));
    primaryAnnotationToolBar->setObjectName(QStringLiteral("kcfg_PrimaryAnnotationToolBar"));
    layout->addRow(i18nc("label:listbox Config dialog, general page", "Annotation toolbar:"), primaryAnnotationToolBar);
    // END Annotation toolbar

    // BEGIN Author row: Line edit to set the annotation’s default author value.
    QLineEdit *authorLineEdit = new QLineEdit(this);
    authorLineEdit->setObjectName(QStringLiteral("kcfg_IdentityAuthor"));
    layout->addRow(i18nc("@label:textbox Config dialog, annotations page", "Author:"), authorLineEdit);

    QLabel *authorInfoLabel = new QLabel(this);
    authorInfoLabel->setText(
        i18nc("@info Config dialog, annotations page", "<b>Note:</b> the information here is used only for annotations. The information is saved in annotated documents, and so will be transmitted together with the document."));
    authorInfoLabel->setWordWrap(true);
    layout->addRow(authorInfoLabel);
    // END Author row

    // Silly 1Em spacer:
    layout->addRow(new QLabel(this));

    // BEGIN Quick annotation tools section: WidgetAnnotTools manages tools.
    QLabel *toolsLabel = new QLabel(this);
    toolsLabel->setText(i18nc("@label Config dialog, annotations page, heading line for Quick Annotations tool manager", "<h3>Quick Annotation Tools</h3>"));
    layout->addRow(toolsLabel);

    WidgetAnnotTools *kcfg_QuickAnnotationTools = new WidgetAnnotTools(this);
    kcfg_QuickAnnotationTools->setObjectName(QStringLiteral("kcfg_QuickAnnotationTools"));
    layout->addRow(kcfg_QuickAnnotationTools);
    // END Quick annotation tools section

    auto *configureWidths = new QPushButton(i18nc("@action:button", "Configure Width Presets…"), this);
    configureWidths->setToolTip(i18nc("@info:tooltip", "Set the shared width presets used by drawing tools"));
    layout->addRow(i18nc("@label Config dialog, annotations page", "Drawing tool widths:"), configureWidths);
    connect(configureWidths, &QPushButton::clicked, this, [this] {
        WidthPresetDialog dialog(this);
        dialog.exec();
    });
}

#include "moc_dlgannotations.cpp"
