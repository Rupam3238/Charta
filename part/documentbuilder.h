/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)

    Document Builder: the full-window page that hosts the grid of imported pages.

    It is deliberately split into three small pieces:
      * DocumentBuilderDelegate paints one tile (thumbnail, caption, page number),
      * DocumentBuilderView owns selection, drag & drop reordering and the grid geometry,
      * DocumentBuilderWidget is the page shown inside the part, with its own toolbar.

    Export is the only thing here that writes to disk, and it does so through
    `Okular::DocumentBuilderExport` so that the QPDF link and the atomic-output contract stay in one
    unit that has no UI at all.
*/

#ifndef DOCUMENTBUILDER_H
#define DOCUMENTBUILDER_H

#include <QList>
#include <QListView>
#include <QPixmap>
#include <QPointer>
#include <QSize>
#include <QStyledItemDelegate>
#include <QUuid>
#include <QWidget>

#include "okularpart_export.h"

class DocumentBuilderModel;
class KMessageWidget;
class QLabel;
class QMenu;
class QProgressDialog;
class QTimer;
class QToolButton;

class OKULARPART_EXPORT DocumentBuilderDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit DocumentBuilderDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /// Where the little delete badge sits inside a tile, so the view can hit-test it.
    static QRect deleteBadgeRect(const QRect &itemRect);

    /// Hides the file-name strip under a card and gives the room back to the thumbnail (Configure).
    void setShowFileNames(bool show);
    bool showFileNames() const
    {
        return m_showFileNames;
    }

    /// Cell size chosen by the view; used as the size hint so the grid and the painting agree.
    void setCellSize(const QSize &size);
    QSize cellSize() const
    {
        return m_cellSize;
    }

private:
    bool m_showFileNames = true;
    QSize m_cellSize;
};

class OKULARPART_EXPORT DocumentBuilderView : public QListView
{
    Q_OBJECT

public:
    explicit DocumentBuilderView(QWidget *parent = nullptr);

    QList<QUuid> selectedEntryIds() const;
    QList<int> selectedEntryRows() const;

    /// Replaces the selection with the entries whose ids are given (rows are looked up again, so
    /// the selection survives a reorder).
    void selectEntryIds(const QList<QUuid> &ids);

    /// Number of cards per row; 0 means "as many as fit at a readable size".
    void setColumnCount(int columns);
    int columnCount() const
    {
        return m_columns;
    }

    /// Multiplier for the derived card size (See DocumentBuilderConfig::thumbnailScale).
    void setTileScale(qreal scale);
    qreal tileScale() const
    {
        return m_tileScale;
    }

    /// Turns the caption strip on/off; asks the delegate, then recomputes the cell height.
    void setShowFileNames(bool show);

    /// Requests previews for the cards that are on screen now and queues the rest in the
    /// background. Cheap to call: the work is coalesced into one pass of the event loop.
    void scheduleThumbnailFill();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

Q_SIGNALS:
    void addTileActivated();
    void deleteRequested(const QList<int> &rows);
    /// Alt+Up / Alt+Down: move the selected entries one slot up or down.
    void moveSelectionRequested(int delta);
    void contextMenuRequested(const QPoint &globalPos);
    void filesDropped(const QStringList &paths);
    /// URLs that are not local files were dropped; the widget explains why they are ignored.
    void remoteFilesDropped(const QStringList &urls);

private:
    void updateGridSize();
    void fillThumbnails();
    /// Starts the hand-rolled card drag once the mouse has moved past the drag distance.
    void beginEntryDrag();
    /// Follows the cursor and keeps the insertion bar under it.
    void updateEntryDrag(const QPoint &pos);
    /// Ends the drag, applying the reorder when @p drop is true.
    void finishEntryDrag(bool drop);
    void cancelEntryDrag();
    void autoScrollDuringDrag();

    /// Insertion row for a drop at @p pos: the card it lands before, in reading order.
    int dropRowFor(const QPoint &pos) const;
    /// Thin bar drawn between two cards to show where the drop goes.
    QRect dropIndicatorRect() const;
    QPixmap dragPixmap(const QList<int> &rows) const;

    int m_columns = 0;
    qreal m_tileScale = 1.0;
    bool m_inGridUpdate = false;
    QTimer *m_fillTimer = nullptr;
    /// Rows that still needed a preview at the start of the previous fill pass. Used to stop the
    /// self-rescheduling chain when a pass no longer makes progress (e.g. an unreadable image).
    int m_pendingAtLastFill = -1;
    int m_dropRow = -1;

    // Reordering is a manual drag rather than Qt's drag and drop: the icon-mode implementation
    // filters out drops onto another card, and a native drag does not reliably come back to the
    // same view as its target. The Pages panel uses the same approach.
    int m_pressedRow = -1;
    bool m_dragging = false;
    QPoint m_pressPos;
    QPoint m_dragPos;
    QList<QUuid> m_dragIds;
    QPixmap m_dragPixmap;
    QPoint m_dragHotSpot;
    QTimer *m_dragScrollTimer = nullptr;
};

class OKULARPART_EXPORT DocumentBuilderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentBuilderWidget(QWidget *parent = nullptr);
    ~DocumentBuilderWidget() override;

    DocumentBuilderModel *model() const;

    /// Seeds the builder with a whole PDF the first time it is entered. Returns true when the file
    /// actually added pages (a non-PDF leaves the builder empty and returns false).
    bool seedFromPdf(const QString &path);
    /// Seeds the builder with the given pages of a PDF. Returns true when pages were added.
    bool seedFromPages(const QString &path, const QList<int> &pages);

    /// Enables the Save button: the file the builder was seeded from, which Save replaces in place.
    /// Only a real local document gets a target (never an untitled document).
    void setSaveTarget(const QString &path);
    QString saveTargetPath() const;

    bool isSeeded() const;
    int pageCount() const;
    QList<QUuid> selectedEntryIds() const;

    /// Adds files chosen by the user (used by drag & drop of external files too).
    void addFiles(const QStringList &paths);

    /// Empties the builder. The seeding flag is kept: clearing does not make the builder seed itself
    /// from the same document again.
    void clear();

    /// Empties the builder *and* forgets that it was seeded, for when the document it was seeded from
    /// goes away (Part::closeUrl).
    void reset();

    /// True while a synchronous export is writing. The toolbar and the grid are locked then.
    bool isExporting() const;

    /// The file dialog filter for image import, built from the formats the installed Qt image plugins
    /// can actually read (so SVG only shows up when an SVG plugin is present). Public for testing.
    static QString imageFileFilter();

Q_SIGNALS:
    void closeRequested();
    void pageCountChanged(int count);
    /// The user asked to open a document the builder just wrote.
    void openExportedDocument(const QString &path);
    /// A file was written and belongs in the shell's recent-files list.
    void addRecentFile(const QString &path);
    /// The builder replaced @p path in place (the Save button). The part reloads that document.
    void documentSaved(const QString &path);

private:
    void buildUi();
    void handleAddTile();
    void deleteRows(const QList<int> &rows);
    void updateCountLabel();
    void showContextMenu(const QPoint &globalPos);
    void addFilesViaDialog();
    void clearWithConfirmation();
    void updateSelectionActions();
    void moveSelectionBy(int delta);
    void moveSelectionToStart();
    void moveSelectionToEnd();

    /// Image file patterns the installed Qt image plugins can read (lower-case, sorted).
    static QStringList supportedImagePatterns();

    // --- Phase 4: images and page ranges ---
    void editImagePageSettings();

    // --- Phase 5b: export ---
    void exportPages(const QList<QUuid> &selection);
    void exportAllPages();
    void exportSelectedPages();
    /// Phase 10: replaces the document the builder was seeded from, in place.
    void saveToTarget();
    void updateExportActions();
    bool confirmSourceChanges();
    QString suggestedExportPath() const;
    void showExportSuccess(const QString &path, int pageCount);
    void setExportBusy(bool busy);

    // --- Phase 7: configuration and warnings ---
    void showConfigureMenu(const QPoint &globalPos);
    void applySettings();
    void maybeWarnAboutLargeImage(const QString &path);

    DocumentBuilderModel *m_model = nullptr;
    DocumentBuilderView *m_view = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_fileLabel = nullptr;
    QToolButton *m_addButton = nullptr;
    QToolButton *m_clearButton = nullptr;
    QToolButton *m_removeButton = nullptr;
    QToolButton *m_exportButton = nullptr;
    QToolButton *m_saveButton = nullptr;
    QToolButton *m_configureButton = nullptr;
    QMenu *m_exportMenu = nullptr;
    QAction *m_exportAllAction = nullptr;
    QAction *m_exportSelectedAction = nullptr;
    KMessageWidget *m_messageWidget = nullptr;
    QPointer<QProgressDialog> m_exportProgress;
    bool m_exporting = false;
    bool m_seeded = false;
    QString m_baseName;
    /// The local document this builder was seeded from; the Save button writes it in place.
    QString m_saveTarget;
    bool m_largeImageWarningShown = false;
};

#endif // DOCUMENTBUILDER_H
