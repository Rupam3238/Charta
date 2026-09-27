/*
    SPDX-FileCopyrightText: 2019 Simone Gaiarin <simgunz@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationactionhandler.h"

// qt includes
#include <QActionGroup>
#include <QApplication>
#include <QBitmap>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileInfo>
#include <QFontDialog>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRadioButton>
#include <QSlider>
#include <QSignalBlocker>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidgetAction>

// kde includes
#include <KActionCollection>
#include <KActionCategory>
#include <KLocalizedString>
#include <KMessageBox>
#include <KParts/MainWindow>
#include <KSelectAction>
#include <KToolBar>

// local includes
#include "actionbar.h"
#include "annotationwidgets.h"
#include "core/annotations.h"
#include "gui/guiutils.h"
#include "pageview.h"
#include "pageviewannotator.h"
#include "settings.h"
#include "toggleactionmenu.h"

class AnnotationActionHandlerPrivate
{
public:
    enum class AnnotationColor { Color, InnerColor };
    enum class DrawingToolKind { Pen, Eraser, Line, Arrow, Rectangle, Ellipse, Polygon, Text };
    struct DrawingToolSettings {
        double width = 3.0;
        QColor color = Qt::black;
        QColor fillColor = Qt::transparent;
        int fillOpacity = 0;
        bool partialEraser = false;
        bool initialized = false;
    };
    static const QList<QPair<KLocalizedString, QColor>> defaultColors;
    static const QList<double> widthStandardValues;
    static const QList<double> opacityStandardValues;

    explicit AnnotationActionHandlerPrivate(AnnotationActionHandler *qq)
        : q(qq)
        , annotator(nullptr)
        , agTools(nullptr)
        , agLastAction(nullptr)
        , aQuickTools(nullptr)
        , aQuickToolsBar(nullptr)
        , aGeomShapes(nullptr)
        , aStamp(nullptr)
        , aAddToQuickTools(nullptr)
        , aContinuousMode(nullptr)
        , aConstrainRatioAndAngle(nullptr)
        , aWidth(nullptr)
        , aColor(nullptr)
        , aInnerColor(nullptr)
        , aOpacity(nullptr)
        , aFont(nullptr)
        , aAdvancedSettings(nullptr)
        , aHideToolBar(nullptr)
        , aShowToolBar(nullptr)
        , aToolBarVisibility(nullptr)
        , aSelectTool(nullptr)
        , aEraserTool(nullptr)
        , aDrawingBrowse(nullptr)
        , aBrowseTool(nullptr)
        , aCustomStamp(nullptr)
        , aCustomWidth(nullptr)
        , aCustomOpacity(nullptr)
        , currentColor(QColor())
        , currentInnerColor(QColor())
        , currentFont(QFont())
        , currentWidth(-1)
        , selectedBuiltinTool(-1)
        , textToolsEnabled(false)
    {
    }

    QAction *selectActionItem(KSelectAction *aList, QAction *aCustomCurrent, double value, const QList<double> &defaultValues, const QIcon &icon, const QString &label);

    /**
     * @short Adds a custom stamp annotation action to the stamp list when the stamp is not a default stamp
     *
     * When @p stampIconName cannot be found among the default stamps, this method creates a new action
     * for the custom stamp annotation and adds it to the stamp action combo box.
     * If a custom action is already present in the list, it is removed before adding the new custom action.
     * If @p stampIconName matches a default stamp, any existing custom stamp annotation action is removed.
     */
    void maybeUpdateCustomStampAction(const QString &stampIconName);
    void parseTool(int toolId);

    void updateConfigActions(const QString &annotType = QLatin1String(""));
    void populateQuickAnnotations();
    KSelectAction *colorPickerAction(AnnotationColor colorType);

    const QIcon widthIcon(double width);
    const QIcon stampIcon(const QString &stampIconName);

    void selectTool(int toolId);
    void slotStampToolSelected(const QString &stamp);
    void slotQuickToolSelected(int favToolId);
    void slotSetColor(AnnotationColor colorType, const QColor &color = QColor());
    void slotSelectAnnotationFont();
    bool showToolSettings(QAction *action);
    bool isQuickToolAction(QAction *aTool);
    bool isQuickToolStamp(int toolId);
    void assertToolBarExists(KParts::MainWindow *mw, const QString &toolBarName);

    AnnotationActionHandler *q;

    PageViewAnnotator *annotator;

    QList<QAction *> quickTools;
    QList<QAction *> textTools;
    QList<QAction *> textQuickTools;
    QList<QAction *> drawingTools;
    QHash<QAction *, DrawingToolKind> drawingToolKinds;
    QHash<QAction *, int> drawingToolIds;
    QHash<QString, DrawingToolSettings> drawingToolSettings;
    QHash<QObject *, QAction *> settingsButtonActions;
    QAction *pendingMousePopupAction = nullptr;
    bool deselectingTools = false;
    QActionGroup *agTools;
    QAction *agLastAction;

    ToggleActionMenu *aQuickTools;
    ActionBar *aQuickToolsBar;
    ToggleActionMenu *aGeomShapes;
    ToggleActionMenu *aStamp;
    QAction *aAddToQuickTools;
    KToggleAction *aContinuousMode;
    KToggleAction *aConstrainRatioAndAngle;
    KSelectAction *aWidth;
    KSelectAction *aColor;
    KSelectAction *aInnerColor;
    KSelectAction *aOpacity;
    QAction *aFont;
    QAction *aAdvancedSettings;
    QAction *aHideToolBar;
    QAction *aShowToolBar;
    KToggleAction *aToolBarVisibility;
    KToggleAction *aSelectTool;
    KToggleAction *aEraserTool;
    KToggleAction *aDrawingBrowse;
    QAction *aBrowseTool;
    KToolBar *drawingToolBar = nullptr;
    bool selectModeSwitchingToBrowse = false;

    QAction *aCustomStamp;
    QAction *aCustomWidth;
    QAction *aCustomOpacity;

    QColor currentColor;
    QColor currentInnerColor;
    QFont currentFont;
    int currentWidth;

    int selectedBuiltinTool;
    bool textToolsEnabled;
};

const QList<QPair<KLocalizedString, QColor>> AnnotationActionHandlerPrivate::defaultColors = {{ki18nc("@item:inlistbox Color name", "Red"), Qt::red},
                                                                                              {ki18nc("@item:inlistbox Color name", "Orange"), QColor(255, 85, 0)},
                                                                                              {ki18nc("@item:inlistbox Color name", "Yellow"), Qt::yellow},
                                                                                              {ki18nc("@item:inlistbox Color name", "Green"), Qt::green},
                                                                                              {ki18nc("@item:inlistbox Color name", "Cyan"), Qt::cyan},
                                                                                              {ki18nc("@item:inlistbox Color name", "Blue"), Qt::blue},
                                                                                              {ki18nc("@item:inlistbox Color name", "Magenta"), Qt::magenta},
                                                                                              {ki18nc("@item:inlistbox Color name", "White"), Qt::white},
                                                                                              {ki18nc("@item:inlistbox Color name", "Gray"), Qt::gray},
                                                                                              {ki18nc("@item:inlistbox Color name", "Black"), Qt::black}

};

const QList<double> AnnotationActionHandlerPrivate::widthStandardValues = {1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5};

const QList<double> AnnotationActionHandlerPrivate::opacityStandardValues = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};

QAction *AnnotationActionHandlerPrivate::selectActionItem(KSelectAction *aList, QAction *aCustomCurrent, double value, const QList<double> &defaultValues, const QIcon &icon, const QString &label)
{
    if (aCustomCurrent) {
        aList->removeAction(aCustomCurrent);
        delete aCustomCurrent;
    }
    QAction *aCustom = nullptr;
    const int defaultValueIdx = defaultValues.indexOf(value);
    if (defaultValueIdx >= 0) {
        aList->setCurrentItem(defaultValueIdx);
    } else {
        aCustom = new KToggleAction(icon, label, q);
        const int aBeforeIdx = std::lower_bound(defaultValues.begin(), defaultValues.end(), value) - defaultValues.begin();
        QAction *aBefore = aBeforeIdx < defaultValues.size() ? aList->actions().at(aBeforeIdx) : nullptr;
        aList->insertAction(aBefore, aCustom);
        aList->setCurrentAction(aCustom);
    }
    return aCustom;
}

bool AnnotationActionHandlerPrivate::showToolSettings(QAction *action)
{
    const auto kindIt = drawingToolKinds.constFind(action);
    if (kindIt == drawingToolKinds.cend()) {
        return false;
    }

    const QString key = action->objectName().isEmpty() ? action->text() : action->objectName();
    DrawingToolSettings &initialSettings = drawingToolSettings[key];
    if (!initialSettings.initialized) {
        if (*kindIt == DrawingToolKind::Eraser) {
            initialSettings.width = annotator->eraserWidth();
        } else if (const auto toolId = drawingToolIds.constFind(action); toolId != drawingToolIds.cend()) {
            const QDomElement annotation = annotator->builtinTool(*toolId).firstChildElement(QStringLiteral("engine")).firstChildElement(QStringLiteral("annotation"));
            initialSettings.width = annotation.hasAttribute(QStringLiteral("width")) ? annotation.attribute(QStringLiteral("width")).toDouble() / 0.75 : 3.0;
            const QString colorAttribute = annotation.attribute(QStringLiteral("textColor"), annotation.attribute(QStringLiteral("color")));
            if (QColor color(colorAttribute); color.isValid()) {
                initialSettings.color = color;
            }
            const QColor fill(annotation.attribute(QStringLiteral("innerColor")));
            if (fill.isValid()) {
                initialSettings.fillOpacity = qRound(fill.alphaF() * 100.0);
                initialSettings.fillColor = QColor(fill.red(), fill.green(), fill.blue());
            }
        }
        initialSettings.initialized = true;
    }

    QMenu popup;
    popup.setStyleSheet(QStringLiteral(
        "QMenu { background: #202630; color: #edf1f5; border: 1px solid #3b4653; padding: 4px; }"
        "QWidget { background: #202630; color: #edf1f5; }"
        "QLabel { color: #c8d1db; background: transparent; }"
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #3a4552; }"
        "QSlider::sub-page:horizontal { border-radius: 2px; background: #27bfd6; }"
        "QSlider::handle:horizontal { width: 12px; margin: -5px 0; border-radius: 6px; background: #27bfd6; border: 1px solid #d6edf1; }"
        "QDoubleSpinBox, QComboBox, QToolButton { color: #edf1f5; background: #29323d; border: 1px solid #465361; border-radius: 4px; padding: 4px; }"
        "QDoubleSpinBox:focus, QComboBox:focus, QToolButton:focus { border: 1px solid #27bfd6; }"
        "QToolButton:hover { background: #35414e; }"));
    auto *contents = new QWidget(&popup);
    auto *layout = new QVBoxLayout(contents);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto addWidthControl = [&](double minimum, double maximum, int decimals) {
        auto *row = new QHBoxLayout;
        auto *title = new QLabel(i18n("Width"), contents);
        auto *currentValue = new QLabel(contents);
        row->addWidget(title);
        row->addStretch(1);
        row->addWidget(currentValue);
        layout->addLayout(row);

        auto *slider = new QSlider(Qt::Horizontal, contents);
        const int scale = decimals == 0 ? 1 : 2;
        slider->setRange(qRound(minimum * scale), qRound(maximum * scale));
        slider->setSingleStep(1);

        auto *spinBox = new QDoubleSpinBox(contents);
        spinBox->setRange(minimum, maximum);
        spinBox->setDecimals(decimals);
        spinBox->setSingleStep(decimals == 0 ? 1.0 : 0.5);
        spinBox->setSuffix(i18n(" px"));
        spinBox->setFixedWidth(110);
        auto *controlRow = new QHBoxLayout;
        controlRow->setSpacing(8);
        controlRow->addWidget(slider, 1);
        controlRow->addWidget(spinBox);
        layout->addLayout(controlRow);

        const double initialWidth = drawingToolSettings.value(key).width;
        spinBox->setValue(initialWidth);
        slider->setValue(qRound(initialWidth * scale));
        currentValue->setText(i18n("%1 px", spinBox->value()));

        QObject::connect(slider, &QSlider::valueChanged, spinBox, [spinBox, scale](int value) { spinBox->setValue(static_cast<double>(value) / scale); });
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, slider, [slider, scale](double value) { slider->setValue(qRound(value * scale)); });
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, currentValue, [currentValue](double value) { currentValue->setText(i18n("%1 px", value)); });
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, q, [this, key, kind = *kindIt](double value) {
            drawingToolSettings[key].width = value;
            if (kind == DrawingToolKind::Eraser) {
                annotator->setEraserWidth(value);
            } else if (kind != DrawingToolKind::Line && kind != DrawingToolKind::Arrow) {
                annotator->setDrawingToolWidthPixels(value);
            }
        });
    };

    auto addColorControl = [&](const QString &label, bool fillColor) {
        auto *row = new QHBoxLayout;
        auto *title = new QLabel(label, contents);
        auto *swatch = new QToolButton(contents);
        swatch->setMinimumWidth(92);
        swatch->setMinimumHeight(28);
        swatch->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        swatch->setText(i18n("Choose…"));
        swatch->setToolTip(label);
        const QColor color = fillColor ? drawingToolSettings.value(key).fillColor : drawingToolSettings.value(key).color;
        auto setSwatch = [swatch](const QColor &swatchColor) {
            QPixmap pixmap(18, 18);
            pixmap.fill(swatchColor);
            QPainter painter(&pixmap);
            painter.setPen(QApplication::palette().color(QPalette::Mid));
            painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
            swatch->setIcon(QIcon(pixmap));
        };
        setSwatch(color);
        row->addWidget(title);
        row->addStretch(1);
        row->addWidget(swatch);
        layout->addLayout(row);
        QObject::connect(swatch, &QToolButton::clicked, q, [this, swatch, key, label, fillColor] {
            const QColor initial = fillColor ? drawingToolSettings.value(key).fillColor : drawingToolSettings.value(key).color;
            QColorDialog colorDialog(initial, drawingToolBar ? drawingToolBar->window() : QApplication::activeWindow());
            colorDialog.setWindowTitle(label);
            colorDialog.setOption(QColorDialog::ShowAlphaChannel, fillColor);
            if (colorDialog.exec() != QDialog::Accepted) {
                return;
            }
            const QColor selected = colorDialog.selectedColor();
            if (!selected.isValid()) {
                return;
            }
            if (fillColor) {
                drawingToolSettings[key].fillColor = selected;
                QColor effectiveFillColor = selected;
                effectiveFillColor.setAlphaF(drawingToolSettings[key].fillOpacity / 100.0);
                currentInnerColor = effectiveFillColor;
                annotator->setAnnotationInnerColor(effectiveFillColor);
            } else {
                drawingToolSettings[key].color = selected;
                currentColor = selected;
                annotator->setAnnotationColor(selected);
            }
            QPixmap pixmap(18, 18);
            pixmap.fill(fillColor ? drawingToolSettings.value(key).fillColor : selected);
            QPainter painter(&pixmap);
            painter.setPen(QApplication::palette().color(QPalette::Mid));
            painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
            swatch->setIcon(QIcon(pixmap));
        });
    };

    addWidthControl(*kindIt == DrawingToolKind::Eraser ? 1.0 : 0.5, *kindIt == DrawingToolKind::Eraser ? 64.0 : 20.0, *kindIt == DrawingToolKind::Eraser ? 0 : 1);

    if (*kindIt != DrawingToolKind::Eraser) {
        addColorControl(i18n("Color"), false);
    }
    if (*kindIt == DrawingToolKind::Rectangle || *kindIt == DrawingToolKind::Ellipse) {
        addColorControl(i18n("Fill color"), true);
        auto *opacityTitle = new QLabel(i18n("Fill opacity"), contents);
        auto *opacityValue = new QLabel(contents);
        auto *opacityRow = new QHBoxLayout;
        opacityRow->addWidget(opacityTitle);
        opacityRow->addStretch(1);
        opacityRow->addWidget(opacityValue);
        layout->addLayout(opacityRow);
        auto *opacitySlider = new QSlider(Qt::Horizontal, contents);
        opacitySlider->setRange(0, 100);
        opacitySlider->setValue(drawingToolSettings.value(key).fillOpacity);
        layout->addWidget(opacitySlider);
        opacityValue->setText(i18n("%1%", opacitySlider->value()));
        QObject::connect(opacitySlider, &QSlider::valueChanged, opacityValue, [opacityValue](int value) { opacityValue->setText(i18n("%1%", value)); });
        QObject::connect(opacitySlider, &QSlider::valueChanged, q, [this, key](int value) {
            drawingToolSettings[key].fillOpacity = value;
            QColor fill = drawingToolSettings[key].fillColor;
            fill.setAlphaF(value / 100.0);
            currentInnerColor = fill;
            annotator->setAnnotationInnerColor(fill);
        });
    }
    if (*kindIt == DrawingToolKind::Eraser) {
        auto *modeLabel = new QLabel(i18n("Mode"), contents);
        layout->addWidget(modeLabel);
        auto *mode = new QComboBox(contents);
        mode->addItems({i18n("Stroke eraser"), i18n("Partial eraser")});
        mode->setCurrentIndex(drawingToolSettings.value(key).partialEraser ? 1 : 0);
        layout->addWidget(mode);
        QObject::connect(mode, &QComboBox::currentIndexChanged, q, [this, key](int index) { drawingToolSettings[key].partialEraser = index == 1; });
    }

    auto *widgetAction = new QWidgetAction(&popup);
    widgetAction->setDefaultWidget(contents);
    popup.addAction(widgetAction);
    popup.setWindowTitle(i18n("%1 Settings", action->text()));
    popup.setMinimumWidth(250);
    popup.ensurePolished();
    popup.adjustSize();
    QPoint popupPosition = QCursor::pos();
    if (drawingToolBar) {
        if (QWidget *toolButton = drawingToolBar->widgetForAction(action)) {
            popupPosition = toolButton->mapToGlobal(QPoint(toolButton->width() + 6, 0));
        }
    }
    if (QScreen *screen = QGuiApplication::screenAt(popupPosition)) {
        const QRect available = screen->availableGeometry();
        if (popupPosition.x() + popup.width() > available.right()) {
            popupPosition.setX(qMax(available.left(), popupPosition.x() - popup.width() - (drawingToolBar ? drawingToolBar->width() + 12 : 0)));
        }
        popupPosition.setY(qBound(available.top(), popupPosition.y(), available.bottom() - popup.height() + 1));
    }
    popup.exec(popupPosition);
    return true;
}

void AnnotationActionHandlerPrivate::maybeUpdateCustomStampAction(const QString &stampIconName)
{
    const auto defaultStamps = StampAnnotationWidget::defaultStamps();
    auto it = std::find_if(defaultStamps.begin(), defaultStamps.end(), [&stampIconName](const QPair<QString, QString> &element) { return element.second == stampIconName; });
    bool defaultStamp = it != defaultStamps.end();

    if (aCustomStamp) {
        aStamp->removeAction(aCustomStamp);
        agTools->removeAction(aCustomStamp);
        delete aCustomStamp;
        aCustomStamp = nullptr;
    }
    if (!defaultStamp) {
        QFileInfo info(stampIconName);
        QString stampActionName = info.fileName();
        aCustomStamp = new KToggleAction(stampIcon(stampIconName), stampActionName, q);
        aStamp->addAction(aCustomStamp);
        aStamp->setDefaultAction(aCustomStamp);
        agTools->addAction(aCustomStamp);
        aCustomStamp->setChecked(true);
        QObject::connect(aCustomStamp, &QAction::triggered, q, [this, stampIconName]() { slotStampToolSelected(stampIconName); });
    }
}

void AnnotationActionHandlerPrivate::parseTool(int toolId)
{
    if (toolId == -1) {
        updateConfigActions();
        return;
    }

    QDomElement toolElement = annotator->builtinTool(toolId);
    const QString annotType = toolElement.attribute(QStringLiteral("type"));
    QDomElement engineElement = toolElement.firstChildElement(QStringLiteral("engine"));
    QDomElement annElement = engineElement.firstChildElement(QStringLiteral("annotation"));

    QColor color, innerColor, textColor;
    if (annElement.hasAttribute(QStringLiteral("color"))) {
        color = QColor(annElement.attribute(QStringLiteral("color")));
    }
    if (annElement.hasAttribute(QStringLiteral("innerColor"))) {
        innerColor = QColor(annElement.attribute(QStringLiteral("innerColor")));
    }
    if (annElement.hasAttribute(QStringLiteral("textColor"))) {
        textColor = QColor(annElement.attribute(QStringLiteral("textColor")));
    }
    if (textColor.isValid()) {
        currentColor = textColor;
        currentInnerColor = color;
    } else {
        currentColor = color;
        currentInnerColor = innerColor;
    }

    if (annElement.hasAttribute(QStringLiteral("font"))) {
        currentFont.fromString(annElement.attribute(QStringLiteral("font")));
    }

    // if the width value is not a default one, insert a new action in the width list
    if (annElement.hasAttribute(QStringLiteral("width"))) {
        double width = annElement.attribute(QStringLiteral("width")).toDouble();
        aCustomWidth = selectActionItem(aWidth, aCustomWidth, width, widthStandardValues, widthIcon(width), i18nc("@item:inlistbox", "Width %1", width));
    }

    // if the opacity value is not a default one, insert a new action in the opacity list
    if (annElement.hasAttribute(QStringLiteral("opacity"))) {
        double opacity = annElement.attribute(QStringLiteral("opacity")).toDouble();
        aCustomOpacity = selectActionItem(aOpacity, aCustomOpacity, opacity, opacityStandardValues, GuiUtils::createOpacityIcon(opacity), i18nc("@item:inlistbox", "%1%", opacity * 100));
    } else {
        aOpacity->setCurrentItem(opacityStandardValues.size() - 1); // 100 %
    }

    // if the tool is a custom stamp, insert a new action in the stamp list
    if (annotType == QStringLiteral("stamp")) {
        QString stampIconName = annElement.attribute(QStringLiteral("icon"));
        maybeUpdateCustomStampAction(stampIconName);
    }

    updateConfigActions(annotType);
}

void AnnotationActionHandlerPrivate::updateConfigActions(const QString &annotType)
{
    const bool isAnnotationSelected = !annotType.isEmpty();
    const bool isEraser = annotator->eraserMode();
    const bool isTypewriter = annotType == QStringLiteral("typewriter");
    const bool isInlineNote = annotType == QStringLiteral("note-inline");
    const bool isText = isInlineNote || isTypewriter;
    const bool isPolygon = annotType == QStringLiteral("polygon");
    const bool isShape = annotType == QStringLiteral("rectangle") || annotType == QStringLiteral("ellipse") || isPolygon;
    const bool isStraightLine = annotType == QStringLiteral("straight-line");
    const bool isLine = annotType == QStringLiteral("ink") || isStraightLine;
    const bool isStamp = annotType == QStringLiteral("stamp");

    if (isTypewriter) {
        aColor->setIcon(GuiUtils::createColorIcon({currentColor}, QIcon::fromTheme(QStringLiteral("format-text-color"))));
    } else {
        aColor->setIcon(GuiUtils::createColorIcon({currentColor}, QIcon::fromTheme(QStringLiteral("format-stroke-color"))));
    }
    aInnerColor->setIcon(GuiUtils::createColorIcon({currentInnerColor}, QIcon::fromTheme(QStringLiteral("format-fill-color"))));

    aAddToQuickTools->setEnabled(isAnnotationSelected);
    aWidth->setEnabled(isLine || isShape || isEraser);
    aColor->setEnabled(isAnnotationSelected && !isStamp);
    aInnerColor->setEnabled(isShape);
    aOpacity->setEnabled(isAnnotationSelected);
    aFont->setEnabled(isText);
    aConstrainRatioAndAngle->setEnabled(isStraightLine || isShape);
    aAdvancedSettings->setEnabled(isAnnotationSelected);

    if (isEraser) {
        const double width = annotator->eraserWidth();
        aCustomWidth = selectActionItem(aWidth, aCustomWidth, width, widthStandardValues, widthIcon(width), i18nc("@item:inlistbox", "Eraser diameter %1 px", width));
        aWidth->setToolTip(i18nc("@info:tooltip", "Eraser diameter"));
        return;
    }

    // set tooltips
    if (!isAnnotationSelected) {
        aWidth->setToolTip(i18nc("@info:tooltip", "Annotation line width (No annotation selected)"));
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation color (No annotation selected)"));
        aInnerColor->setToolTip(i18nc("@info:tooltip", "Annotation fill color (No annotation selected)"));
        aOpacity->setToolTip(i18nc("@info:tooltip", "Annotation opacity (No annotation selected)"));
        aFont->setToolTip(i18nc("@info:tooltip", "Annotation font (No annotation selected)"));
        aAddToQuickTools->setToolTip(i18nc("@info:tooltip", "Add the current annotation to the quick annotations menu (No annotation selected)"));
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain shape ratio to 1:1 or line angle to 15° steps (No annotation selected)"));
        aAdvancedSettings->setToolTip(i18nc("@info:tooltip", "Advanced settings for the current annotation tool (No annotation selected)"));
        return;
    }

    if (isLine || isShape) {
        aWidth->setToolTip(i18nc("@info:tooltip", "Annotation line width"));
    } else {
        aWidth->setToolTip(i18nc("@info:tooltip", "Annotation line width (Current annotation has no line width)"));
    }

    if (isTypewriter) {
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation text color"));
    } else if (isShape) {
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation border color"));
    } else {
        aColor->setToolTip(i18nc("@info:tooltip", "Annotation color"));
    }

    if (isShape) {
        aInnerColor->setToolTip(i18nc("@info:tooltip", "Annotation fill color"));
    } else {
        aInnerColor->setToolTip(i18nc("@info:tooltip", "Annotation fill color (Current annotation has no fill color)"));
    }

    if (isText) {
        aFont->setToolTip(i18nc("@info:tooltip", "Annotation font"));
    } else {
        aFont->setToolTip(i18nc("@info:tooltip", "Annotation font (Current annotation has no font)"));
    }

    if (isStraightLine || isPolygon) {
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain line angle to 15° steps"));
    } else if (isShape) {
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain shape ratio to 1:1"));
    } else {
        aConstrainRatioAndAngle->setToolTip(i18nc("@info:tooltip", "Constrain shape ratio to 1:1 or line angle to 15° steps (Not supported by current annotation)"));
    }

    aOpacity->setToolTip(i18nc("@info:tooltip", "Annotation opacity"));
    aAddToQuickTools->setToolTip(i18nc("@info:tooltip", "Add the current annotation to the quick annotations menu"));
    aAdvancedSettings->setToolTip(i18nc("@info:tooltip", "Advanced settings for the current annotation tool"));
}

void AnnotationActionHandlerPrivate::populateQuickAnnotations()
{
    if (!aQuickTools->isEnabled()) {
        return;
    }

    const QList<int> numberKeys = {Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5, Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9, Qt::Key_0};
    const bool isFirstTimePopulated = aQuickTools->menu()->actions().isEmpty();

    // to be safe and avoid undefined states of the currently selected quick annotation
    if (isQuickToolAction(agTools->checkedAction())) {
        q->deselectAllAnnotationActions();
    }

    for (QAction *action : std::as_const(quickTools)) {
        aQuickTools->removeAction(action);
        aQuickToolsBar->removeAction(action);
        delete action;
    }
    quickTools.clear();
    textQuickTools.clear();

    int favToolId = 1;
    QList<int>::const_iterator shortcutNumber = numberKeys.begin();
    QDomElement favToolElement = annotator->quickTool(favToolId);
    int actionBarInsertPosition = 0;
    QAction *aSeparator = aQuickTools->menu()->actions().constFirst();
    while (!favToolElement.isNull()) {
        QString itemText = favToolElement.attribute(QStringLiteral("name"));
        if (favToolElement.attribute(QStringLiteral("default"), QStringLiteral("false")) == QLatin1String("true")) {
            itemText = i18n(itemText.toLatin1().constData());
        }
        if (itemText.isEmpty()) {
            itemText = PageViewAnnotator::defaultToolName(favToolElement);
        }
        QIcon toolIcon = QIcon(PageViewAnnotator::makeToolPixmap(favToolElement));
        QAction *annFav = new KToggleAction(toolIcon, itemText, q);
        aQuickTools->insertAction(aSeparator, annFav);
        aQuickToolsBar->insertAction(actionBarInsertPosition++, annFav);
        agTools->addAction(annFav);
        quickTools.append(annFav);
        if (shortcutNumber != numberKeys.end()) {
            const int shortcutKey = *(shortcutNumber++);
            if (shortcutKey != Qt::Key_1) {
                annFav->setShortcut(QKeySequence(shortcutKey));
                annFav->setShortcutContext(Qt::WidgetWithChildrenShortcut);
            }
        }
        QObject::connect(annFav, &KToggleAction::toggled, q, [this, favToolId](bool checked) {
            if (checked) {
                slotQuickToolSelected(favToolId);
            }
        });
        QDomElement engineElement = favToolElement.firstChildElement(QStringLiteral("engine"));
        if (engineElement.attribute(QStringLiteral("type")) == QStringLiteral("TextSelector")) {
            textQuickTools.append(annFav);
            annFav->setEnabled(textToolsEnabled);
        }
        favToolElement = annotator->quickTool(++favToolId);
    }
    aQuickToolsBar->recreateWidgets();

    // set the default action
    if (quickTools.isEmpty()) {
        aShowToolBar->setVisible(false);
        aQuickTools->addAction(aToolBarVisibility);
        aQuickTools->setDefaultAction(aToolBarVisibility);
        Okular::Settings::setQuickAnnotationDefaultAction(0);
        Okular::Settings::self()->save();
    } else {
        aShowToolBar->setVisible(true);
        aQuickTools->removeAction(aToolBarVisibility);
        aQuickTools->setDefaultAction(aQuickTools);
        int defaultAction = Okular::Settings::quickAnnotationDefaultAction();
        if (isFirstTimePopulated && defaultAction < quickTools.count()) {
            // we can reach here also if no quick tools were defined before, in that case defaultAction is correctly equal to zero
            aQuickTools->setDefaultAction(quickTools.at(defaultAction));
        } else {
            // if the quick tools have been modified we cannot restore the previous default action
            aQuickTools->setDefaultAction(quickTools.at(0));
            Okular::Settings::setQuickAnnotationDefaultAction(0);
            Okular::Settings::self()->save();
        }
    }
}

KSelectAction *AnnotationActionHandlerPrivate::colorPickerAction(AnnotationColor colorType)
{
    auto colorList = defaultColors;
    QString aText(i18nc("@action:intoolbar Current annotation config option", "Color"));
    if (colorType == AnnotationColor::InnerColor) {
        aText = i18nc("@action:intoolbar Current annotation config option", "Fill Color");
        colorList.append(QPair<KLocalizedString, Qt::GlobalColor>(ki18nc("@item:inlistbox Color name", "Transparent"), Qt::transparent));
    }
    KSelectAction *aColorPicker = new KSelectAction(QIcon(), aText, q);
    aColorPicker->setToolBarMode(KSelectAction::MenuMode);
    for (const auto &colorNameValue : colorList) {
        QColor color(colorNameValue.second);
        QAction *colorAction = new QAction(GuiUtils::createColorIcon({color}, QIcon(), GuiUtils::VisualizeTransparent), colorNameValue.first.toString(), q);
        aColorPicker->addAction(colorAction);
        QObject::connect(colorAction, &QAction::triggered, q, [this, colorType, color]() { slotSetColor(colorType, color); });
    }
    QAction *aCustomColor = new QAction(QIcon::fromTheme(QStringLiteral("color-picker")), i18nc("@item:inlistbox", "Custom Color…"), q);
    aColorPicker->addAction(aCustomColor);
    QObject::connect(aCustomColor, &QAction::triggered, q, [this, colorType]() { slotSetColor(colorType); });
    return aColorPicker;
}

const QIcon AnnotationActionHandlerPrivate::widthIcon(double width)
{
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::black, 2 * width, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(0, pm.height() / 2, pm.width(), pm.height() / 2);
    p.end();
    return QIcon(pm);
}

const QIcon AnnotationActionHandlerPrivate::stampIcon(const QString &stampIconName)
{
    QPixmap stampPix = Okular::AnnotationUtils::loadStamp(stampIconName, QSize(32, 32));
    if (stampPix.width() == stampPix.height()) {
        return QIcon(stampPix);
    } else {
        return QIcon::fromTheme(QStringLiteral("tag"));
    }
}

void AnnotationActionHandlerPrivate::selectTool(int toolId)
{
    if (toolId != -1) {
        aBrowseTool->setChecked(false);
        annotator->setAnnotationSelectionMode(false);
        annotator->setEraserMode(false);
    }
    selectedBuiltinTool = toolId;
    annotator->selectBuiltinTool(toolId, PageViewAnnotator::ShowTip::Yes);
    parseTool(toolId);
}

void AnnotationActionHandlerPrivate::slotStampToolSelected(const QString &stamp)
{
    aBrowseTool->setChecked(false);
    annotator->setAnnotationSelectionMode(false);
    annotator->setEraserMode(false);
    selectedBuiltinTool = PageViewAnnotator::STAMP_TOOL_ID;
    annotator->selectStampTool(stamp); // triggers a reparsing thus calling parseTool
}

void AnnotationActionHandlerPrivate::slotQuickToolSelected(int favToolId)
{
    aBrowseTool->setChecked(false);
    annotator->setAnnotationSelectionMode(false);
    annotator->setEraserMode(false);
    annotator->selectQuickTool(favToolId);
    selectedBuiltinTool = -1;
    updateConfigActions();
    Okular::Settings::setQuickAnnotationDefaultAction(favToolId - 1);
    Okular::Settings::self()->save();
}

void AnnotationActionHandlerPrivate::slotSetColor(AnnotationColor colorType, const QColor &color)
{
    QColor selectedColor(color);
    if (!selectedColor.isValid()) {
        selectedColor = QColorDialog::getColor(currentColor, nullptr, i18nc("@title:window", "Select color"));
        if (!selectedColor.isValid()) {
            return;
        }
    }
    if (colorType == AnnotationColor::Color) {
        currentColor = selectedColor;
        annotator->setAnnotationColor(selectedColor);
    } else if (colorType == AnnotationColor::InnerColor) {
        currentInnerColor = selectedColor;
        annotator->setAnnotationInnerColor(selectedColor);
    }
}

void AnnotationActionHandlerPrivate::slotSelectAnnotationFont()
{
    bool ok;
    QFont selectedFont = QFontDialog::getFont(&ok, currentFont);
    if (ok) {
        currentFont = selectedFont;
        annotator->setAnnotationFont(currentFont);
    }
}

bool AnnotationActionHandlerPrivate::isQuickToolAction(QAction *aTool)
{
    return quickTools.contains(aTool);
}

bool AnnotationActionHandlerPrivate::isQuickToolStamp(int toolId)
{
    QDomElement toolElement = annotator->quickTool(toolId);
    const QString annotType = toolElement.attribute(QStringLiteral("type"));
    QDomElement engineElement = toolElement.firstChildElement(QStringLiteral("engine"));
    QDomElement annElement = engineElement.firstChildElement(QStringLiteral("annotation"));
    return annotType == QStringLiteral("stamp");
}

void AnnotationActionHandlerPrivate::assertToolBarExists(KParts::MainWindow *mw, const QString &toolBarName)
{
    QList<KToolBar *> toolbars = mw->toolBars();
    auto itToolBar = std::find_if(toolbars.begin(), toolbars.end(), [&](const KToolBar *toolBar) { return toolBar->objectName() == toolBarName; });
    Q_ASSERT(itToolBar != toolbars.end());
}

// TODO: icon names should match getAnnotationInfo in gui/guiutils.cpp
AnnotationActionHandler::AnnotationActionHandler(PageViewAnnotator *parent, KActionCollection *ac)
    : QObject(parent)
    , d(new AnnotationActionHandlerPrivate(this))
{
    d->annotator = parent;

    // toolbar visibility actions
    d->aToolBarVisibility = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18n("&Annotations"), this);
    d->aHideToolBar = new QAction(QIcon::fromTheme(QStringLiteral("dialog-close")), i18nc("@action:intoolbar Hide the drawing toolbar", "Hide drawing tools"), this);
    d->aShowToolBar = new QAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18nc("@action:intoolbar Show the drawing toolbar", "Show drawing tools"), this);

    // Text markup actions
    KToggleAction *aHighlighter = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-highlight")), i18nc("@action:intoolbar Annotation tool", "Highlighter"), this);
    KToggleAction *aUnderline = new KToggleAction(QIcon::fromTheme(QStringLiteral("format-text-underline")), i18nc("@action:intoolbar Annotation tool", "Underline"), this);
    KToggleAction *aSquiggle = new KToggleAction(QIcon::fromTheme(QStringLiteral("format-text-underline-squiggle")), i18nc("@action:intoolbar Annotation tool", "Squiggle"), this);
    KToggleAction *aStrikeout = new KToggleAction(QIcon::fromTheme(QStringLiteral("format-text-strikethrough")), i18nc("@action:intoolbar Annotation tool", "Strike Out"), this);
    // Notes actions
    KToggleAction *aTypewriter = new KToggleAction(QIcon::fromTheme(QStringLiteral("tool-text")), i18nc("@action:intoolbar Drawing tool", "Text"), this);
    KToggleAction *aInlineNote = new KToggleAction(QIcon::fromTheme(QStringLiteral("tool-text")), i18nc("@action:intoolbar Annotation tool", "Text"), this);
    KToggleAction *aPopupNote = new KToggleAction(QIcon::fromTheme(QStringLiteral("edit-comment")), i18nc("@action:intoolbar Annotation tool", "Popup Note"), this);
    d->aSelectTool = new KToggleAction(QIcon::fromTheme(QStringLiteral("edit-select-all")), i18nc("@action:intoolbar Drawing tool", "Select"), this);
    d->aEraserTool = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-eraser")), i18nc("@action:intoolbar Drawing tool", "Eraser"), this);
    d->aBrowseTool = ac->action(QStringLiteral("mouse_drag"));
    Q_ASSERT(d->aBrowseTool);
    d->aDrawingBrowse = new KToggleAction(QIcon::fromTheme(QStringLiteral("transform-browse")), i18nc("@action:intoolbar Drawing tool", "Browse"), this);
    d->aDrawingBrowse->setChecked(d->aBrowseTool->isChecked());
    KToggleAction *aFreehandLine = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18nc("@action:intoolbar Drawing tool", "Pen / Freehand"), this);
    // Geometrical shapes actions
    KToggleAction *aStraightLine = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-line")), i18nc("@action:intoolbar Annotation tool", "Straight line"), this);
    KToggleAction *aArrow = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-arrow")), i18nc("@action:intoolbar Annotation tool", "Arrow"), this);
    KToggleAction *aRectangle = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-rectangle")), i18nc("@action:intoolbar Annotation tool", "Rectangle"), this);
    KToggleAction *aEllipse = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-ellipse")), i18nc("@action:intoolbar Annotation tool", "Ellipse"), this);
    KToggleAction *aPolygon = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-polyline")), i18nc("@action:intoolbar Annotation tool", "Polygon"), this);
    d->drawingToolKinds.insert(aFreehandLine, AnnotationActionHandlerPrivate::DrawingToolKind::Pen);
    d->drawingToolKinds.insert(d->aEraserTool, AnnotationActionHandlerPrivate::DrawingToolKind::Eraser);
    d->drawingToolKinds.insert(aStraightLine, AnnotationActionHandlerPrivate::DrawingToolKind::Line);
    d->drawingToolKinds.insert(aArrow, AnnotationActionHandlerPrivate::DrawingToolKind::Arrow);
    d->drawingToolKinds.insert(aRectangle, AnnotationActionHandlerPrivate::DrawingToolKind::Rectangle);
    d->drawingToolKinds.insert(aEllipse, AnnotationActionHandlerPrivate::DrawingToolKind::Ellipse);
    d->drawingToolKinds.insert(aPolygon, AnnotationActionHandlerPrivate::DrawingToolKind::Polygon);
    d->drawingToolIds.insert(aFreehandLine, 8);
    d->drawingToolIds.insert(aStraightLine, 10);
    d->drawingToolIds.insert(aArrow, 9);
    d->drawingToolIds.insert(aRectangle, 11);
    d->drawingToolIds.insert(aEllipse, 12);
    d->drawingToolIds.insert(aPolygon, 13);
    d->drawingTools = {d->aDrawingBrowse, d->aSelectTool, aFreehandLine, d->aEraserTool, aStraightLine, aArrow, aRectangle, aEllipse, aPolygon, aTypewriter};
    d->aGeomShapes = new ToggleActionMenu(i18nc("@action", "Geometrical shapes"), this);
    d->aGeomShapes->setEnabled(true); // Need to explicitly set this once, or refreshActions() in part.cpp will disable this action
    d->aGeomShapes->setPopupMode(QToolButton::MenuButtonPopup);
    d->aGeomShapes->addAction(aArrow);
    d->aGeomShapes->addAction(aStraightLine);
    d->aGeomShapes->addAction(aRectangle);
    d->aGeomShapes->addAction(aEllipse);
    d->aGeomShapes->addAction(aPolygon);
    d->aGeomShapes->setDefaultAction(aArrow);
    connect(d->aGeomShapes->menu(), &QMenu::triggered, d->aGeomShapes, &ToggleActionMenu::setDefaultAction);

    // The order in which the actions are added is relevant to connect
    // them to the correct toolId defined in tools.xml
    d->agTools = new QActionGroup(this);
    d->agTools->setExclusive(true);
    d->agTools->addAction(aHighlighter);
    d->agTools->addAction(aUnderline);
    d->agTools->addAction(aSquiggle);
    d->agTools->addAction(aStrikeout);
    d->agTools->addAction(aTypewriter);
    d->agTools->addAction(aInlineNote);
    d->agTools->addAction(aPopupNote);
    d->agTools->addAction(aFreehandLine);
    d->agTools->addAction(aArrow);
    d->agTools->addAction(aStraightLine);
    d->agTools->addAction(aRectangle);
    d->agTools->addAction(aEllipse);
    d->agTools->addAction(aPolygon);

    d->textTools.append(aHighlighter);
    d->textTools.append(aUnderline);
    d->textTools.append(aSquiggle);
    d->textTools.append(aStrikeout);

    int toolId = 1;
    const QList<QAction *> tools = d->agTools->actions();
    for (const auto &ann : tools) {
        // action group workaround: connecting to toggled instead of triggered
        connect(ann, &QAction::toggled, this, [this, toolId](bool checked) {
            if (checked) {
                d->aBrowseTool->setChecked(false);
                d->annotator->setAnnotationSelectionMode(false);
                d->selectTool(toolId);
            }
        });
        toolId++;
    }

    d->agTools->addAction(d->aSelectTool);
    d->agTools->addAction(d->aEraserTool);
    connect(d->aSelectTool, &QAction::toggled, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        d->aBrowseTool->setChecked(false);
        d->annotator->setEraserMode(false);
        d->annotator->selectBuiltinTool(-1, PageViewAnnotator::ShowTip::No);
        d->annotator->setAnnotationSelectionMode(true);
        d->selectModeSwitchingToBrowse = true;
        d->aBrowseTool->trigger();
        d->selectModeSwitchingToBrowse = false;
        if (d->aBrowseTool->isCheckable()) {
            d->aBrowseTool->setChecked(false);
        }
    });
    connect(d->aEraserTool, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            d->aBrowseTool->setChecked(false);
            d->annotator->setAnnotationSelectionMode(false);
        }
        d->annotator->setEraserMode(checked);
        d->updateConfigActions();
    });
    connect(d->aBrowseTool, &QAction::toggled, this, [this](bool checked) {
        const QSignalBlocker blocker(d->aDrawingBrowse);
        d->aDrawingBrowse->setChecked(checked);
    });
    connect(d->aBrowseTool, &QAction::triggered, this, [this] {
        if (d->selectModeSwitchingToBrowse) {
            return;
        }
        d->annotator->setAnnotationSelectionMode(false);
        d->q->deselectAllAnnotationActions();
    });
    connect(d->aDrawingBrowse, &QAction::toggled, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        d->annotator->setAnnotationSelectionMode(false);
        d->selectModeSwitchingToBrowse = true;
        d->aBrowseTool->trigger();
        d->selectModeSwitchingToBrowse = false;
        d->q->deselectAllAnnotationActions();
    });

    // Stamp action
    d->aStamp = new ToggleActionMenu(QIcon::fromTheme(QStringLiteral("tag")), i18nc("@action", "Stamp"), this);
    d->aStamp->setPopupMode(QToolButton::MenuButtonPopup);
    for (const auto &stamp : StampAnnotationWidget::defaultStamps()) {
        KToggleAction *ann = new KToggleAction(d->stampIcon(stamp.second), stamp.first, this);
        d->aStamp->addAction(ann);
        d->agTools->addAction(ann);
        // action group workaround: connecting to toggled instead of triggered
        // (because deselectAllAnnotationActions has to call triggered)
        connect(ann, &QAction::toggled, this, [this, stamp](bool checked) {
            if (checked) {
                d->slotStampToolSelected(stamp.second);
            }
        });
    }
    if (!d->aStamp->menu()->actions().isEmpty()) {
        d->aStamp->setDefaultAction(d->aStamp->menu()->actions().constFirst());
    }
    connect(d->aStamp->menu(), &QMenu::triggered, d->aStamp, &ToggleActionMenu::setDefaultAction);

    // Quick annotations action
    d->aQuickTools = new ToggleActionMenu(i18nc("@action:intoolbar Show list of quick annotation tools", "Quick Annotations"), this);
    d->aQuickTools->setPopupMode(QToolButton::MenuButtonPopup);
    d->aQuickTools->setIcon(QIcon::fromTheme(QStringLiteral("draw-freehand")));
    d->aQuickTools->setToolTip(i18nc("@info:tooltip", "Choose an annotation tool from the quick annotations"));
    d->aQuickTools->setEnabled(true); // required to ensure that populateQuickAnnotations is executed the first time
    // set the triggered quick annotation as default action (but avoid setting 'Configure...' as default action)
    connect(d->aQuickTools->menu(), &QMenu::triggered, this, [this](QAction *action) {
        if (action->isCheckable()) {
            d->aQuickTools->setDefaultAction(action);
        }
    });

    d->aQuickToolsBar = new ActionBar(this);
    d->aQuickToolsBar->setText(i18n("Quick Annotation Bar"));

    QAction *aQuickToolsSeparator = new QAction(this);
    aQuickToolsSeparator->setSeparator(true);
    d->aQuickTools->addAction(aQuickToolsSeparator);
    d->aQuickTools->addAction(d->aShowToolBar);
    QAction *aConfigAnnotation = ac->action(QStringLiteral("options_configure_annotations"));
    if (aConfigAnnotation) {
        d->aQuickTools->addAction(aConfigAnnotation);
        d->aQuickToolsBar->addAction(aConfigAnnotation);
    }
    d->populateQuickAnnotations();

    // Add to quick annotation action
    d->aAddToQuickTools = new QAction(QIcon::fromTheme(QStringLiteral("favorite")), i18nc("@action:intoolbar Add current annotation tool to the quick annotations list", "Add to Quick Annotations"), this);

    // Pin action
    d->aContinuousMode = new KToggleAction(QIcon::fromTheme(QStringLiteral("pin")), i18nc("@action:intoolbar When checked keep the current annotation tool active after use", "Keep Active"), this);
    d->aContinuousMode->setToolTip(i18nc("@info:tooltip", "Keep the annotation tool active after use"));
    d->aContinuousMode->setChecked(d->annotator->continuousMode());

    // Constrain angle action
    d->aConstrainRatioAndAngle =
        new KToggleAction(QIcon::fromTheme(QStringLiteral("snap-angle")), i18nc("@action When checked, line annotations are constrained to 15° steps, shape annotations to 1:1 ratio", "Constrain Ratio and Angle of Annotation Tools"), this);
    d->aConstrainRatioAndAngle->setChecked(d->annotator->constrainRatioAndAngleActive());

    // Annotation settings actions
    d->aColor = d->colorPickerAction(AnnotationActionHandlerPrivate::AnnotationColor::Color);
    d->aInnerColor = d->colorPickerAction(AnnotationActionHandlerPrivate::AnnotationColor::InnerColor);
    d->aFont = new QAction(QIcon::fromTheme(QStringLiteral("font-face")), i18nc("@action:intoolbar Current annotation config option", "Font"), this);
    d->aAdvancedSettings = new QAction(QIcon::fromTheme(QStringLiteral("settings-configure")), i18nc("@action:intoolbar Current annotation advanced settings", "Annotation Settings"), this);

    // Width list
    d->aWidth = new KSelectAction(QIcon::fromTheme(QStringLiteral("edit-line-width")), i18nc("@action:intoolbar Current annotation config option", "Line width"), this);
    d->aWidth->setToolBarMode(KSelectAction::MenuMode);
    for (auto width : d->widthStandardValues) {
        KToggleAction *ann = new KToggleAction(d->widthIcon(width), i18nc("@item:inlistbox", "Width %1", width), this);
        d->aWidth->addAction(ann);
        connect(ann, &QAction::triggered, this, [this, width]() {
            d->annotator->setAnnotationWidth(width);
            d->updateConfigActions();
        });
    }

    // Opacity list
    d->aOpacity = new KSelectAction(QIcon::fromTheme(QStringLiteral("edit-opacity")), i18nc("@action:intoolbar Current annotation config option", "Opacity"), this);
    d->aOpacity->setToolBarMode(KSelectAction::MenuMode);
    for (double opacity : d->opacityStandardValues) {
        KToggleAction *ann = new KToggleAction(GuiUtils::createOpacityIcon(opacity), i18nc("@item:inlistbox Annotation opacity percentage level, make sure to include %1 in your translation", "%1%", opacity * 100), this);
        d->aOpacity->addAction(ann);
        connect(ann, &QAction::triggered, this, [this, opacity]() { d->annotator->setAnnotationOpacity(opacity); });
    }

    connect(d->aAddToQuickTools, &QAction::triggered, d->annotator, &PageViewAnnotator::addToQuickAnnotations);
    connect(d->aContinuousMode, &QAction::toggled, d->annotator, &PageViewAnnotator::setContinuousMode);
    connect(d->aConstrainRatioAndAngle, &QAction::toggled, d->annotator, &PageViewAnnotator::setConstrainRatioAndAngle);
    connect(d->aAdvancedSettings, &QAction::triggered, d->annotator, &PageViewAnnotator::slotAdvancedSettings);
    connect(d->aFont, &QAction::triggered, std::bind(&AnnotationActionHandlerPrivate::slotSelectAnnotationFont, d));

    // action group workaround: allows unchecking the currently selected annotation action.
    // Other parts of code dependent to this workaround are marked with "action group workaround".
    connect(d->agTools, &QActionGroup::triggered, this, [this](QAction *action) {
        if (action == d->agLastAction) {
            const bool mouseRepeat = d->pendingMousePopupAction == action;
            d->pendingMousePopupAction = nullptr;
            if (d->deselectingTools) {
                d->agLastAction = nullptr;
                d->agTools->checkedAction()->setChecked(false);
                d->selectTool(-1);
            } else if (mouseRepeat && d->showToolSettings(action)) {
                return;
            } else if (!d->drawingToolKinds.contains(action)) {
                d->agLastAction = nullptr;
                d->agTools->checkedAction()->setChecked(false);
                d->selectTool(-1);
            }
        } else {
            d->pendingMousePopupAction = nullptr;
            d->agLastAction = action;
            // Show the annotation toolbar whenever builtin tool actions are triggered (e.g using shortcuts)
            if (!d->isQuickToolAction(action)) {
                d->aToolBarVisibility->setChecked(true);
            }
        }
    });

    auto *drawingToolCategory = new KActionCategory(i18n("Drawing Toolbar"), ac);
    drawingToolCategory->addAction(QStringLiteral("drawing_browse"), d->aDrawingBrowse);
    drawingToolCategory->addAction(QStringLiteral("drawing_select"), d->aSelectTool);
    drawingToolCategory->addAction(QStringLiteral("annotation_freehand_line"), aFreehandLine);
    drawingToolCategory->addAction(QStringLiteral("drawing_eraser"), d->aEraserTool);
    drawingToolCategory->addAction(QStringLiteral("annotation_straight_line"), aStraightLine);
    drawingToolCategory->addAction(QStringLiteral("annotation_arrow"), aArrow);
    drawingToolCategory->addAction(QStringLiteral("annotation_rectangle"), aRectangle);
    drawingToolCategory->addAction(QStringLiteral("annotation_ellipse"), aEllipse);
    drawingToolCategory->addAction(QStringLiteral("annotation_polygon"), aPolygon);
    drawingToolCategory->addAction(QStringLiteral("annotation_typewriter"), aTypewriter);

    ac->addAction(QStringLiteral("mouse_toggle_annotate"), d->aToolBarVisibility);
    ac->addAction(QStringLiteral("hide_annotation_toolbar"), d->aHideToolBar);
    ac->addAction(QStringLiteral("quick_annotation_action_bar"), d->aQuickToolsBar);
    ac->addAction(QStringLiteral("annotation_highlighter"), aHighlighter);
    ac->addAction(QStringLiteral("annotation_underline"), aUnderline);
    ac->addAction(QStringLiteral("annotation_squiggle"), aSquiggle);
    ac->addAction(QStringLiteral("annotation_strike_out"), aStrikeout);
    ac->addAction(QStringLiteral("annotation_inline_note"), aInlineNote);
    ac->addAction(QStringLiteral("annotation_popup_note"), aPopupNote);
    ac->addAction(QStringLiteral("annotation_geometrical_shape"), d->aGeomShapes);
    ac->addAction(QStringLiteral("annotation_stamp"), d->aStamp);
    ac->addAction(QStringLiteral("annotation_favorites"), d->aQuickTools);
    ac->addAction(QStringLiteral("annotation_bookmark"), d->aAddToQuickTools);
    ac->addAction(QStringLiteral("annotation_settings_pin"), d->aContinuousMode);
    ac->addAction(QStringLiteral("annotation_constrain_ratio_angle"), d->aConstrainRatioAndAngle);
    ac->addAction(QStringLiteral("annotation_settings_width"), d->aWidth);
    ac->addAction(QStringLiteral("annotation_settings_color"), d->aColor);
    ac->addAction(QStringLiteral("annotation_settings_inner_color"), d->aInnerColor);
    ac->addAction(QStringLiteral("annotation_settings_opacity"), d->aOpacity);
    ac->addAction(QStringLiteral("annotation_settings_font"), d->aFont);
    ac->addAction(QStringLiteral("annotation_settings_advanced"), d->aAdvancedSettings);

    ac->setDefaultShortcut(d->aToolBarVisibility, Qt::Key_F6);
    ac->setDefaultShortcut(aHighlighter, Qt::ALT | Qt::Key_1);
    ac->setDefaultShortcut(aUnderline, Qt::ALT | Qt::Key_2);
    ac->setDefaultShortcut(aSquiggle, Qt::ALT | Qt::Key_3);
    ac->setDefaultShortcut(aStrikeout, Qt::ALT | Qt::Key_4);
    ac->setDefaultShortcut(aTypewriter, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_T));
    ac->setDefaultShortcut(aInlineNote, Qt::ALT | Qt::Key_6);
    ac->setDefaultShortcut(aPopupNote, Qt::ALT | Qt::Key_7);
    ac->setDefaultShortcut(aFreehandLine, Qt::Key_1);
    ac->setDefaultShortcut(aArrow, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_A));
    ac->setDefaultShortcut(aRectangle, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_R));
    ac->setDefaultShortcut(d->aBrowseTool, QKeySequence(Qt::CTRL | Qt::Key_1));
    ac->setDefaultShortcut(d->aDrawingBrowse, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_B));
    ac->setDefaultShortcut(d->aSelectTool, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_S));
    ac->setDefaultShortcut(d->aEraserTool, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_E));
    ac->setDefaultShortcut(aStraightLine, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_L));
    ac->setDefaultShortcut(aEllipse, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_O));
    ac->setDefaultShortcut(aPolygon, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_P));
    ac->setDefaultShortcut(d->aAddToQuickTools, QKeySequence((Qt::CTRL | Qt::SHIFT) | Qt::Key_B));
    d->updateConfigActions();

    connect(Okular::Settings::self(), &Okular::Settings::primaryAnnotationToolBarChanged, this, &AnnotationActionHandler::setupAnnotationToolBarVisibilityAction);
}

AnnotationActionHandler::~AnnotationActionHandler()
{
    // delete the private data storage structure
    delete d;
}

bool AnnotationActionHandler::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto action = d->settingsButtonActions.constFind(watched);
        if (action != d->settingsButtonActions.cend() && d->agTools->checkedAction() == *action) {
            d->pendingMousePopupAction = *action;
        }
    }
    return QObject::eventFilter(watched, event);
}

void AnnotationActionHandler::setupAnnotationToolBarVisibilityAction()
{
    // find the main window associated to the toggle toolbar action
    QList<QObject *> objects = d->aToolBarVisibility->associatedObjects();
    auto itMainWindow = std::find_if(objects.begin(), objects.end(), [](const QObject *object) { return qobject_cast<const KParts::MainWindow *>(object) != nullptr; });
    Q_ASSERT(itMainWindow != objects.end());
    KParts::MainWindow *mw = qobject_cast<KParts::MainWindow *>(*itMainWindow);

    // ensure that the annotation toolbars have been created
    d->assertToolBarExists(mw, QStringLiteral("annotationToolBar"));
    d->assertToolBarExists(mw, QStringLiteral("quickAnnotationToolBar"));
    KToolBar *drawingToolBar = nullptr;
    for (KToolBar *toolBar : mw->toolBars()) {
        if (toolBar->objectName() == QLatin1String("drawingToolBar")) {
            drawingToolBar = toolBar;
            break;
        }
    }
    const bool createDrawingToolBar = !drawingToolBar;
    if (!drawingToolBar) {
        drawingToolBar = new KToolBar(QStringLiteral("drawingToolBar"), mw, Qt::LeftToolBarArea, false, false, false);
        drawingToolBar->setWindowTitle(i18n("Drawing Tools"));
        drawingToolBar->setOrientation(Qt::Vertical);
        drawingToolBar->setIconSize(QSize(32, 32));
        drawingToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        drawingToolBar->setMovable(false);
        drawingToolBar->setFloatable(false);
        drawingToolBar->setAllowedAreas(Qt::LeftToolBarArea);
    }
    drawingToolBar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #1d242d; border: 0; border-right: 1px solid #394451; spacing: 4px; padding: 7px 4px; }"
        "QToolButton { color: #e8edf2; background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 5px; margin: 1px; }"
        "QToolButton:hover { background: #303a46; }"
        "QToolButton:checked { background: #16414c; border-color: #27bfd6; }"
        "QToolButton:focus { border-color: #27bfd6; }"
        "QToolButton[chartaRole='erase']:checked { background: #393541; border-color: #b59ad8; }"
        "QToolButton[chartaRole='shape']:checked { background: #403725; border-color: #e6b45d; }"
        "QToolButton[chartaRole='text']:checked { background: #402d38; border-color: #e58ca9; }"
        "QToolBar::separator { height: 1px; margin: 5px 7px; background: #45515e; }"));
    d->drawingToolBar = drawingToolBar;
    if (drawingToolBar->actions().isEmpty()) {
        for (QAction *action : std::as_const(d->drawingTools)) {
            drawingToolBar->addAction(action);
        }
    }
    for (qsizetype index = 0; index < d->drawingTools.size(); ++index) {
        const QString role = index == 3 ? QStringLiteral("erase") : (index >= 4 && index <= 8 ? QStringLiteral("shape") : (index == 9 ? QStringLiteral("text") : QStringLiteral("navigation")));
        if (QWidget *toolButton = drawingToolBar->widgetForAction(d->drawingTools.at(index))) {
            toolButton->setProperty("chartaRole", role);
        }
    }
    for (auto it = d->drawingToolKinds.cbegin(); it != d->drawingToolKinds.cend(); ++it) {
        if (QWidget *toolButton = drawingToolBar->widgetForAction(it.key())) {
            if (!d->settingsButtonActions.contains(toolButton)) {
                d->settingsButtonActions.insert(toolButton, it.key());
                toolButton->installEventFilter(this);
                connect(toolButton, &QObject::destroyed, this, [this, toolButton] { d->settingsButtonActions.remove(toolButton); });
            }
        }
    }
    if (createDrawingToolBar) {
        drawingToolBar->show();
    }

    KToolBar *annotationToolBar = mw->toolBar(QStringLiteral("annotationToolBar"));
    KToolBar *quickAnnotationToolBar = mw->toolBar(QStringLiteral("quickAnnotationToolBar"));
    if (KToolBar *mainToolBar = mw->toolBar(QStringLiteral("mainToolBar"))) {
        mainToolBar->removeAction(d->aQuickTools);
        mainToolBar->setStyleSheet(QStringLiteral(
            "QToolBar { background: #1d242d; border: 0; border-bottom: 1px solid #394451; spacing: 5px; padding: 4px 7px; }"
            "QToolButton { color: #e8edf2; background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 5px; }"
            "QToolButton:hover { background: #303a46; }"
            "QToolButton:checked { background: #16414c; border-color: #27bfd6; }"
            "QToolButton:focus { border-color: #27bfd6; }"
            "QToolBar::separator { width: 1px; margin: 5px 6px; background: #45515e; }"));
    }
    annotationToolBar->hide();
    quickAnnotationToolBar->hide();
    QObject::disconnect(d->aShowToolBar, &QAction::triggered, annotationToolBar, &KToolBar::show);
    QObject::disconnect(d->aHideToolBar, &QAction::triggered, annotationToolBar, &KToolBar::hide);
    QObject::disconnect(annotationToolBar, &QToolBar::visibilityChanged, this, &AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged);
    QObject::disconnect(d->aToolBarVisibility, nullptr, annotationToolBar, nullptr);
    QObject::disconnect(d->aToolBarVisibility, nullptr, quickAnnotationToolBar, nullptr);
    QObject::disconnect(annotationToolBar, nullptr, d->aToolBarVisibility, nullptr);
    QObject::disconnect(quickAnnotationToolBar, nullptr, d->aToolBarVisibility, nullptr);

    connect(drawingToolBar, &QToolBar::visibilityChanged, this, &AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged, Qt::UniqueConnection);
    connect(d->aShowToolBar, &QAction::triggered, drawingToolBar, &KToolBar::show, Qt::UniqueConnection);
    connect(d->aHideToolBar, &QAction::triggered, drawingToolBar, &KToolBar::hide, Qt::UniqueConnection);
    QObject::disconnect(d->aToolBarVisibility, &QAction::toggled, drawingToolBar, &KToolBar::setVisible);
    QObject::disconnect(drawingToolBar, &QToolBar::visibilityChanged, d->aToolBarVisibility, &QAction::setChecked);
    d->aToolBarVisibility->setChecked(drawingToolBar->isVisible());
    connect(drawingToolBar, &QToolBar::visibilityChanged, d->aToolBarVisibility, &QAction::setChecked, Qt::UniqueConnection);
    connect(d->aToolBarVisibility, &QAction::toggled, drawingToolBar, &KToolBar::setVisible, Qt::UniqueConnection);
    d->aShowToolBar->setEnabled(!drawingToolBar->isVisible());
}

void AnnotationActionHandler::reparseBuiltinToolsConfig()
{
    d->parseTool(d->selectedBuiltinTool);
}

void AnnotationActionHandler::reparseQuickToolsConfig()
{
    d->populateQuickAnnotations();
}

void AnnotationActionHandler::setToolsEnabled(bool on)
{
    d->aDrawingBrowse->setEnabled(on);
    d->aBrowseTool->setEnabled(on);
    d->aSelectTool->setEnabled(on);
    d->aEraserTool->setEnabled(on);
    const QList<QAction *> tools = d->agTools->actions();
    for (QAction *ann : tools) {
        ann->setEnabled(on);
    }
    d->aQuickTools->setEnabled(on);
    d->aGeomShapes->setEnabled(on);
    d->aStamp->setEnabled(on);
    d->aContinuousMode->setEnabled(on);
}

void AnnotationActionHandler::setTextToolsEnabled(bool on)
{
    d->textToolsEnabled = on;
    for (QAction *ann : std::as_const(d->textTools)) {
        ann->setEnabled(on);
    }
    for (QAction *ann : std::as_const(d->textQuickTools)) {
        ann->setEnabled(on);
    }
}

void AnnotationActionHandler::deselectAllAnnotationActions()
{
    QAction *checkedAction = d->agTools->checkedAction();
    if (checkedAction) {
        d->deselectingTools = true;
        checkedAction->trigger(); // action group workaround: using trigger instead of setChecked
        d->deselectingTools = false;
    }
}

void AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged(bool visible)
{
    d->aShowToolBar->setEnabled(!visible);
    if (!visible && !d->isQuickToolAction(d->agTools->checkedAction())) {
        deselectAllAnnotationActions();
    }
}

#include "moc_annotationactionhandler.cpp"
