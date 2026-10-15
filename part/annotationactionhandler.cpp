/*
    SPDX-FileCopyrightText: 2019 Simone Gaiarin <simgunz@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "annotationactionhandler.h"
#include "annotationcolorhistory.h"

// qt includes
#include <QActionGroup>
#include <QActionEvent>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QBitmap>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileInfo>
#include <QFontDialog>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsItem>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeySequence>
#include <QKeyEvent>
#include <QLineEdit>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointer>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QRadioButton>
#include <QSlider>
#include <QSignalBlocker>
#include <QScreen>
#include <QStackedLayout>
#include <QTimer>
#include <QTextEdit>
#include <QVBoxLayout>

// kde includes
#include <KActionCollection>
#include <KActionCategory>
#include <KLocalizedString>
#include <KMessageBox>
#include <KParts/MainWindow>
#include <KSelectAction>
#include <KToolBar>

#include <optional>
#include <functional>
#include <memory>

// local includes
#include "actionbar.h"
#include "annotationwidgets.h"
#include "core/annotations.h"
#include "core/document.h"
#include "core/page.h"
#include "gui/guiutils.h"
#include "pageview.h"
#include "pageviewannotator.h"
#include "settings.h"
#include "toggleactionmenu.h"
#include "widthpresetutils.h"

namespace
{
constexpr int temporaryToolHoldThresholdMs = 180;

std::optional<AnnotationActionHandler::TemporaryTool> temporaryToolForKey(int key)
{
    if (key == Qt::Key_Space) {
        return AnnotationActionHandler::TemporaryTool::Pan;
    }
    if (key == Qt::Key_E) {
        return AnnotationActionHandler::TemporaryTool::Eraser;
    }
    return std::nullopt;
}

class WidthPresetLineEdit final : public QLineEdit
{
public:
    explicit WidthPresetLineEdit(QWidget *parent)
        : QLineEdit(parent)
    {
        connect(this, &QLineEdit::editingFinished, this, [this] {
            if (m_cancelled || m_committing || !m_commit) {
                return;
            }
            m_committing = true;
            const bool saved = m_commit(text());
            m_committing = false;
            if (!saved) {
                QTimer::singleShot(0, this, [this] {
                    if (isVisible()) {
                        setFocus();
                        selectAll();
                    }
                });
            }
        });
    }

    void setCallbacks(std::function<bool(const QString &)> commit, std::function<void()> cancel)
    {
        m_commit = std::move(commit);
        m_cancel = std::move(cancel);
    }

    void beginEditing(const QString &value)
    {
        m_cancelled = false;
        setText(value);
        selectAll();
        setFocus();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            m_cancelled = true;
            if (m_cancel) {
                m_cancel();
            }
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }

private:
    std::function<bool(const QString &)> m_commit;
    std::function<void()> m_cancel;
    bool m_cancelled = false;
    bool m_committing = false;
};

class DrawingToolBarIsland final : public KToolBar
{
public:
    enum class Anchor { Left, Right };

    DrawingToolBarIsland(QWidget *host, QWidget *visibleCanvas, QWidget *geometryOwner = nullptr)
        : KToolBar(QStringLiteral("drawingToolBar"), geometryOwner ? geometryOwner : host, false)
        , m_host(host)
        , m_geometryOwner(geometryOwner)
        , m_visibleCanvas(visibleCanvas)
        , m_window(host->window())
    {
        setOrientation(Qt::Vertical);
        setIconSize(QSize(18, 18));
        setToolButtonStyle(Qt::ToolButtonIconOnly);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setMovable(false);
        setFloatable(false);
        host->installEventFilter(this);
        visibleCanvas->installEventFilter(this);
        m_window->installEventFilter(this);
    }

    void setAnchor(Anchor anchor)
    {
        m_anchor = anchor;
        scheduleIslandUpdate();
    }

    void setCompactView(QWidget *compactView)
    {
        m_compactView = compactView;
        scheduleIslandUpdate();
    }

    void setCompactPresentation(bool compact)
    {
        if (!m_geometryOwner) {
            setVisible(!compact);
            return;
        }
        if (m_compactView) {
            m_compactView->setVisible(compact);
        }
        setVisible(!compact);
        if (m_geometryOwner->layout()) {
            m_geometryOwner->layout()->activate();
        }
        m_geometryOwner->adjustSize();
        updateIslandGeometry();
    }

    void setRequestedVisible(bool visible)
    {
        m_requestedVisible = visible;
        if (m_geometryOwner) {
            m_geometryOwner->setVisible(visible);
        } else {
            setVisible(visible);
        }
        scheduleIslandUpdate();
    }

    bool requestedVisible() const
    {
        return m_requestedVisible;
    }

    void refreshGeometry()
    {
        scheduleIslandUpdate();
    }

protected:
    void actionEvent(QActionEvent *event) override
    {
        KToolBar::actionEvent(event);
        updateIslandGeometry();
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const bool geometryEvent = event->type() == QEvent::Resize || event->type() == QEvent::Show;
        if ((watched == m_host || watched == m_visibleCanvas) && geometryEvent) {
            scheduleIslandUpdate();
        } else if (watched == m_window && event->type() == QEvent::WindowStateChange) {
            scheduleIslandUpdate();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void scheduleIslandUpdate()
    {
        if (m_updateScheduled) {
            return;
        }
        m_updateScheduled = true;
        QTimer::singleShot(0, this, [this] {
            m_updateScheduled = false;
            if (!m_host || !m_visibleCanvas) {
                return;
            }
            if (m_requestedVisible && (!m_geometryOwner || !m_geometryOwner->isVisible()) && m_host->isVisible()) {
                if (m_geometryOwner) {
                    m_geometryOwner->show();
                } else {
                    show();
                }
            }
            updateIslandGeometry();
        });
    }

    void updateIslandGeometry()
    {
        QWidget *geometryOwner = m_geometryOwner ? m_geometryOwner : this;
        if (!m_host || !m_visibleCanvas || !m_host->isAncestorOf(m_visibleCanvas.data())) {
            return;
        }
        if (geometryOwner->layout()) {
            geometryOwner->layout()->activate();
        }
        geometryOwner->adjustSize();
        const int margin = 12;
        const QPoint canvasOrigin = m_visibleCanvas->mapTo(m_host.data(), QPoint(0, 0));
        const QRect canvasRect(canvasOrigin, m_visibleCanvas->size());
        const int x = canvasRect.right() - geometryOwner->width() - margin + 1;
        const int y = canvasRect.top() + (canvasRect.height() - geometryOwner->height()) / 2;
        geometryOwner->move(x, y);
        geometryOwner->raise();
    }

    QPointer<QWidget> m_host;
    QPointer<QWidget> m_geometryOwner;
    QPointer<QWidget> m_visibleCanvas;
    QPointer<QWidget> m_window;
    QPointer<QWidget> m_compactView;
    Anchor m_anchor = Anchor::Left;
    bool m_requestedVisible = true;
    bool m_updateScheduled = false;
};

class CompactToolBarView final : public QWidget
{
public:
    CompactToolBarView(KToolBar *toolbar, QAction *modeAction, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_toolbar(toolbar)
        , m_modeAction(modeAction)
    {
        setObjectName(QStringLiteral("drawingToolBarCompactView"));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(2);
        setStyleSheet(QStringLiteral(
            "QWidget { background: #202630; border: 1px solid #3b4653; border-radius: 10px; }"
            "QToolButton { color: #aab6c2; background: transparent; border: 1px solid transparent; border-radius: 6px; min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; padding: 2px; margin: 0; }"
            "QToolButton:hover { color: #f2f5f8; background: #303a46; }"
            "QToolButton:checked { color: #ffffff; background: #16414c; border-color: #27bfd6; }"
            "QToolButton[compactToggle=true]:checked { color: #ffffff; background: transparent; border-color: transparent; }"
            "QToolButton:focus { border-color: #27bfd6; }"));

        m_activeToolButton = new QToolButton(this);
        m_activeToolButton->setAutoRaise(true);
        m_activeToolButton->setCheckable(true);
        m_activeToolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_activeToolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(m_activeToolButton, &QToolButton::clicked, this, [this] {
            if (auto *activeAction = m_activeAction) {
                if (auto *realButton = qobject_cast<QToolButton *>(m_toolbar->widgetForAction(activeAction))) {
                    realButton->animateClick();
                    return;
                }
                activeAction->trigger();
            }
        });

        m_toggleButton = new QToolButton(this);
        m_toggleButton->setObjectName(QStringLiteral("drawingToolBarCompactModeToggle"));
        m_toggleButton->setProperty("compactToggle", true);
        m_toggleButton->setAutoRaise(true);
        m_toggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_toggleButton->setDefaultAction(modeAction);

        layout->addWidget(m_activeToolButton);
        layout->addWidget(m_toggleButton);
        setVisible(false);
    }

    void setActiveAction(QAction *action)
    {
        m_activeAction = action;
        if (!m_activeToolButton) {
            return;
        }
        if (!action) {
            m_activeToolButton->setIcon(QIcon());
            m_activeToolButton->setText(QString());
            m_activeToolButton->setToolTip(QString());
            m_activeToolButton->setEnabled(false);
            m_activeToolButton->setChecked(false);
            return;
        }
        m_activeToolButton->setEnabled(action->isEnabled());
        m_activeToolButton->setChecked(true);
        m_activeToolButton->setIcon(action->icon());
        m_activeToolButton->setText(action->text());
        m_activeToolButton->setToolTip(action->toolTip().isEmpty() ? action->text() : action->toolTip());

        if (auto *realButton = qobject_cast<QToolButton *>(m_toolbar->widgetForAction(action))) {
            m_activeToolButton->setIcon(realButton->icon());
            m_activeToolButton->setToolTip(realButton->toolTip().isEmpty() ? action->text() : realButton->toolTip());
        }
    }

    QWidget *activeToolButton() const
    {
        return m_activeToolButton;
    }

private:
    KToolBar *m_toolbar = nullptr;
    QAction *m_modeAction = nullptr;
    QAction *m_activeAction = nullptr;
    QToolButton *m_activeToolButton = nullptr;
    QToolButton *m_toggleButton = nullptr;
};

class ToolSettingsPopupDismissFilter final : public QObject
{
public:
    explicit ToolSettingsPopupDismissFilter(QDialog *popup, QAction *action, QObject *parent)
        : QObject(parent)
        , m_popup(popup)
        , m_action(action)
    {
    }

    bool awaitingEscapeRelease() const
    {
        return m_escapeReleasePending;
    }

    bool matchesToolShortcut(const QKeyEvent *keyEvent) const
    {
        if (!m_action) {
            return false;
        }
        const QKeySequence pressedShortcut(keyEvent->key() | keyEvent->modifiers());
        const QList<QKeySequence> shortcuts = m_action->shortcuts();
        return std::any_of(shortcuts.cbegin(), shortcuts.cend(), [&pressedShortcut](const QKeySequence &shortcut) {
            return shortcut.count() == 1 && shortcut == pressedShortcut;
        });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (m_escapeReleasePending) {
            if (event->type() == QEvent::KeyRelease) {
                auto *keyEvent = static_cast<QKeyEvent *>(event);
                if (keyEvent->key() == Qt::Key_Escape) {
                    keyEvent->accept();
                    m_escapeReleasePending = false;
                    qApp->removeEventFilter(this);
                    deleteLater();
                    return true;
                }
            }
            return QObject::eventFilter(watched, event);
        }

        if (!m_popup || !m_popup->isVisible()) {
            return QObject::eventFilter(watched, event);
        }

        if (event->type() == QEvent::ShortcutOverride) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            QWidget *modalWidget = QApplication::activeModalWidget();
            if (keyEvent->key() == Qt::Key_Escape && (!modalWidget || modalWidget == m_popup || m_popup->isAncestorOf(modalWidget))) {
                keyEvent->accept();
                return true;
            }
            if (matchesToolShortcut(keyEvent) && (!modalWidget || modalWidget == m_popup || m_popup->isAncestorOf(modalWidget))) {
                keyEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            QWidget *modalWidget = QApplication::activeModalWidget();
            if (keyEvent->key() == Qt::Key_Escape && (!modalWidget || modalWidget == m_popup || m_popup->isAncestorOf(modalWidget))) {
                keyEvent->accept();
                m_escapeReleasePending = true;
                m_popup->reject();
                return true;
            }
            if (matchesToolShortcut(keyEvent) && (!modalWidget || modalWidget == m_popup || m_popup->isAncestorOf(modalWidget))) {
                keyEvent->accept();
                m_popup->reject();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            QWidget *modalWidget = QApplication::activeModalWidget();
            if (modalWidget && modalWidget != m_popup && modalWidget->isVisible()) {
                return QObject::eventFilter(watched, event);
            }

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (!m_popup->frameGeometry().contains(mouseEvent->globalPosition().toPoint())) {
                QWidget *activePopup = QApplication::activePopupWidget();
                if (!activePopup || !activePopup->isVisible() || !activePopup->frameGeometry().contains(mouseEvent->globalPosition().toPoint())) {
                    m_popup->reject();
                }
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QDialog *m_popup;
    QPointer<QAction> m_action;
    bool m_escapeReleasePending = false;
};
}

class ResponsiveSlider : public QSlider
{
public:
    using QSlider::QSlider;

protected:
    int pixelPosToRangeValue(int pos) const
    {
        QStyleOptionSlider opt;
        initStyleOption(&opt);

        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        const int span = qMax(1, groove.width() - handle.width());

        return QStyle::sliderValueFromPosition(minimum(), maximum(), pos - groove.x(), span, opt.upsideDown);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QSlider::mousePressEvent(event);
            return;
        }

        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        if (!handleRect.contains(event->pos())) {
            event->accept();
            const int value = pixelPosToRangeValue(event->pos().x());
            setValue(value);
            return;
        }

        QSlider::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        QSlider::mouseMoveEvent(event);
    }
};

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
    QList<double> widthStandardValues() const;
    void populateWidthActions();
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
        , aMouseSelectTools(nullptr)
        , aCustomStamp(nullptr)
        , aCustomWidth(nullptr)
        , aCustomOpacity(nullptr)
        , aColorCycle(nullptr)
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
    void rebuildColorPickerAction(KSelectAction *action, AnnotationColor colorType);

    const QIcon widthIcon(double width);
    const QIcon stampIcon(const QString &stampIconName);

    void selectTool(int toolId);
    void slotStampToolSelected(const QString &stamp);
    void slotQuickToolSelected(int favToolId);
    void slotSetColor(AnnotationColor colorType, const QColor &color = QColor());
    void slotSelectAnnotationFont();
    bool showToolSettings(QAction *action, Okular::Document *document = nullptr, const QList<Okular::Annotation *> &annotations = {}, int pageNumber = -1, QWidget *anchorWidget = nullptr);
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
    QPointer<QDialog> activeToolSettingsPopup;
    QAction *activeToolSettingsAction = nullptr;
    bool deselectingTools = false;
    QAction *temporaryToolPreviousAction = nullptr;
    QAction *temporaryToolAction = nullptr;
    bool restoringTemporaryTool = false;
    bool changingTemporaryTool = false;
    QList<int> heldTemporaryToolKeys;
    QTimer *temporaryToolHoldTimer = nullptr;
    bool selectionToolWasActiveOnPress = false;
    bool freeformHighlightActive = false;
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
    KToggleAction *aLassoSelectTool = nullptr;
    ToggleActionMenu *aSelectTools = nullptr;
    KToggleAction *aCompactMode = nullptr;
    KToggleAction *aEraserTool;
    KToggleAction *aDrawingBrowse;
    QAction *aBrowseTool;
    QAction *aMouseSelectTools;
    KToolBar *drawingToolBar = nullptr;
    CompactToolBarView *compactToolBarView = nullptr;
    bool selectModeSwitchingToBrowse = false;

    QAction *aCustomStamp;
    QAction *aCustomWidth;
    QAction *aCustomOpacity;
    QAction *aColorCycle = nullptr;
    AnnotationWidthPresets::SelectionTracker widthPresetActivation;

    QColor currentColor;
    QColor currentInnerColor;
    QFont currentFont;
    int currentWidth;

    int selectedBuiltinTool;
    bool textToolsEnabled;
};

QList<double> AnnotationActionHandlerPrivate::widthStandardValues() const
{
    return AnnotationWidthPresets::presetValuesOrDefaults(Okular::Settings::annotationWidthMenuPresets(), AnnotationWidthPresets::defaultMenuPresetStrings());
}

void AnnotationActionHandlerPrivate::populateWidthActions()
{
    for (QAction *action : aWidth->actions()) {
        aWidth->removeAction(action);
        delete action;
    }
    aCustomWidth = nullptr;
    widthPresetActivation.setSelectedIndex(-1);

    const QList<double> presetWidths = widthStandardValues();
    for (qsizetype index = 0; index < presetWidths.size(); ++index) {
        const double width = presetWidths.at(index);
        auto *action = new KToggleAction(widthIcon(width), i18nc("@item:inlistbox", "Width %1", width), q);
        aWidth->addAction(action);
        QObject::connect(action, &QAction::triggered, q, [this, index, width] {
            if (widthPresetActivation.activate(index)) {
                bool accepted = false;
                QWidget *dialogParent = QApplication::activeWindow();
                const double editedWidth = QInputDialog::getDouble(dialogParent,
                                                                   i18nc("@title:window", "Edit Width Preset"),
                                                                   i18nc("@label:spinbox", "Width (px):"),
                                                                   width,
                                                                   AnnotationWidthPresets::minimumWidth,
                                                                   AnnotationWidthPresets::maximumWidth,
                                                                   3,
                                                                   &accepted);
                if (accepted) {
                    const QStringList currentPresets = Okular::Settings::annotationWidthMenuPresets();
                    QStringList updatedPresets;
                    if (!AnnotationWidthPresets::replacePresetString(currentPresets, index, QLocale::c().toString(editedWidth, 'g', 12), &updatedPresets)) {
                        KMessageBox::error(dialogParent, i18nc("@info", "That width is already in the preset list or is outside the allowed range."));
                    } else {
                        Okular::Settings::setAnnotationWidthMenuPresets(updatedPresets);
                        Okular::Settings::self()->save();
                        parseTool(selectedBuiltinTool);
                    }
                }
                return;
            }
            annotator->setAnnotationWidth(width);
            updateConfigActions();
        });
    }
}

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

bool AnnotationActionHandlerPrivate::showToolSettings(QAction *action, Okular::Document *document, const QList<Okular::Annotation *> &selectedAnnotations, int pageNumber, QWidget *anchorWidget)
{
    const auto kindIt = drawingToolKinds.constFind(action);
    if (kindIt == drawingToolKinds.cend()) {
        return false;
    }
    const bool restoreActiveAction = action->isCheckable() && agLastAction == action;
    if (restoreActiveAction && !action->isChecked()) {
        const QSignalBlocker blocker(action);
        action->setChecked(true);
    }
    const bool isFreeformHighlight = drawingToolIds.value(action, -1) == 15;
    Okular::Annotation *selectedAnnotation = selectedAnnotations.isEmpty() ? nullptr : selectedAnnotations.constFirst();

    const QString key = action->objectName().isEmpty() ? action->text() : action->objectName();
    DrawingToolSettings &initialSettings = drawingToolSettings[key];
    if (selectedAnnotation) {
        initialSettings.width = selectedAnnotation->style().width() / 0.75;
        initialSettings.color = selectedAnnotation->style().color();
        initialSettings.fillColor = QColor();
        initialSettings.fillOpacity = 0;
        if (selectedAnnotation->subType() == Okular::Annotation::AGeom) {
            const auto *geometry = static_cast<const Okular::GeomAnnotation *>(selectedAnnotation);
            initialSettings.fillColor = geometry->geometricalInnerColor();
            initialSettings.fillOpacity = geometry->geometricalInnerColor().isValid() ? geometry->geometricalInnerColor().alpha() * 100 / 255 : 0;
        }
        initialSettings.initialized = true;
    }
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

    QStringList selectedAnnotationNames;
    for (Okular::Annotation *annotation : selectedAnnotations) {
        selectedAnnotationNames.append(annotation->uniqueName());
    }
    bool selectedAnnotationChanged = false;
    bool selectedWidthChanged = false;
    bool selectedColorChanged = false;
    bool selectedFillColorChanged = false;
    bool selectedFillOpacityChanged = false;
    bool selectedOpacityChanged = false;
    double selectedOpacity = selectedAnnotation ? selectedAnnotation->style().opacity() : 1.0;

    QDialog popup;
    popup.setObjectName(QStringLiteral("toolSettingsPanel"));
    popup.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popup.setAttribute(Qt::WA_TranslucentBackground);
    popup.setStyleSheet(QStringLiteral(
        "QDialog#toolSettingsPanel { background: transparent; }"
        "QWidget#toolSettingsContents { background: #202630; color: #edf1f5; border: 1px solid #3b4653; border-radius: 13px; }"
        "QLabel { color: #c8d1db; background: transparent; }"
        "QSlider { min-height: 18px; }"
        "QSlider::groove:horizontal { height: 5px; border-radius: 3px; background: #313d4b; }"
        "QSlider::sub-page:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #49d2ff, stop:1 #6de7cf); border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #effcff, stop:1 #66d8f3); border: 1px solid rgba(255,255,255,0.9); }"
        "QSlider::handle:horizontal:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #ffffff, stop:1 #7fe5ff); }"
        "QSlider::handle:horizontal:pressed { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #dffaff, stop:1 #35c8ea); }"
        "QDoubleSpinBox, QComboBox, QToolButton { color: #edf1f5; background: #29323d; border: 1px solid #465361; border-radius: 4px; padding: 4px; }"
        "QDoubleSpinBox:focus, QComboBox:focus, QToolButton:focus { border: 1px solid #27bfd6; }"
        "QToolButton:hover { background: #35414e; }"
        "QToolButton[chartaSwatch='true'] { background: transparent; border: 2px solid transparent; border-radius: 14px; padding: 2px; }"
        "QToolButton[chartaSwatch='true']:checked { background: #35414e; border-color: #27bfd6; }"
        "QLabel#toolSettingsTitle { color: #f3f6f8; font-size: 14px; font-weight: 600; }"
        "QLabel#toolSettingsContext { color: #27bfd6; font-size: 9px; }"));
    auto *contents = new QWidget(&popup);
    contents->setObjectName(QStringLiteral("toolSettingsContents"));
    auto *panelShadow = new QGraphicsDropShadowEffect(contents);
    panelShadow->setBlurRadius(22);
    panelShadow->setColor(QColor(0, 0, 0, 150));
    panelShadow->setOffset(0, 8);
    contents->setGraphicsEffect(panelShadow);
    auto *popupLayout = new QVBoxLayout(&popup);
    popupLayout->setContentsMargins(18, 18, 18, 26);
    popupLayout->addWidget(contents);
    auto *layout = new QVBoxLayout(contents);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    auto *header = new QHBoxLayout;
    auto *toolIcon = new QLabel(contents);
    toolIcon->setPixmap(action->icon().pixmap(QSize(22, 22)));
    auto *title = new QLabel(action->text(), contents);
    title->setObjectName(QStringLiteral("toolSettingsTitle"));
    auto *context = new QLabel(i18n("TOOL SETTINGS"), contents);
    context->setObjectName(QStringLiteral("toolSettingsContext"));
    header->addWidget(toolIcon);
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(context);
    layout->addLayout(header);

    QWidget *controlParent = contents;
    QVBoxLayout *controlLayout = layout;
    QWidget *freeformControls = nullptr;
    if (isFreeformHighlight && !selectedAnnotation) {
        auto *mode = new QComboBox(contents);
        mode->addItems({i18n("Text Highlight"), i18n("Freeform Highlight")});
        mode->setCurrentIndex(freeformHighlightActive ? 1 : 0);
        layout->addWidget(mode);

        freeformControls = new QWidget(contents);
        controlLayout = new QVBoxLayout(freeformControls);
        controlLayout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(freeformControls);
        freeformControls->setEnabled(freeformHighlightActive);
        QObject::connect(mode, &QComboBox::currentIndexChanged, q, [this, mode, freeformControls](int index) {
            freeformHighlightActive = index == 1;
            freeformControls->setEnabled(freeformHighlightActive);
            aBrowseTool->setChecked(false);
            annotator->setAnnotationSelectionMode(false);
            agLastAction = drawingTools.at(3);
            selectTool(freeformHighlightActive ? 15 : 1);
        });
        controlParent = freeformControls;
    }

    auto addWidthControl = [&](double minimum, double maximum, int decimals) {
        auto *row = new QHBoxLayout;
        auto *title = new QLabel(i18n("Width"), controlParent);
        auto *currentValue = new QLabel(controlParent);
        row->addWidget(title);
        row->addStretch(1);
        row->addWidget(currentValue);
        controlLayout->addLayout(row);

        auto *slider = new ResponsiveSlider(Qt::Horizontal, controlParent);
        const int scale = qRound(1.0 / AnnotationWidthPresets::adjustmentStep);
        const int minSliderValue = qRound(std::max(minimum, AnnotationWidthPresets::minimumWidth) * scale);
        const int maxSliderValue = qRound(maximum * scale);
        slider->setRange(minSliderValue, maxSliderValue);
        slider->setSingleStep(1);
        slider->setPageStep(qMax(1, (maxSliderValue - minSliderValue) / 10));
        slider->setTracking(true);

        auto *spinBox = new QDoubleSpinBox(controlParent);
        spinBox->setRange(std::max(minimum, AnnotationWidthPresets::minimumWidth), maximum);
        spinBox->setDecimals(decimals);
        spinBox->setSingleStep(AnnotationWidthPresets::adjustmentStep);
        spinBox->setSuffix(i18n(" px"));
        spinBox->setFixedWidth(110);
        auto *controlRow = new QHBoxLayout;
        controlRow->setSpacing(8);
        controlRow->addWidget(slider, 1);
        controlRow->addWidget(spinBox);
        controlLayout->addLayout(controlRow);

        const QList<double> presetWidths = AnnotationWidthPresets::presetValuesOrDefaults(Okular::Settings::toolSettingsWidthPresets(), AnnotationWidthPresets::defaultToolPresetStrings());
        auto currentPresetWidths = std::make_shared<QList<double>>(presetWidths);
        auto selectedPreset = std::make_shared<AnnotationWidthPresets::SelectionTracker>();
        auto *presetRow = new QHBoxLayout;
        presetRow->setSpacing(6);
        struct PresetButtonEntry {
            QWidget *container;
            QToolButton *button;
            WidthPresetLineEdit *editor;
        };
        auto presetEntries = std::make_shared<QList<PresetButtonEntry>>();
        for (qsizetype index = 0; index < presetWidths.size(); ++index) {
            const double presetWidth = presetWidths.at(index);
            auto *container = new QWidget(controlParent);
            container->setFixedSize(76, 30);
            auto *stack = new QStackedLayout(container);
            stack->setContentsMargins(0, 0, 0, 0);
            auto *presetButton = new QToolButton(container);
            presetButton->setText(i18n("%1 px", presetWidth));
            presetButton->setToolTip(i18n("Select %1 px. Click again to edit this preset", presetWidth));
            presetButton->setCheckable(true);
            presetButton->setAutoRaise(true);
            presetButton->setFixedSize(container->size());
            auto *editor = new WidthPresetLineEdit(container);
            auto *validator = new QDoubleValidator(AnnotationWidthPresets::minimumWidth, AnnotationWidthPresets::maximumWidth, 3, editor);
            validator->setLocale(QLocale::c());
            validator->setNotation(QDoubleValidator::StandardNotation);
            editor->setValidator(validator);
            editor->setFixedSize(container->size());
            stack->addWidget(presetButton);
            stack->addWidget(editor);
            presetRow->addWidget(container);
            presetEntries->append({container, presetButton, editor});
        }
        controlLayout->addLayout(presetRow);

        const auto updatePresetSelection = [presetEntries, currentPresetWidths, spinBox](double width) {
            for (qsizetype index = 0; index < presetEntries->size(); ++index) {
                presetEntries->at(index).button->setChecked(qFuzzyCompare(currentPresetWidths->at(index), width));
            }
            spinBox->setValue(width);
        };

        for (qsizetype index = 0; index < presetEntries->size(); ++index) {
            const PresetButtonEntry entry = presetEntries->at(index);
            auto *stack = qobject_cast<QStackedLayout *>(entry.container->layout());
            QObject::connect(entry.button, &QToolButton::clicked, q, [this, key, kind = *kindIt, selectedAnnotation, &selectedAnnotationChanged, &selectedWidthChanged,
                                                              entry, stack, index, presetEntries, currentPresetWidths, selectedPreset, spinBox, updatePresetSelection] {
                if (selectedPreset->activate(index)) {
                    const QSignalBlocker blocker(entry.button);
                    entry.button->setChecked(true);
                    entry.editor->beginEditing(QLocale::c().toString(currentPresetWidths->at(index), 'g', 12));
                    stack->setCurrentWidget(entry.editor);
                    entry.editor->setCallbacks(
                        [entry, stack, index, currentPresetWidths, presetEntries, selectedPreset, spinBox](const QString &text) {
                            QStringList updated;
                            if (!AnnotationWidthPresets::replacePresetString(Okular::Settings::toolSettingsWidthPresets(), index, text, &updated)) {
                                entry.editor->setToolTip(i18nc("@info:tooltip", "Enter a unique width from %1 to %2 px", AnnotationWidthPresets::minimumWidth, AnnotationWidthPresets::maximumWidth));
                                return false;
                            }
                            Okular::Settings::setToolSettingsWidthPresets(updated);
                            Okular::Settings::self()->save();
                            *currentPresetWidths = AnnotationWidthPresets::presetValuesOrDefaults(updated, AnnotationWidthPresets::defaultToolPresetStrings());
                            selectedPreset->setSelectedIndex(updated.indexOf(QLocale::c().toString(currentPresetWidths->at(index), 'g', 12)));
                            for (qsizetype presetIndex = 0; presetIndex < presetEntries->size(); ++presetIndex) {
                                const double width = currentPresetWidths->at(presetIndex);
                                QToolButton *button = presetEntries->at(presetIndex).button;
                                button->setText(i18n("%1 px", width));
                                button->setToolTip(i18n("Select %1 px. Click again to edit this preset", width));
                                button->setChecked(qFuzzyCompare(width, spinBox->value()));
                            }
                            stack->setCurrentWidget(entry.button);
                            return true;
                        },
                        [entry, stack] { stack->setCurrentWidget(entry.button); });
                    return;
                }

                selectedPreset->setSelectedIndex(index);
                const double presetWidth = currentPresetWidths->at(index);
                updatePresetSelection(presetWidth);
                drawingToolSettings[key].width = presetWidth;
                if (selectedAnnotation) {
                    selectedAnnotationChanged = true;
                    selectedWidthChanged = true;
                    return;
                }
                if (kind == DrawingToolKind::Eraser) {
                    annotator->setEraserWidth(presetWidth);
                } else {
                    annotator->setDrawingToolWidthPixels(presetWidth);
                }
            });
        }

        const double initialWidth = qBound(minimum, drawingToolSettings.value(key).width, maximum);
        spinBox->setValue(initialWidth);
        slider->setValue(qRound(initialWidth * scale));
        currentValue->setText(i18n("%1 px", spinBox->value()));

        const auto updateSpinStep = [spinBox]() {
            spinBox->setSingleStep(AnnotationWidthPresets::adjustmentStep);
        };
        updateSpinStep();

        QObject::connect(slider, &QSlider::valueChanged, spinBox, [spinBox, scale](int value) { spinBox->setValue(static_cast<double>(value) / scale); });
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, slider, [slider, scale](double value) { slider->setValue(qRound(value * scale)); });
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, spinBox, updateSpinStep);
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, currentValue, [currentValue](double value) { currentValue->setText(i18n("%1 px", value)); });
        // Update annotator only when editing is finished or slider released
        QObject::connect(spinBox, &QDoubleSpinBox::editingFinished, q, [this, key, kind = *kindIt, selectedAnnotation, &selectedAnnotationChanged, &selectedWidthChanged, spinBox]() {
            double value = spinBox->value();
            drawingToolSettings[key].width = value;
            if (selectedAnnotation) {
                selectedAnnotationChanged = true;
                selectedWidthChanged = true;
                return;
            }
            if (kind == DrawingToolKind::Eraser) {
                annotator->setEraserWidth(value);
            } else {
                annotator->setDrawingToolWidthPixels(value);
            }
        });
        QObject::connect(slider, &QSlider::sliderReleased, q, [this, key, kind = *kindIt, selectedAnnotation, &selectedAnnotationChanged, &selectedWidthChanged, scale, slider]() {
            int value = slider->value();
            double widthValue = static_cast<double>(value) / scale;
            drawingToolSettings[key].width = widthValue;
            if (selectedAnnotation) {
                selectedAnnotationChanged = true;
                selectedWidthChanged = true;
                return;
            }
            if (kind == DrawingToolKind::Eraser) {
                annotator->setEraserWidth(widthValue);
            } else {
                annotator->setDrawingToolWidthPixels(widthValue);
            }
        });
        updatePresetSelection(initialWidth);
    };

    auto addColorControl = [&](const QString &label, bool fillColor) {
        auto *row = new QHBoxLayout;
        auto *title = new QLabel(label, controlParent);
        auto *swatch = new QToolButton(controlParent);
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
        controlLayout->addLayout(row);
        QList<QToolButton *> colorPresets;
        {
            const QList<QColor> presetColors = Okular::AnnotationColorHistory::recentColors();
            auto *presetRow = new QHBoxLayout;
            presetRow->setSpacing(4);
            if (presetColors.isEmpty()) {
                auto *emptyState = new QLabel(i18n("No recent colors"), controlParent);
                emptyState->setMinimumHeight(28);
                presetRow->addWidget(emptyState);
            }
            for (const QColor &preset : presetColors) {
                auto *button = new QToolButton(controlParent);
                button->setCheckable(true);
                button->setFixedSize(28, 28);
                button->setProperty("chartaSwatch", true);
                button->setAccessibleName(i18n("Set color to %1", preset.name(QColor::HexRgb)));
                button->setToolTip(button->accessibleName());
                QPixmap colorDot(20, 20);
                colorDot.fill(Qt::transparent);
                QPainter painter(&colorDot);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(QColor(QStringLiteral("#0f151b")), 1));
                painter.setBrush(preset);
                painter.drawEllipse(QRect(2, 2, 16, 16));
                button->setIcon(QIcon(colorDot));
                button->setIconSize(QSize(20, 20));
                button->setChecked(preset == color);
                colorPresets.append(button);
                presetRow->addWidget(button);
            }
            controlLayout->addLayout(presetRow);
            for (int index = 0; index < colorPresets.size(); ++index) {
                QToolButton *button = colorPresets.at(index);
                const QColor preset = presetColors.at(index);
                QObject::connect(button, &QToolButton::clicked, q, [this, button, colorPresets, preset, swatch, setSwatch, key, selectedAnnotation, &selectedAnnotationChanged, &selectedColorChanged, &selectedFillColorChanged, fillColor] {
                    for (QToolButton *presetButton : colorPresets) {
                        presetButton->setChecked(presetButton == button);
                    }
                    if (fillColor) {
                        drawingToolSettings[key].fillColor = preset;
                        if (selectedAnnotation) {
                            selectedAnnotationChanged = true;
                            selectedFillColorChanged = true;
                        } else {
                            currentInnerColor = preset;
                            annotator->setAnnotationInnerColor(preset);
                        }
                        setSwatch(preset);
                    } else {
                        drawingToolSettings[key].color = preset;
                        if (selectedAnnotation) {
                            selectedAnnotationChanged = true;
                            selectedColorChanged = true;
                        } else {
                            currentColor = preset;
                            annotator->setAnnotationColor(preset);
                        }
                        setSwatch(preset);
                    }
                });
            }
        }
        QObject::connect(swatch, &QToolButton::clicked, q, [this, swatch, colorPresets, key, label, fillColor, selectedAnnotation, &selectedAnnotationChanged, &selectedColorChanged, &selectedFillColorChanged] {
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
            for (QToolButton *presetButton : colorPresets) {
                presetButton->setChecked(false);
            }
            if (fillColor) {
                drawingToolSettings[key].fillColor = selected;
                if (selectedAnnotation) {
                    selectedAnnotationChanged = true;
                    selectedFillColorChanged = true;
                }
                QColor effectiveFillColor = selected;
                effectiveFillColor.setAlphaF(drawingToolSettings[key].fillOpacity / 100.0);
                if (!selectedAnnotation) {
                    currentInnerColor = effectiveFillColor;
                    annotator->setAnnotationInnerColor(effectiveFillColor);
                }
            } else {
                drawingToolSettings[key].color = selected;
                if (selectedAnnotation) {
                    selectedAnnotationChanged = true;
                    selectedColorChanged = true;
                } else {
                    currentColor = selected;
                    annotator->setAnnotationColor(selected);
                }
            }
            QPixmap pixmap(18, 18);
            pixmap.fill(fillColor ? drawingToolSettings.value(key).fillColor : selected);
            QPainter painter(&pixmap);
            painter.setPen(QApplication::palette().color(QPalette::Mid));
            painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
            swatch->setIcon(QIcon(pixmap));
        });
    };

    if (*kindIt == DrawingToolKind::Text) {
        auto *fontSizeRow = new QHBoxLayout;
        fontSizeRow->addWidget(new QLabel(i18n("Font size"), contents));
        fontSizeRow->addStretch(1);
        auto *fontSize = new QDoubleSpinBox(contents);
        fontSize->setRange(0.5, 200.0);
        fontSize->setDecimals(1);
        fontSize->setSingleStep(0.5);
        fontSize->setSuffix(i18n(" pt"));
        fontSize->setFixedWidth(110);
        const double initialPointSize = currentFont.pointSizeF() > 0.0 ? currentFont.pointSizeF() : 10.0;
        fontSize->setValue(initialPointSize);
        fontSizeRow->addWidget(fontSize);
        layout->addLayout(fontSizeRow);
        QObject::connect(fontSize, &QDoubleSpinBox::valueChanged, q, [this](double pointSize) {
            currentFont.setPointSizeF(pointSize);
            annotator->setAnnotationFont(currentFont);
            updateConfigActions();
        });

        auto *fontFamily = new QToolButton(contents);
        fontFamily->setText(i18n("Font family…"));
        fontFamily->setToolTip(i18n("Choose the font used for new text annotations"));
        layout->addWidget(fontFamily, 0, Qt::AlignLeft);
        QObject::connect(fontFamily, &QToolButton::clicked, q, [this, fontSize] {
            bool accepted = false;
            QWidget *fontDialogParent = drawingToolBar ? drawingToolBar->window() : QApplication::activeWindow();
            QFont selectedFont = QFontDialog::getFont(&accepted, currentFont, fontDialogParent);
            if (accepted) {
                selectedFont.setPointSizeF(fontSize->value());
                currentFont = selectedFont;
                annotator->setAnnotationFont(currentFont);
                updateConfigActions();
            }
        });
    } else {
        addWidthControl(*kindIt == DrawingToolKind::Eraser ? 1.0 : AnnotationWidthPresets::minimumWidth, *kindIt == DrawingToolKind::Eraser ? 100.0 : 200.0, 1);
    }

    if (*kindIt != DrawingToolKind::Eraser) {
        addColorControl(i18n("Color"), false);
    }
    if (selectedAnnotation && selectedAnnotation->subType() == Okular::Annotation::AInk) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(i18n("Opacity"), contents));
        auto *opacity = new QComboBox(contents);
        int currentIndex = -1;
        for (double value : opacityStandardValues) {
            opacity->addItem(GuiUtils::createOpacityIcon(value), i18n("%1%", qRound(value * 100)), value);
            if (qFuzzyCompare(value, selectedOpacity)) {
                currentIndex = opacity->count() - 1;
            }
        }
        if (currentIndex < 0) {
            opacity->insertItem(0, GuiUtils::createOpacityIcon(selectedOpacity), i18n("%1%", qRound(selectedOpacity * 100)), selectedOpacity);
            currentIndex = 0;
        }
        opacity->setCurrentIndex(currentIndex);
        row->addStretch(1);
        row->addWidget(opacity);
        layout->addLayout(row);
        QObject::connect(opacity, &QComboBox::currentIndexChanged, q, [opacity, selectedAnnotation, &selectedAnnotationChanged, &selectedOpacityChanged, &selectedOpacity](int index) {
            selectedOpacity = opacity->itemData(index).toDouble();
            selectedAnnotationChanged = true;
            selectedOpacityChanged = true;
        });
    }
    if (*kindIt == DrawingToolKind::Rectangle || *kindIt == DrawingToolKind::Ellipse || *kindIt == DrawingToolKind::Polygon) {
        addColorControl(i18n("Fill color"), true);
        auto *opacityTitle = new QLabel(i18n("Fill opacity"), contents);
        auto *opacityValue = new QLabel(contents);
        auto *opacityRow = new QHBoxLayout;
        opacityRow->addWidget(opacityTitle);
        opacityRow->addStretch(1);
        opacityRow->addWidget(opacityValue);
        layout->addLayout(opacityRow);
        auto *opacitySlider = new ResponsiveSlider(Qt::Horizontal, contents);
        opacitySlider->setRange(0, 100);
        opacitySlider->setPageStep(10);
        opacitySlider->setTracking(true);
        opacitySlider->setValue(drawingToolSettings.value(key).fillOpacity);
        layout->addWidget(opacitySlider);
        opacityValue->setText(i18n("%1%", opacitySlider->value()));
        QObject::connect(opacitySlider, &QSlider::valueChanged, opacityValue, [opacityValue](int value) { opacityValue->setText(i18n("%1%", value)); });
        QObject::connect(opacitySlider, &QSlider::sliderReleased, q, [this, key, selectedAnnotation, &selectedAnnotationChanged, &selectedFillOpacityChanged, opacitySlider]() {
            int value = opacitySlider->value();
            drawingToolSettings[key].fillOpacity = value;
            if (selectedAnnotation) {
                selectedAnnotationChanged = true;
            selectedFillOpacityChanged = true;
                return;
            }
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

    popup.setWindowTitle(i18n("%1 Settings", action->text()));
    popup.setMinimumWidth(286);
    popup.ensurePolished();
    popup.adjustSize();
    QPoint popupPosition = QCursor::pos();
    QWidget *effectiveAnchor = anchorWidget;
    if (!effectiveAnchor && drawingToolBar && drawingToolBar->isHidden() && compactToolBarView) {
        effectiveAnchor = compactToolBarView->activeToolButton();
    }
    if (effectiveAnchor) {
        popupPosition = effectiveAnchor->mapToGlobal(QPoint(effectiveAnchor->width() + 6, 0));
    } else if (drawingToolBar) {
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
    popup.move(popupPosition);
    auto *dismissFilter = new ToolSettingsPopupDismissFilter(&popup, action, qApp);
    qApp->installEventFilter(dismissFilter);
    activeToolSettingsPopup = &popup;
    activeToolSettingsAction = action;
    popup.exec();
    activeToolSettingsPopup = nullptr;
    activeToolSettingsAction = nullptr;
    if (!dismissFilter->awaitingEscapeRelease()) {
        qApp->removeEventFilter(dismissFilter);
        delete dismissFilter;
    }

    if (restoreActiveAction && !action->isChecked()) {
        const QSignalBlocker blocker(action);
        action->setChecked(true);
    }
    if (restoreActiveAction) {
        agLastAction = action;
    }

    if (document && selectedAnnotation && selectedAnnotationChanged) {
        const Okular::Page *page = document->page(pageNumber);
        QList<Okular::Annotation *> annotationsToModify;
        if (page) {
            for (const QString &name : std::as_const(selectedAnnotationNames)) {
                Okular::Annotation *annotation = page->annotation(name);
                if (annotation && document->canModifyPageAnnotation(annotation)) {
                    annotationsToModify.append(annotation);
                }
            }
        }
        if (annotationsToModify.size() > 1) {
            document->beginUndoMacro(i18n("modify annotation properties"));
        }
        for (Okular::Annotation *annotation : std::as_const(annotationsToModify)) {
            document->prepareToModifyAnnotationProperties(annotation);
            if (selectedWidthChanged) {
                annotation->style().setWidth(drawingToolSettings.value(key).width * 0.75);
            }
            if (selectedColorChanged) {
                annotation->style().setColor(drawingToolSettings.value(key).color);
            }
            if (selectedOpacityChanged && annotation->subType() == Okular::Annotation::AInk) {
                annotation->style().setOpacity(selectedOpacity);
            }
            if (annotation->subType() == Okular::Annotation::AGeom) {
                auto *geometry = static_cast<Okular::GeomAnnotation *>(annotation);
                if (selectedFillColorChanged || selectedFillOpacityChanged) {
                    QColor fill = geometry->geometricalInnerColor();
                    if (selectedFillColorChanged) {
                        const QColor selectedFill = drawingToolSettings.value(key).fillColor;
                        fill = selectedFill;
                        if (fill.isValid() && !selectedFillOpacityChanged) {
                            fill.setAlpha(geometry->geometricalInnerColor().isValid() ? geometry->geometricalInnerColor().alpha() : 0);
                        }
                    }
                    if (selectedFillOpacityChanged && fill.isValid()) {
                        fill.setAlpha(qRound(drawingToolSettings.value(key).fillOpacity * 2.55));
                    }
                    geometry->setGeometricalInnerColor(fill);
                }
            }
            document->modifyPageAnnotationProperties(pageNumber, annotation);
        }
        if (annotationsToModify.size() > 1) {
            document->endUndoMacro();
        }
    }
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
        aCustomWidth = selectActionItem(aWidth, aCustomWidth, width, widthStandardValues(), widthIcon(width), i18nc("@item:inlistbox", "Width %1", width));
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
    if (aColorCycle) {
        aColorCycle->setEnabled(isAnnotationSelected && !isStamp);
    }
    aInnerColor->setEnabled(isShape);
    aOpacity->setEnabled(isAnnotationSelected);
    aFont->setEnabled(isText);
    aConstrainRatioAndAngle->setEnabled(isStraightLine || isShape);
    aAdvancedSettings->setEnabled(isAnnotationSelected);

    if (isEraser) {
        const double width = annotator->eraserWidth();
        aCustomWidth = selectActionItem(aWidth, aCustomWidth, width, widthStandardValues(), widthIcon(width), i18nc("@item:inlistbox", "Eraser diameter %1 px", width));
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
            if (shortcutKey != Qt::Key_1 && shortcutKey != Qt::Key_2) {
                annFav->setShortcut(QKeySequence(shortcutKey));
                annFav->setShortcutContext(Qt::WidgetWithChildrenShortcut);
            }
        }
        QObject::connect(annFav, &KToggleAction::toggled, q, [this, favToolId](bool checked) {
            if (checked) {
                if (restoringTemporaryTool) {
                    aBrowseTool->setChecked(false);
                    annotator->setAnnotationSelectionMode(false);
                    annotator->setEraserMode(false);
                    annotator->selectQuickTool(favToolId);
                    selectedBuiltinTool = -1;
                    updateConfigActions();
                } else {
                    slotQuickToolSelected(favToolId);
                }
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

void AnnotationActionHandlerPrivate::rebuildColorPickerAction(KSelectAction *action, AnnotationColor colorType)
{
    while (!action->actions().isEmpty()) {
        delete action->removeAction(action->actions().constFirst());
    }

    const QList<QColor> recentColors = Okular::AnnotationColorHistory::recentColors();
    if (recentColors.isEmpty()) {
        auto *emptyState = new QAction(i18nc("@item:inlistbox", "No recent colors"), action);
        emptyState->setEnabled(false);
        action->addAction(emptyState);
    }

    const QColor selectedColor = colorType == AnnotationColor::Color ? currentColor : currentInnerColor;
    QAction *selectedAction = nullptr;
    for (const QColor &color : recentColors) {
        auto *colorAction = new QAction(GuiUtils::createColorIcon({color}, QIcon(), GuiUtils::VisualizeTransparent), color.name(QColor::HexArgb), action);
        action->addAction(colorAction);
        QObject::connect(colorAction, &QAction::triggered, q, [this, colorType, color]() { slotSetColor(colorType, color); });
        if (color == selectedColor) {
            selectedAction = colorAction;
        }
    }
    if (colorType == AnnotationColor::InnerColor) {
        auto *transparentAction = new QAction(GuiUtils::createColorIcon({Qt::transparent}, QIcon(), GuiUtils::VisualizeTransparent), i18nc("@item:inlistbox Color name", "Transparent"), action);
        action->addAction(transparentAction);
        QObject::connect(transparentAction, &QAction::triggered, q, [this, colorType]() { slotSetColor(colorType, Qt::transparent); });
        if (selectedColor == Qt::transparent) {
            selectedAction = transparentAction;
        }
    }

    auto *customColorAction = new QAction(QIcon::fromTheme(QStringLiteral("color-picker")), i18nc("@item:inlistbox", "Custom Color…"), action);
    action->addAction(customColorAction);
    QObject::connect(customColorAction, &QAction::triggered, q, [this, colorType]() { slotSetColor(colorType); });
    if (selectedAction) {
        action->setCurrentAction(selectedAction);
    }
}

KSelectAction *AnnotationActionHandlerPrivate::colorPickerAction(AnnotationColor colorType)
{
    const QString actionText = colorType == AnnotationColor::InnerColor ? i18nc("@action:intoolbar Current annotation config option", "Fill Color") : i18nc("@action:intoolbar Current annotation config option", "Color");
    auto *colorPicker = new KSelectAction(QIcon(), actionText, q);
    colorPicker->setToolBarMode(KSelectAction::MenuMode);
    rebuildColorPickerAction(colorPicker, colorType);
    return colorPicker;
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
    annotator->selectBuiltinTool(toolId);
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
    d->temporaryToolHoldTimer = new QTimer(this);
    d->temporaryToolHoldTimer->setSingleShot(true);
    d->temporaryToolHoldTimer->setInterval(temporaryToolHoldThresholdMs);
    connect(d->temporaryToolHoldTimer, &QTimer::timeout, this, [this] {
        if (d->heldTemporaryToolKeys.isEmpty()) {
            return;
        }
        if (const auto tool = temporaryToolForKey(d->heldTemporaryToolKeys.constLast())) {
            switchTemporaryTool(*tool);
        }
    });
    qApp->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *oldWidget, QWidget *newWidget) {
        if (d->heldTemporaryToolKeys.isEmpty() || oldWidget == newWidget) {
            return;
        }
        QTimer::singleShot(0, this, [this, oldWidget] {
            if (!d->heldTemporaryToolKeys.isEmpty() && QApplication::focusWidget() != oldWidget) {
                cancelTemporaryToolSwitch();
            }
        });
    });

    // toolbar visibility actions
    d->aToolBarVisibility = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18n("&Annotations"), this);
    d->aHideToolBar = new QAction(QIcon::fromTheme(QStringLiteral("dialog-close")), i18nc("@action:intoolbar Hide the drawing toolbar", "Hide drawing tools"), this);
    d->aShowToolBar = new QAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18nc("@action:intoolbar Show the drawing toolbar", "Show drawing tools"), this);
    d->aCompactMode = new KToggleAction(QIcon::fromTheme(QStringLiteral("charta"), QIcon(QStringLiteral(":/branding/charta.svg"))), i18nc("@action:intoolbar Switch drawing toolbar between expanded and compact presentation", "Compact"), this);
    d->aCompactMode->setChecked(Okular::Settings::drawingToolBarCompact());

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
    d->aLassoSelectTool = new KToggleAction(QIcon::fromTheme(QStringLiteral("edit-select-lasso")), i18nc("@action:intoolbar Drawing tool", "Lasso Select"), this);
    d->aSelectTools = new ToggleActionMenu(i18nc("@action:intoolbar Drawing tool", "Selection Tool"), this);
    d->aSelectTools->setPopupMode(QToolButton::MenuButtonPopup);
    d->aSelectTools->addAction(d->aSelectTool);
    d->aSelectTools->addAction(d->aLassoSelectTool);
    d->aSelectTools->setDefaultAction(d->aSelectTool);
    connect(d->aSelectTools->menu(), &QMenu::triggered, d->aSelectTools, &ToggleActionMenu::setDefaultAction);
    d->aEraserTool = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-eraser")), i18nc("@action:intoolbar Drawing tool", "Eraser"), this);
    d->aBrowseTool = ac->action(QStringLiteral("mouse_drag"));
    Q_ASSERT(d->aBrowseTool);
    d->aMouseSelectTools = ac->action(QStringLiteral("mouse_selecttools"));
    Q_ASSERT(d->aMouseSelectTools);
    d->aDrawingBrowse = new KToggleAction(QIcon::fromTheme(QStringLiteral("transform-browse")), i18nc("@action:intoolbar Drawing tool", "Pan"), this);
    d->aDrawingBrowse->setChecked(d->aBrowseTool->isChecked());
    KToggleAction *aFreehandLine = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-freehand")), i18nc("@action:intoolbar Drawing tool", "Pen / Freehand"), this);
    // Geometrical shapes actions
    KToggleAction *aStraightLine = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-line")), i18nc("@action:intoolbar Annotation tool", "Straight line"), this);
    KToggleAction *aArrow = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-arrow")), i18nc("@action:intoolbar Annotation tool", "Arrow"), this);
    KToggleAction *aRectangle = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-rectangle")), i18nc("@action:intoolbar Annotation tool", "Rectangle"), this);
    KToggleAction *aEllipse = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-ellipse")), i18nc("@action:intoolbar Annotation tool", "Ellipse"), this);
    KToggleAction *aPolygon = new KToggleAction(QIcon::fromTheme(QStringLiteral("draw-polyline")), i18nc("@action:intoolbar Annotation tool", "Polygon"), this);
    d->drawingToolKinds.insert(aFreehandLine, AnnotationActionHandlerPrivate::DrawingToolKind::Pen);
    d->drawingToolKinds.insert(aHighlighter, AnnotationActionHandlerPrivate::DrawingToolKind::Pen);
    d->drawingToolIds.insert(aHighlighter, 15);
    d->drawingToolKinds.insert(aTypewriter, AnnotationActionHandlerPrivate::DrawingToolKind::Text);
    d->drawingToolIds.insert(aTypewriter, 5);
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
    d->agTools->addAction(d->aLassoSelectTool);
    d->agTools->addAction(d->aEraserTool);
    const auto selectionToolToggled = [this](bool checked, bool lassoMode, QAction *action) {
        if (!checked) {
            return;
        }
        d->aSelectTools->setDefaultAction(action);
        d->aBrowseTool->setChecked(false);
        d->annotator->setEraserMode(false);
        d->annotator->selectBuiltinTool(-1);
        d->annotator->setAnnotationSelectionMode(true, lassoMode);
        if (d->restoringTemporaryTool) {
            d->updateConfigActions();
            return;
        }
        d->selectModeSwitchingToBrowse = true;
        d->aBrowseTool->trigger();
        d->selectModeSwitchingToBrowse = false;
        if (d->aBrowseTool->isCheckable()) {
            d->aBrowseTool->setChecked(false);
        }
    };
    connect(d->aSelectTool, &QAction::toggled, this, [this, selectionToolToggled](bool checked) { selectionToolToggled(checked, false, d->aSelectTool); });
    connect(d->aLassoSelectTool, &QAction::toggled, this, [this, selectionToolToggled](bool checked) { selectionToolToggled(checked, true, d->aLassoSelectTool); });
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
        if (!d->changingTemporaryTool && !d->restoringTemporaryTool && (d->temporaryToolAction || !d->heldTemporaryToolKeys.isEmpty())) {
            abandonTemporaryToolSwitch();
        }
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
        if (!d->changingTemporaryTool && !d->restoringTemporaryTool && (d->temporaryToolAction || !d->heldTemporaryToolKeys.isEmpty())) {
            abandonTemporaryToolSwitch();
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

    QAction *aMouseTextSelect = ac->action(QStringLiteral("mouse_textselect"));
    Q_ASSERT(aMouseTextSelect);
    d->drawingTools = {d->aDrawingBrowse, d->aSelectTools, aFreehandLine, aHighlighter, d->aEraserTool, aStraightLine, aArrow, aRectangle, aEllipse, aPolygon, aMouseTextSelect, aTypewriter};

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
    connect(Okular::Settings::self(), &Okular::Settings::recentAnnotationColorsChanged, this, [this] {
        d->rebuildColorPickerAction(d->aColor, AnnotationActionHandlerPrivate::AnnotationColor::Color);
        d->rebuildColorPickerAction(d->aInnerColor, AnnotationActionHandlerPrivate::AnnotationColor::InnerColor);
    }, Qt::QueuedConnection);
    d->aFont = new QAction(QIcon::fromTheme(QStringLiteral("font-face")), i18nc("@action:intoolbar Current annotation config option", "Font"), this);
    d->aAdvancedSettings = new QAction(QIcon::fromTheme(QStringLiteral("settings-configure")), i18nc("@action:intoolbar Current annotation advanced settings", "Annotation Settings"), this);

    // Color cycle action
    d->aColorCycle = new QAction(QIcon::fromTheme(QStringLiteral("format-stroke-color")), i18nc("@action:intoolbar Cycle annotation color", "Cycle Annotation Color"), this);
    d->aColorCycle->setEnabled(false);
    connect(d->aColorCycle, &QAction::triggered, this, &AnnotationActionHandler::slotColorCycle);

    // Width list
    d->aWidth = new KSelectAction(QIcon::fromTheme(QStringLiteral("edit-line-width")), i18nc("@action:intoolbar Current annotation config option", "Line width"), this);
    d->aWidth->setToolBarMode(KSelectAction::MenuMode);
    d->populateWidthActions();
    connect(Okular::Settings::self(), &Okular::Settings::annotationWidthMenuPresetsChanged, this, [this] {
        d->populateWidthActions();
        d->parseTool(d->selectedBuiltinTool);
    });

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
    connect(d->agTools, &QActionGroup::triggered, this, [this, aHighlighter](QAction *action) {
        if (d->restoringTemporaryTool && action == d->temporaryToolPreviousAction) {
            d->pendingMousePopupAction = nullptr;
            d->agLastAction = action;
            return;
        }
        if (!d->changingTemporaryTool && !d->restoringTemporaryTool && (d->temporaryToolAction || !d->heldTemporaryToolKeys.isEmpty())) {
            abandonTemporaryToolSwitch();
        }
        if (action == d->agLastAction) {
            const bool mouseRepeat = d->pendingMousePopupAction == action;
            d->pendingMousePopupAction = nullptr;
            if ((action == d->aSelectTool || action == d->aLassoSelectTool) && !d->deselectingTools && !d->restoringTemporaryTool) {
                action->setChecked(true);
            } else if (d->deselectingTools) {
                d->agLastAction = nullptr;
                d->agTools->checkedAction()->setChecked(false);
                d->selectTool(-1);
            } else if (d->restoringTemporaryTool) {
                return;
            } else if (d->activeToolSettingsAction == action && d->activeToolSettingsPopup) {
                d->activeToolSettingsPopup->reject();
                return;
            } else if ((mouseRepeat || d->drawingToolKinds.contains(action)) && d->showToolSettings(action)) {
                return;
            } else if (!d->drawingToolKinds.contains(action)) {
                d->agLastAction = nullptr;
                d->agTools->checkedAction()->setChecked(false);
                d->selectTool(-1);
            }
        } else {
            d->pendingMousePopupAction = nullptr;
            d->agLastAction = action;
            if (action == aHighlighter) {
                d->freeformHighlightActive = false;
            }
            // Show the annotation toolbar whenever builtin tool actions are triggered (e.g using shortcuts)
            if (!d->isQuickToolAction(action)) {
                d->aToolBarVisibility->setChecked(true);
            }
        }
    });

    auto *drawingToolCategory = new KActionCategory(i18n("Drawing Toolbar"), ac);
    drawingToolCategory->addAction(QStringLiteral("drawing_browse"), d->aDrawingBrowse);
    drawingToolCategory->addAction(QStringLiteral("drawing_select"), d->aSelectTool);
    drawingToolCategory->addAction(QStringLiteral("drawing_lasso_select"), d->aLassoSelectTool);
    drawingToolCategory->addAction(QStringLiteral("annotation_freehand_line"), aFreehandLine);
    drawingToolCategory->addAction(QStringLiteral("annotation_highlight_tools"), aHighlighter);
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
    ac->addAction(QStringLiteral("annotation_cycle_color"), d->aColorCycle);

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
    ac->setDefaultShortcut(d->aLassoSelectTool, QKeySequence(Qt::Key_B));
    ac->setDefaultShortcut(d->aEraserTool, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_E));
    ac->setDefaultShortcut(aStraightLine, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_L));
    ac->setDefaultShortcut(aEllipse, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_O));
    ac->setDefaultShortcut(aPolygon, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_P));
    ac->setDefaultShortcut(d->aAddToQuickTools, QKeySequence((Qt::CTRL | Qt::SHIFT) | Qt::Key_B));
    ac->setDefaultShortcut(d->aColorCycle, QKeySequence(Qt::Key_C));
    d->updateConfigActions();

    connect(Okular::Settings::self(), &Okular::Settings::primaryAnnotationToolBarChanged, this, &AnnotationActionHandler::setupAnnotationToolBarVisibilityAction);
}

AnnotationActionHandler::~AnnotationActionHandler()
{
    // delete the private data storage structure
    delete d;
}

bool AnnotationActionHandler::showAnnotationSettings(Okular::Document *document, const QList<Okular::Annotation *> &annotations, int pageNumber, QWidget *anchorWidget)
{
    if (!document || annotations.isEmpty() || pageNumber < 0) {
        return false;
    }

    const auto toolIdForAnnotation = [](const Okular::Annotation *annotation) {
        switch (annotation->subType()) {
        case Okular::Annotation::AInk:
            return 15;
        case Okular::Annotation::AGeom: {
            const auto *geometry = static_cast<const Okular::GeomAnnotation *>(annotation);
            return geometry->geometricalType() == Okular::GeomAnnotation::InscribedCircle ? 12 : 11;
        }
        case Okular::Annotation::ALine: {
            const auto *line = static_cast<const Okular::LineAnnotation *>(annotation);
            if (line->linePoints().size() > 2) {
                return 13;
            }
            return line->lineIntent() == Okular::LineAnnotation::Arrow ? 9 : 10;
        }
        default:
            return -1;
        }
    };

    const int toolId = toolIdForAnnotation(annotations.constFirst());
    if (toolId < 0) {
        return false;
    }
    for (Okular::Annotation *annotation : annotations) {
        if (toolIdForAnnotation(annotation) != toolId || !document->canModifyPageAnnotation(annotation)) {
            return false;
        }
    }

    for (auto it = d->drawingToolIds.cbegin(); it != d->drawingToolIds.cend(); ++it) {
        if (it.value() == toolId) {
            return d->showToolSettings(it.key(), document, annotations, pageNumber, anchorWidget);
        }
    }
    return false;
}

bool AnnotationActionHandler::eventFilter(QObject *watched, QEvent *event)
{
    auto *pageView = qobject_cast<PageView *>(d->annotator->parent());
    auto *eventWidget = qobject_cast<QWidget *>(watched);
    const auto isInPageView = [pageView](QWidget *widget) { return pageView && widget && (widget == pageView || pageView->isAncestorOf(widget)); };
    if (!d->heldTemporaryToolKeys.isEmpty()
        && (event->type() == QEvent::ApplicationDeactivate || (pageView && event->type() == QEvent::WindowDeactivate && eventWidget == pageView->window()))) {
        cancelTemporaryToolSwitch();
    }

    const bool heldKeyRelease = event->type() == QEvent::KeyRelease && d->heldTemporaryToolKeys.contains(static_cast<QKeyEvent *>(event)->key());
    if ((event->type() == QEvent::KeyRelease || event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) && (isInPageView(eventWidget) || heldKeyRelease)) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();
        const auto activeAction = [this] {
            QAction *action = d->agTools->checkedAction();
            return action ? action : (d->aDrawingBrowse->isChecked() ? static_cast<QAction *>(d->aDrawingBrowse) : nullptr);
        };
        const auto actionForTool = [this](TemporaryTool tool) -> QAction * {
            return tool == TemporaryTool::Eraser ? static_cast<QAction *>(d->aEraserTool) : d->aDrawingBrowse;
        };

        if (const auto tool = temporaryToolForKey(key)) {
            const bool held = d->heldTemporaryToolKeys.contains(key);
            if (event->type() == QEvent::KeyRelease && held) {
                keyEvent->accept();
                if (keyEvent->isAutoRepeat()) {
                    return true;
                }
                d->heldTemporaryToolKeys.removeAll(key);
                if (d->heldTemporaryToolKeys.isEmpty() && d->temporaryToolAction) {
                    endTemporaryTool();
                } else if (d->heldTemporaryToolKeys.isEmpty()) {
                    d->temporaryToolHoldTimer->stop();
                    if (const auto persistentTool = temporaryToolForKey(key)) {
                        actionForTool(*persistentTool)->trigger();
                    }
                } else if (const auto activeTool = temporaryToolForKey(d->heldTemporaryToolKeys.constLast())) {
                    switchTemporaryTool(*activeTool);
                }
                return true;
            }

            if ((event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) && keyEvent->modifiers() == Qt::NoModifier) {
                QWidget *focusWidget = QApplication::focusWidget();
                const bool textInputFocused = qobject_cast<QLineEdit *>(focusWidget) || qobject_cast<QTextEdit *>(focusWidget) || qobject_cast<QPlainTextEdit *>(focusWidget)
                    || qobject_cast<QAbstractSpinBox *>(focusWidget) || (qobject_cast<QComboBox *>(focusWidget) && qobject_cast<QComboBox *>(focusWidget)->isEditable());
                const bool canSwitch = !textInputFocused && (d->temporaryToolAction || (activeAction() && activeAction() != actionForTool(*tool)));

                if (canSwitch && event->type() == QEvent::ShortcutOverride && (!keyEvent->isAutoRepeat() || held)) {
                    keyEvent->accept();
                    return false;
                }
                if (canSwitch && event->type() == QEvent::KeyPress) {
                    if (held) {
                        keyEvent->accept();
                        return true;
                    }
                    if (keyEvent->isAutoRepeat()) {
                        return false;
                    }
                    const bool overlapping = !d->heldTemporaryToolKeys.isEmpty();
                    d->heldTemporaryToolKeys.append(key);
                    if (overlapping) {
                        d->temporaryToolHoldTimer->stop();
                        if (switchTemporaryTool(*tool)) {
                            keyEvent->accept();
                            return true;
                        }
                    } else {
                        d->temporaryToolHoldTimer->start();
                        keyEvent->accept();
                        return true;
                    }
                    d->heldTemporaryToolKeys.removeAll(key);
                }
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        const auto action = d->settingsButtonActions.constFind(watched);
        if (action != d->settingsButtonActions.cend() && d->agTools->checkedAction() == *action) {
            d->pendingMousePopupAction = *action;
        }
    }
    return QObject::eventFilter(watched, event);
}

void AnnotationActionHandler::cancelTemporaryToolSwitch()
{
    d->temporaryToolHoldTimer->stop();
    d->heldTemporaryToolKeys.clear();
    endTemporaryTool();
}

void AnnotationActionHandler::abandonTemporaryToolSwitch()
{
    d->temporaryToolHoldTimer->stop();
    d->heldTemporaryToolKeys.clear();
    d->temporaryToolAction = nullptr;
    d->temporaryToolPreviousAction = nullptr;
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
    KToolBar *drawingToolBar = d->drawingToolBar;
    const bool createDrawingToolBar = !drawingToolBar;
    if (!drawingToolBar) {
        PageView *pageView = mw->findChild<PageView *>();
        if (!pageView) {
            return;
        }
        auto *floatingHost = new QWidget(pageView);
        floatingHost->setObjectName(QStringLiteral("drawingToolBarHost"));
        floatingHost->setAttribute(Qt::WA_StyledBackground, false);
        floatingHost->setContentsMargins(0, 0, 0, 0);
        auto *floatingLayout = new QVBoxLayout(floatingHost);
        floatingLayout->setContentsMargins(0, 0, 0, 0);
        floatingLayout->setSpacing(0);

        auto *island = new DrawingToolBarIsland(pageView, pageView->viewport(), floatingHost);
        island->setAnchor(DrawingToolBarIsland::Anchor::Right);
        drawingToolBar = island;
        drawingToolBar->setWindowTitle(i18n("Drawing Tools"));
        d->drawingToolBar = drawingToolBar;

        d->compactToolBarView = new CompactToolBarView(island, d->aCompactMode, floatingHost);
        island->setCompactView(d->compactToolBarView);
        d->compactToolBarView->hide();

        floatingLayout->addWidget(island);
        floatingLayout->addWidget(d->compactToolBarView);
        floatingHost->adjustSize();
        floatingHost->hide();
        d->compactToolBarView->setVisible(false);
    }
    auto *island = static_cast<DrawingToolBarIsland *>(drawingToolBar);
    drawingToolBar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #202630; border: 1px solid #3b4653; border-radius: 10px; spacing: 1px; padding: 4px; margin: 0; }"
        "QToolButton { color: #aab6c2; background: transparent; border: 1px solid transparent; border-radius: 6px; min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; padding: 2px; margin: 0; }"
        "QToolButton:hover { color: #f2f5f8; background: #303a46; }"
        "QToolButton[chartaRole='selection']:hover, QToolButton[chartaRole='selection']:open { color: #aab6c2; background: transparent; }"
        "QToolButton[chartaRole='selection'] { padding-right: 6px; }"
        "QToolButton[chartaRole='selection']::menu-button { width: 6px; right: 3px; border: none; background: transparent; }"
        "QToolButton[chartaRole='selection']::menu-button:hover { background: transparent; }"
        "QToolButton[chartaRole='selection']::menu-arrow { width: 5px; height: 5px; }"
        "QToolButton[chartaRole='selection']:hover::menu-arrow, QToolButton[chartaRole='selection']:open::menu-arrow { color: #ffffff; }"
        "QToolButton:checked { color: #ffffff; background: #16414c; border-color: #27bfd6; }"
        "QToolButton:focus { border-color: #27bfd6; }"
        "QToolButton[chartaRole='erase']:checked { background: #393541; border-color: #b59ad8; }"
        "QToolButton[chartaRole='shape']:checked { background: #403725; border-color: #e6b45d; }"
        "QToolButton[chartaRole='text']:checked { background: #402d38; border-color: #e58ca9; }"
        "QToolBar::separator { height: 1px; margin: 3px 6px; background: #45515e; }"));
    d->drawingToolBar = drawingToolBar;
    if (drawingToolBar->actions().isEmpty()) {
        for (qsizetype index = 0; index < d->drawingTools.size(); ++index) {
            if (index == 2 || index == 5 || index == 10) {
                drawingToolBar->addSeparator();
            }
            if (QAction *action = d->drawingTools.at(index)) {
                drawingToolBar->addAction(action);
            }
        }
        drawingToolBar->addAction(d->aCompactMode);
    }
    if (d->compactToolBarView) {
        QAction *checkedAction = d->agTools->checkedAction();
        d->compactToolBarView->setActiveAction(checkedAction ? checkedAction : d->drawingTools.constFirst());
        connect(d->agTools, &QActionGroup::triggered, this, [this](QAction *action) {
            if (d->compactToolBarView) {
                d->compactToolBarView->setActiveAction(action);
            }
        });
        connect(d->aDrawingBrowse, &QAction::toggled, this, [this](bool checked) {
            if (checked && d->compactToolBarView) {
                d->compactToolBarView->setActiveAction(d->aDrawingBrowse);
            }
        });
        connect(d->aCompactMode, &QAction::toggled, this, [this, island](bool compact) {
            Okular::Settings::setDrawingToolBarCompact(compact);
            Okular::Settings::self()->save();
            island->setCompactPresentation(compact);
            if (d->compactToolBarView) {
                d->compactToolBarView->setVisible(compact);
            }
        });
    }
    island->setCompactPresentation(d->aCompactMode->isChecked());
    if (auto *selectionButton = qobject_cast<QToolButton *>(drawingToolBar->widgetForAction(d->aSelectTools));
        selectionButton && !selectionButton->property("selectionRepeatPopupConnected").toBool()) {
        selectionButton->setProperty("selectionRepeatPopupConnected", true);
        connect(selectionButton, &QToolButton::pressed, this, [this] {
            d->selectionToolWasActiveOnPress = d->agTools->checkedAction() == d->aSelectTools->defaultAction();
        });
        connect(selectionButton, &QToolButton::clicked, this, [this, selectionButton] {
            if (d->selectionToolWasActiveOnPress) {
                selectionButton->showMenu();
            }
            d->selectionToolWasActiveOnPress = false;
        });
        QMenu *selectionMenu = d->aSelectTools->menu();
        connect(selectionMenu, &QMenu::aboutToShow, this, [selectionMenu] {
            QTimer::singleShot(0, selectionMenu, [selectionMenu] {
                selectionMenu->move(selectionMenu->pos() - QPoint(8, 0));
            });
        });
    }
    for (qsizetype index = 0; index < d->drawingTools.size(); ++index) {
        const QString role = index == 1 ? QStringLiteral("selection") : (index == 3 ? QStringLiteral("highlight") : (index == 4 ? QStringLiteral("erase") : (index >= 5 && index <= 9 ? QStringLiteral("shape") : (index == 11 ? QStringLiteral("text") : QStringLiteral("navigation")))));
        if (QWidget *toolButton = drawingToolBar->widgetForAction(d->drawingTools.at(index))) {
            toolButton->setProperty("chartaRole", role);
            toolButton->style()->unpolish(toolButton);
            toolButton->style()->polish(toolButton);
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
        connect(d->aShowToolBar, &QAction::triggered, island, [island] { island->setRequestedVisible(true); });
        connect(d->aHideToolBar, &QAction::triggered, island, [island] { island->setRequestedVisible(false); });
        connect(d->aToolBarVisibility, &QAction::toggled, island, [island](bool visible) { island->setRequestedVisible(visible); });
        connect(drawingToolBar, &QToolBar::visibilityChanged, this, &AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged, Qt::UniqueConnection);
        connect(drawingToolBar, &QToolBar::visibilityChanged, this, [this, island](bool visible) {
            if (d->aCompactMode->isChecked()) {
                return;
            }
            if (!visible && (island->window()->isFullScreen() || !island->isHidden())) {
                return;
            }
            d->aToolBarVisibility->setChecked(visible);
        });
        island->setRequestedVisible(true);
    }

    KToolBar *annotationToolBar = mw->toolBar(QStringLiteral("annotationToolBar"));
    KToolBar *quickAnnotationToolBar = mw->toolBar(QStringLiteral("quickAnnotationToolBar"));
    if (KToolBar *mainToolBar = mw->toolBar(QStringLiteral("mainToolBar"))) {
        mainToolBar->removeAction(d->aQuickTools);
        mainToolBar->removeAction(d->aBrowseTool);
        mainToolBar->removeAction(d->aMouseSelectTools);

        QAction *previousAction = nullptr;
        for (QAction *action : mainToolBar->actions()) {
            if (action->isSeparator() && (!previousAction || previousAction->isSeparator())) {
                mainToolBar->removeAction(action);
                continue;
            }
            previousAction = action;
        }
        const QList<QAction *> toolbarActions = mainToolBar->actions();
        if (!toolbarActions.isEmpty() && toolbarActions.constLast()->isSeparator()) {
            mainToolBar->removeAction(toolbarActions.constLast());
        }
        mainToolBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        mainToolBar->adjustSize();
        mainToolBar->setStyleSheet(QStringLiteral(
            "QToolBar { background: #202630; border: 0; border-bottom: 1px solid #3b4653; spacing: 1px; padding: 1px 5px; }"
            "QToolButton { color: #b9c3ce; background: transparent; border: 1px solid transparent; border-radius: 6px; min-width: 32px; min-height: 28px; padding: 2px 4px; }"
            "QToolButton:hover { background: #303a46; }"
            "QToolButton:checked { background: #16414c; border-color: #27bfd6; }"
            "QToolButton:focus { border-color: #27bfd6; }"
            "QToolBar::separator { width: 1px; margin: 4px 5px; background: #45515e; }"));
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

    d->aToolBarVisibility->setChecked(island->requestedVisible());
    d->aShowToolBar->setEnabled(!island->requestedVisible());
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

bool AnnotationActionHandler::beginTemporaryTool(TemporaryTool tool)
{
    if (d->temporaryToolAction) {
        return false;
    }

    QAction *previousAction = d->agTools->checkedAction();
    if (!previousAction && d->aDrawingBrowse->isChecked()) {
        previousAction = d->aDrawingBrowse;
    }
    if (!previousAction) {
        return false;
    }

    QAction *temporaryAction = tool == TemporaryTool::Select ? d->aSelectTools->defaultAction()
        : (tool == TemporaryTool::Eraser ? static_cast<QAction *>(d->aEraserTool) : d->aDrawingBrowse);
    if (previousAction == temporaryAction) {
        return false;
    }

    d->temporaryToolPreviousAction = previousAction;
    d->temporaryToolAction = temporaryAction;
    d->changingTemporaryTool = true;
    temporaryAction->trigger();
    d->changingTemporaryTool = false;
    if (!temporaryAction->isChecked()) {
        d->temporaryToolPreviousAction = nullptr;
        d->temporaryToolAction = nullptr;
        return false;
    }
    return true;
}

bool AnnotationActionHandler::switchTemporaryTool(TemporaryTool tool)
{
    if (!d->temporaryToolAction) {
        return beginTemporaryTool(tool);
    }

    QAction *temporaryAction = tool == TemporaryTool::Select ? d->aSelectTools->defaultAction()
        : (tool == TemporaryTool::Eraser ? static_cast<QAction *>(d->aEraserTool) : d->aDrawingBrowse);
    if (temporaryAction == d->temporaryToolAction) {
        return true;
    }
    if (!temporaryAction || !temporaryAction->isEnabled()) {
        return false;
    }

    QAction *previousTemporaryAction = d->temporaryToolAction;
    d->temporaryToolAction = temporaryAction;
    d->changingTemporaryTool = true;
    temporaryAction->trigger();
    d->changingTemporaryTool = false;
    if (!temporaryAction->isChecked()) {
        d->temporaryToolAction = previousTemporaryAction;
        return false;
    }
    return true;
}

void AnnotationActionHandler::endTemporaryTool()
{
    if (!d->temporaryToolAction) {
        return;
    }

    QAction *previousAction = d->temporaryToolPreviousAction;
    d->temporaryToolAction = nullptr;
    if (previousAction) {
        d->restoringTemporaryTool = true;
        d->changingTemporaryTool = true;
        previousAction->trigger();
        d->changingTemporaryTool = false;
        d->restoringTemporaryTool = false;
    }
    d->temporaryToolPreviousAction = nullptr;
    d->updateConfigActions();
}

void AnnotationActionHandler::slotAnnotationToolBarVisibilityChanged(bool visible)
{
    if (d->aCompactMode && d->aCompactMode->isChecked()) {
        return;
    }
    if (!visible) {
        auto *island = static_cast<DrawingToolBarIsland *>(d->drawingToolBar);
        if (island->window()->isFullScreen() && island->requestedVisible()) {
            return;
        }
    }
    d->aShowToolBar->setEnabled(!visible);
    if (!visible && !d->isQuickToolAction(d->agTools->checkedAction())) {
        deselectAllAnnotationActions();
    }
}

void AnnotationActionHandler::slotColorCycle()
{
    if (!d->aColor || !d->aColor->isEnabled()) {
        return;
    }

    QWidget *focusWidget = QApplication::focusWidget();
    if (focusWidget && (qobject_cast<QLineEdit *>(focusWidget) || qobject_cast<QTextEdit *>(focusWidget) || qobject_cast<QPlainTextEdit *>(focusWidget))) {
        return;
    }

    const QList<QColor> colorHistory = Okular::AnnotationColorHistory::recentColors();
    if (colorHistory.size() <= 1) {
        return;
    }

    const QColor nextColor = Okular::AnnotationColorHistory::nextCycledColor(colorHistory, d->currentColor);
    if (nextColor.isValid() && !Okular::AnnotationColorHistory::colorsEqual(nextColor, d->currentColor)) {
        d->annotator->setAnnotationColor(nextColor);
    }
}

#include "moc_annotationactionhandler.cpp"
