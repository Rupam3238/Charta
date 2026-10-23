/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#include "documentbuilder.h"

#include "documentbuilderconfig.h"
#include "documentbuilderexport.h"
#include "documentbuildermodel.h"
#include "documentbuildersource.h"
#include "imagepagesettingsdialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QPointer>
#include <QProgressDialog>
#include <QResizeEvent>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QScrollBar>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KStandardGuiItem>

namespace
{
constexpr int kTileWidth = 150;
constexpr int kThumbHeight = 200;
constexpr int kTilePadding = 8;
constexpr int kCaptionHeight = 22;
constexpr int kBadgeSize = 18;

// Grid geometry. The view owns the card size (DocumentBuilderView::updateGridSize) so the Configure
// popup can change the column count and the card size without the delegate knowing about it.
constexpr int kMinCellWidth = 150;
constexpr qreal kCellAspect = 1.34; // cell height / cell width, before the caption strip

// An image this big is embedded at full resolution, which is worth saying once (Phase 7.5).
constexpr qint64 kLargeImagePixels = 100LL * 1000 * 1000;

// How many off-screen previews are handed to the document per event loop pass. Small enough that
// scrolling and painting stay responsive, large enough that a normal document fills in a few passes.
constexpr int kThumbnailFillBatch = 24;

/// The current page setup of an image entry, as the setup dialog wants it.
ImagePageSettings entrySettings(const BuilderEntry &entry)
{
    ImagePageSettings settings;
    settings.pageSize = entry.pageSize;
    settings.background = entry.background;
    settings.fit = entry.fit;
    settings.margin = entry.margin;
    settings.dpi = entry.dpi;
    return settings;
}

/// Pixel size of an image as it will be displayed (EXIF orientation applied); invalid on failure.
QSize imageSizeForPath(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.canRead() ? reader.size() : QSize();
}

/// Pixel count of an image as it will be displayed, or -1 when it cannot be read.
qint64 imagePixelCount(const QString &path)
{
    const QSize size = imageSizeForPath(path);
    if (!size.isValid() || size.isEmpty()) {
        return -1;
    }
    return qint64(size.width()) * size.height();
}
}

// SelectionBar: bottom bar shown during checkbox selection mode.
SelectionBar::SelectionBar(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(12);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet(QStringLiteral("color: #c4ced8; font-weight: bold;"));
    layout->addWidget(m_countLabel);

    layout->addStretch(1);

    m_doneButton = new QToolButton(this);
    m_doneButton->setText(i18n("Done"));
    m_doneButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_doneButton, &QToolButton::clicked, this, &SelectionBar::doneClicked);
    layout->addWidget(m_doneButton);

    m_selectAllButton = new QToolButton(this);
    m_selectAllButton->setText(i18n("Select all"));
    m_selectAllButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_selectAllButton, &QToolButton::clicked, this, &SelectionBar::selectAllClicked);
    layout->addWidget(m_selectAllButton);

    m_deleteButton = new QToolButton(this);
    m_deleteButton->setText(i18n("Delete"));
    m_deleteButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_deleteButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_deleteButton, &QToolButton::clicked, this, &SelectionBar::deleteClicked);
    layout->addWidget(m_deleteButton);

    m_duplicateButton = new QToolButton(this);
    m_duplicateButton->setText(i18n("Duplicate"));
    m_duplicateButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_duplicateButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_duplicateButton, &QToolButton::clicked, this, &SelectionBar::duplicateClicked);
    layout->addWidget(m_duplicateButton);

    hide();
    setSelectionCount(0);
}

void SelectionBar::setSelectionCount(int count)
{
    m_selectionCount = count;
    m_countLabel->setText(i18np("%1 page selected", "%1 pages selected", count));
    m_doneButton->setEnabled(count > 0);
    m_deleteButton->setEnabled(count > 0);
    m_duplicateButton->setEnabled(count > 0);
    m_selectAllButton->setEnabled(count >= 0);
}

DocumentBuilderDelegate::DocumentBuilderDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize DocumentBuilderDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    if (m_cellSize.isValid() && !m_cellSize.isEmpty()) {
        return m_cellSize;
    }
    return QSize(kTileWidth + 2 * kTilePadding, kThumbHeight + 2 * kTilePadding + (m_showFileNames ? kCaptionHeight : 0));
}

void DocumentBuilderDelegate::setShowFileNames(bool show)
{
    m_showFileNames = show;
}

void DocumentBuilderDelegate::setCellSize(const QSize &size)
{
    m_cellSize = size;
}

QRect DocumentBuilderDelegate::deleteBadgeRect(const QRect &itemRect)
{
    return QRect(itemRect.right() - kTilePadding - kBadgeSize - 2, itemRect.top() + kTilePadding + 2, kBadgeSize, kBadgeSize);
}

QRect DocumentBuilderDelegate::checkboxRect(const QRect &itemRect)
{
    return QRect(itemRect.right() - kTilePadding - kBadgeSize - 2, itemRect.bottom() - kTilePadding - kBadgeSize - 2, kBadgeSize, kBadgeSize);
}

void DocumentBuilderDelegate::setSelectMode(bool on)
{
    m_selectMode = on;
}

void DocumentBuilderDelegate::setSelectedEntryIds(const QSet<QUuid> &ids)
{
    m_selectedEntryIds = ids;
}

void DocumentBuilderDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect rect = option.rect;
    const bool isAddTile = index.data(DocumentBuilderModel::IsAddTileRole).toBool();
    const bool selected = option.state & QStyle::State_Selected;

    if (isAddTile) {
        painter->setPen(QPen(QColor(0x3b, 0x46, 0x53), 2, Qt::DashLine));
        painter->setBrush(QColor(0x1b, 0x21, 0x29));
        painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);

        const QPoint center = rect.center() - QPoint(0, kCaptionHeight / 2);
        painter->setPen(QPen(QColor(0x8a, 0x99, 0xa8), 2));
        painter->drawLine(center.x() - 14, center.y(), center.x() + 14, center.y());
        painter->drawLine(center.x(), center.y() - 14, center.x(), center.y() + 14);

        QFont font = option.font;
        font.setPointSizeF(qMax(8.0, font.pointSizeF() - 0.5));
        painter->setFont(font);
        painter->setPen(QColor(0x8a, 0x99, 0xa8));
        const QRect textRect(rect.left() + kTilePadding, rect.bottom() - kCaptionHeight, rect.width() - 2 * kTilePadding, kCaptionHeight);
        painter->drawText(textRect, Qt::AlignCenter, i18n("Add pages…"));
        painter->restore();
        return;
    }

    // Card background.
    painter->setPen(Qt::NoPen);
    painter->setBrush(selected ? QColor(0x16, 0x41, 0x4c) : QColor(0x22, 0x2a, 0x34));
    painter->drawRoundedRect(rect, 6, 6);

    const QRect thumbBox = rect.adjusted(kTilePadding, kTilePadding, -kTilePadding, -kTilePadding - (m_showFileNames ? kCaptionHeight : 0));
    const QImage thumbnail = index.data(DocumentBuilderModel::ThumbnailRole).value<QImage>();

    if (!thumbnail.isNull()) {
        QSize scaled = thumbnail.size();
        scaled.scale(thumbBox.size(), Qt::KeepAspectRatio);
        QRect target(QPoint(0, 0), scaled);
        target.moveCenter(thumbBox.center());
        painter->drawImage(target, thumbnail);
        painter->setPen(QPen(QColor(0x3b, 0x46, 0x53)));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(target.adjusted(0, 0, -1, -1));
    } else {
        painter->setPen(QPen(QColor(0x3b, 0x46, 0x53)));
        painter->setBrush(QColor(0xf3, 0xf4, 0xf6));
        painter->drawRect(thumbBox.adjusted(0, 0, -1, -1));
    }

    // Page number badge.
    const QRect badge(rect.left() + kTilePadding + 2, rect.top() + kTilePadding + 2, 30, kBadgeSize);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 170));
    painter->drawRoundedRect(badge, 4, 4);
    painter->setPen(Qt::white);
    QFont badgeFont = option.font;
    badgeFont.setPointSizeF(qMax(7.0, badgeFont.pointSizeF() - 2.0));
    painter->setFont(badgeFont);
    painter->drawText(badge, Qt::AlignCenter, QString::number(index.row() + 1));

    // Delete badge (shown when not in select mode) or selection checkbox (shown in select mode).
    if (m_selectMode) {
        const QRect checkRect = checkboxRect(rect);
        const bool checked = m_selectedEntryIds.contains(index.data(DocumentBuilderModel::EntryIdRole).value<QUuid>());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 120));
        painter->drawRoundedRect(checkRect, 4, 4);
        painter->setPen(QPen(QColor(0xdc, 0xe3, 0xea), 1.5));
        painter->drawRect(checkRect.adjusted(1, 1, -1, -1));
        if (checked) {
            painter->setBrush(QColor(0x27, 0xbf, 0xd6));
            painter->drawRoundedRect(checkRect.adjusted(5, 5, -5, -5), 2, 2);
        }
    } else {
        const QRect deleteRect = deleteBadgeRect(rect);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 120));
        painter->drawEllipse(deleteRect);
        painter->setPen(QPen(QColor(0xdc, 0xe3, 0xea), 1.5));
        painter->drawLine(deleteRect.adjusted(5, 5, -5, -5).topLeft(), deleteRect.adjusted(5, 5, -5, -5).bottomRight());
        painter->drawLine(deleteRect.adjusted(5, 5, -5, -5).topRight(), deleteRect.adjusted(5, 5, -5, -5).bottomLeft());
    }

    // Caption. The Configure popup can hide it and give the room back to the thumbnail.
    if (m_showFileNames) {
        QFont font = option.font;
        font.setPointSizeF(qMax(7.0, font.pointSizeF() - 1.0));
        painter->setFont(font);
        const QFontMetrics metrics(font);
        const QString caption = index.data(DocumentBuilderModel::CaptionRole).toString();
        const QRect captionRect(rect.left() + kTilePadding, rect.bottom() - kCaptionHeight, rect.width() - 2 * kTilePadding, kCaptionHeight);
        painter->setPen(selected ? QColor(0xff, 0xff, 0xff) : QColor(0xc4, 0xce, 0xd8));
        painter->drawText(captionRect, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(caption, Qt::ElideMiddle, captionRect.width()));
    }

    if (selected) {
        painter->setPen(QPen(QColor(0x27, 0xbf, 0xd6), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);
    }

    painter->restore();
}

DocumentBuilderView::DocumentBuilderView(QWidget *parent)
    : QListView(parent)
{
    setObjectName(QStringLiteral("documentBuilderView"));
    setViewMode(QListView::IconMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setWrapping(true);
    setUniformItemSizes(true);
    setSpacing(8);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Reordering is hand-rolled (beginEntryDrag); Qt only handles drops of external files, so its
    // own drag and the built-in drop indicator stay off.
    setDragDropMode(QAbstractItemView::DropOnly);
    setDropIndicatorShown(false);
    setAcceptDrops(true);
    m_dragScrollTimer = new QTimer(this);
    m_dragScrollTimer->setInterval(30);
    connect(m_dragScrollTimer, &QTimer::timeout, this, &DocumentBuilderView::autoScrollDuringDrag);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setItemDelegate(new DocumentBuilderDelegate(this));

    connect(this, &QWidget::customContextMenuRequested, this, &DocumentBuilderView::contextMenuRequested);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &DocumentBuilderView::scheduleThumbnailFill);
}

void DocumentBuilderView::setColumnCount(int columns)
{
    const int clamped = (columns <= DocumentBuilderConfig::kAutoColumns) ? DocumentBuilderConfig::kAutoColumns : qBound(DocumentBuilderConfig::kMinColumns, columns, DocumentBuilderConfig::kMaxColumns);
    if (clamped == m_columns) {
        return;
    }
    m_columns = clamped;
    updateGridSize();
}

void DocumentBuilderView::setTileScale(qreal scale)
{
    const qreal clamped = qBound(0.5, scale, 2.0);
    if (qFuzzyCompare(clamped, m_tileScale)) {
        return;
    }
    m_tileScale = clamped;
    updateGridSize();
}

void DocumentBuilderView::setShowFileNames(bool show)
{
    auto *delegate = qobject_cast<DocumentBuilderDelegate *>(itemDelegate());
    if (!delegate || delegate->showFileNames() == show) {
        return;
    }
    delegate->setShowFileNames(show);
    updateGridSize();
    viewport()->update();
}

void DocumentBuilderView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    updateGridSize();
}

void DocumentBuilderView::showEvent(QShowEvent *event)
{
    QListView::showEvent(event);
    // The builder is a stacked page: it is created (and the model is populated) before it is ever
    // shown, so this is the first moment the viewport has a size worth measuring "visible" against.
    scheduleThumbnailFill();
}

void DocumentBuilderView::updateGridSize()
{
    if (m_inGridUpdate) {
        return;
    }
    m_inGridUpdate = true;
    const auto guard = qScopeGuard([this] { m_inGridUpdate = false; });

    // Deliberately based on width() and not on viewport()->width(): the latter shrinks when a scroll
    // bar appears, which would make the column count depend on whether the grid needs a scroll bar and
    // could oscillate by a column. Reserving the extent unconditionally keeps the count stable.
    const int spacing = this->spacing();
    const int scrollExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int available = width() - 2 * spacing - scrollExtent;
    if (available <= 0) {
        return;
    }

    // Auto: the largest count in 2..6 that still leaves a readable card.
    // The effective minimum width per column grows with m_tileScale, so zooming in
    // yields fewer (bigger) columns and zooming out yields more (smaller) ones.
    int columns = m_columns;
    if (columns <= DocumentBuilderConfig::kAutoColumns) {
        columns = DocumentBuilderConfig::kMinColumns;
        for (int candidate = DocumentBuilderConfig::kMaxColumns; candidate >= DocumentBuilderConfig::kMinColumns; --candidate) {
            if (available / candidate >= kMinCellWidth * m_tileScale + spacing) {
                columns = candidate;
                break;
            }
        }
    }

    const int cellWidth = qMax(96, available / columns - spacing);
    const auto *delegate = qobject_cast<const DocumentBuilderDelegate *>(itemDelegate());
    const int captionHeight = (delegate && delegate->showFileNames()) ? kCaptionHeight : 0;
    // The card is taller than it is wide because pages usually are; the thumbnail inside keeps its
    // own aspect and is only centred in the box.
    const int cellHeight = qRound(cellWidth * kCellAspect * m_tileScale) + captionHeight;

    if (auto *mutableDelegate = qobject_cast<DocumentBuilderDelegate *>(itemDelegate())) {
        mutableDelegate->setCellSize(QSize(cellWidth, cellHeight));
    }
    setGridSize(QSize(cellWidth, cellHeight));

    // Ask the model for thumbnails at the size we are about to draw them.
    if (auto *listModel = qobject_cast<DocumentBuilderModel *>(model())) {
        listModel->setThumbnailWidth(qMax(32, cellWidth - 2 * kTilePadding));
    }

    // New column count or card size: rows that were not visible before have to be filled too.
    scheduleThumbnailFill();
}

// --- Thumbnail fill -----------------------------------------------------------------------------

void DocumentBuilderView::scheduleThumbnailFill()
{
    if (!m_fillTimer) {
        m_fillTimer = new QTimer(this);
        m_fillTimer->setSingleShot(true);
        connect(m_fillTimer, &QTimer::timeout, this, &DocumentBuilderView::fillThumbnails);
    }
    m_pendingAtLastFill = -1;
    m_fillTimer->start(0);
}

void DocumentBuilderView::fillThumbnails()
{
    auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    if (!listModel || listModel->pageCount() == 0) {
        return;
    }

    // Visible cards are requested at THUMBNAILS_PRIO and the rest at THUMBNAILS_PRELOAD_PRIO, so
    // the document keeps serving what the user is looking at first even when the whole grid is
    // queued. The rest is handed to the document in small batches so painting and scrolling keep
    // getting a slice of the event loop.
    const QRect viewportRect = viewport()->rect();
    QList<int> visible;
    QList<int> background;
    int needing = 0;
    bool more = false;
    for (int row = 0; row < listModel->pageCount(); ++row) {
        if (!listModel->needsThumbnailRequest(row)) {
            continue;
        }
        ++needing;
        if (visualRect(listModel->index(row, 0)).intersects(viewportRect)) {
            visible.append(row);
        } else if (background.size() < kThumbnailFillBatch) {
            background.append(row);
        } else {
            more = true;
        }
    }

    if (!visible.isEmpty()) {
        listModel->requestThumbnails(visible, false);
    }
    if (!background.isEmpty()) {
        listModel->requestThumbnails(background, true);
    }

    // Something visible is still being rendered: check back later. That is what recovers a request
    // the document cancelled because a higher-priority one came in (a cancelled request never
    // notifies), and it also stops once every visible card has its preview.
    bool visiblePending = false;
    for (int row = 0; row < listModel->pageCount(); ++row) {
        if (!listModel->isThumbnailPending(row)) {
            continue;
        }
        if (visualRect(listModel->index(row, 0)).intersects(viewportRect)) {
            visiblePending = true;
            break;
        }
    }
    if (visiblePending) {
        m_fillTimer->start(1000);
        return;
    }

    if (more && (m_pendingAtLastFill < 0 || needing < m_pendingAtLastFill)) {
        m_pendingAtLastFill = needing;
        // Continue with the next batch, but re-arm the timer directly: scheduleThumbnailFill()
        // resets the progress guard, which would turn a stalled pass into an endless loop.
        m_fillTimer->start(0);
    } else {
        m_pendingAtLastFill = -1;
    }
}

QList<int> DocumentBuilderView::selectedEntryRows() const
{
    QList<int> rows;
    const auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    if (!listModel || !selectionModel()) {
        return rows;
    }
    const QModelIndexList indexes = selectionModel()->selectedIndexes();
    for (const QModelIndex &index : indexes) {
        if (!listModel->isAddTile(index.row())) {
            rows.append(index.row());
        }
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}

QList<QUuid> DocumentBuilderView::selectedEntryIds() const
{
    QList<QUuid> ids;
    const auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    if (!listModel) {
        return ids;
    }
    const QList<int> rows = selectedEntryRows();
    for (int row : rows) {
        ids.append(listModel->entryIdAt(row));
    }
    return ids;
}

void DocumentBuilderView::selectEntryIds(const QList<QUuid> &ids)
{
    auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    if (!listModel || !selectionModel()) {
        return;
    }
    selectionModel()->clearSelection();
    bool haveCurrent = false;
    for (const QUuid &id : ids) {
        const int row = listModel->rowForId(id);
        if (row >= 0) {
            const QModelIndex index = listModel->index(row, 0);
            selectionModel()->select(index, QItemSelectionModel::Select);
            if (!haveCurrent) {
                // Keep keyboard navigation anchored on the moved block instead of the stale row.
                selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                haveCurrent = true;
            }
        }
    }
}

void DocumentBuilderView::dragEnterEvent(QDragEnterEvent *event)
{
    // Only external files use Qt's drag and drop. Reordering a card is a manual drag (see
    // beginEntryDrag()): Qt's icon-mode drag filter refuses drops onto another card, and a native
    // drag never reaches this view as a drop target.
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void DocumentBuilderView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void DocumentBuilderView::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
}

void DocumentBuilderView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        QStringList remote;
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            } else {
                remote.append(url.toString());
            }
        }
        if (!paths.isEmpty()) {
            Q_EMIT filesDropped(paths);
        }
        if (!remote.isEmpty()) {
            Q_EMIT remoteFilesDropped(remote);
        }
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void DocumentBuilderView::beginEntryDrag()
{
    auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    if (!listModel || !acceptDrops() || m_pressedRow < 0 || m_pressedRow >= listModel->pageCount()) {
        return;
    }

    QList<int> rows = selectedEntryRows();
    if (rows.isEmpty()) {
        rows.append(m_pressedRow);
    } else if (!rows.contains(m_pressedRow)) {
        // Most likely Ctrl+clicking a selected card toggled it off: there is nothing to drag.
        return;
    } else {
        // The ghost must be the card under the cursor, not the first of a scattered selection.
        rows.removeOne(m_pressedRow);
        rows.prepend(m_pressedRow);
    }

    m_dragIds.clear();
    for (int row : std::as_const(rows)) {
        m_dragIds.append(listModel->entryIdAt(row));
    }
    m_dragPixmap = dragPixmap(rows);
    m_dragHotSpot = m_pressPos - visualRect(listModel->index(m_pressedRow, 0)).topLeft();
    m_dragging = true;
    m_dragPos = m_pressPos;
    setCursor(Qt::ClosedHandCursor);
    m_dragScrollTimer->start();
    viewport()->update();
}

void DocumentBuilderView::updateEntryDrag(const QPoint &pos)
{
    m_dragPos = pos;
    m_dropRow = dropRowFor(pos);
    viewport()->update();
}

void DocumentBuilderView::cancelEntryDrag()
{
    m_dragScrollTimer->stop();
    const bool wasDragging = m_dragging;
    m_dragging = false;
    m_pressedRow = -1;
    m_dragIds.clear();
    m_dragPixmap = QPixmap();
    m_dragHotSpot = QPoint();
    m_dropRow = -1;
    if (wasDragging) {
        unsetCursor();
        viewport()->update();
    }
}

void DocumentBuilderView::finishEntryDrag(bool drop)
{
    auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    const QList<QUuid> ids = m_dragIds;
    const int destination = m_dropRow;
    cancelEntryDrag();
    if (!drop || !listModel || ids.isEmpty() || destination < 0) {
        return;
    }

    QList<int> rows;
    rows.reserve(ids.size());
    for (const QUuid &id : ids) {
        const int row = listModel->rowForId(id);
        if (row >= 0) {
            rows.append(row);
        }
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        return;
    }

    bool moved = false;
    if (rows.last() - rows.first() + 1 == rows.size()) {
        moved = listModel->moveRows(QModelIndex(), rows.first(), rows.size(), QModelIndex(), destination);
    } else {
        // A scattered selection cannot be expressed with one moveRows().
        moved = listModel->moveEntriesBefore(ids, destination);
    }
    if (moved) {
        selectEntryIds(ids);
    }
}

void DocumentBuilderView::autoScrollDuringDrag()
{
    if (!m_dragging) {
        m_dragScrollTimer->stop();
        return;
    }
    const int edge = 40;
    const int maxStep = 26;
    const int height = viewport()->height();
    int delta = 0;
    if (m_dragPos.y() < edge) {
        delta = -qMax(2, (edge - m_dragPos.y()) * maxStep / edge);
    } else if (m_dragPos.y() > height - edge) {
        delta = qMax(2, (m_dragPos.y() - (height - edge)) * maxStep / edge);
    }
    if (delta == 0) {
        return;
    }
    QScrollBar *bar = verticalScrollBar();
    const int before = bar->value();
    bar->setValue(before + delta);
    if (bar->value() != before) {
        // The grid scrolled under the motionless cursor: the drop position changed too.
        updateEntryDrag(m_dragPos);
    }
}

void DocumentBuilderView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);

    if (m_dropRow >= 0) {
        const QRect bar = dropIndicatorRect();
        if (!bar.isNull()) {
            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0x27, 0xbf, 0xd6));
            painter.drawRoundedRect(bar, 1.5, 1.5);
        }
    }

    if (m_dragging && !m_dragPixmap.isNull()) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPoint topLeft = m_dragPos - m_dragHotSpot;
        painter.setOpacity(0.85);
        painter.drawPixmap(topLeft, m_dragPixmap);
        painter.setOpacity(1.0);
        painter.setPen(QPen(QColor(0x27, 0xbf, 0xd6), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRect(topLeft, m_dragPixmap.size()).adjusted(0, 0, -1, -1), 6, 6);
    }
}

int DocumentBuilderView::dropRowFor(const QPoint &pos) const
{
    const auto *listModel = qobject_cast<const DocumentBuilderModel *>(model());
    if (!listModel) {
        return -1;
    }
    const int pages = listModel->pageCount();
    const QModelIndex index = indexAt(pos);
    if (!index.isValid() || index.row() >= pages) {
        // Empty space or the add tile: append, i.e. insert before the add tile.
        return pages;
    }

    // Reading order: the lower half of a card means "after it", the upper half means "before it".
    // Inside the upper half the right side still means "after", so dropping in the gap to the right
    // of a card does not jump in front of it.
    const QRect rect = visualRect(index);
    if (pos.y() > rect.center().y() || pos.x() > rect.center().x()) {
        return index.row() + 1;
    }
    return index.row();
}

QRect DocumentBuilderView::dropIndicatorRect() const
{
    const auto *listModel = qobject_cast<const DocumentBuilderModel *>(model());
    if (!listModel || m_dropRow < 0) {
        return QRect();
    }
    const int anchor = qBound(0, m_dropRow, listModel->pageCount());
    const QModelIndex index = listModel->index(anchor, 0);
    if (!index.isValid()) {
        return QRect();
    }
    const QRect rect = visualRect(index);
    if (rect.isNull()) {
        return QRect();
    }
    const int x = rect.left() - spacing() / 2 - 1;
    return QRect(x, rect.top(), 3, rect.height());
}

QPixmap DocumentBuilderView::dragPixmap(const QList<int> &rows) const
{
    if (rows.isEmpty() || !model()) {
        return QPixmap();
    }
    const QModelIndex first = model()->index(rows.first(), 0);
    if (!first.isValid()) {
        return QPixmap();
    }
    QPixmap pixmap = viewport()->grab(visualRect(first));
    if (pixmap.isNull()) {
        return pixmap;
    }
    if (rows.size() > 1) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRect badge(pixmap.width() - 36, 6, 30, 22);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x27, 0xbf, 0xd6));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(badge, Qt::AlignCenter, QString::number(rows.size()));
    }
    return pixmap;
}

void DocumentBuilderView::mousePressEvent(QMouseEvent *event)
{
    m_pressedRow = -1;
    if (event->button() == Qt::LeftButton) {
        const QPoint pos = event->position().toPoint();
        const QModelIndex index = indexAt(pos);
        const auto *listModel = qobject_cast<const DocumentBuilderModel *>(model());
        const auto *delegate = qobject_cast<const DocumentBuilderDelegate *>(itemDelegate());
        if (index.isValid() && listModel && !listModel->isAddTile(index.row()) && delegate) {
            // Checkbox click while in select mode: toggle selection, no drag.
            if (delegate->isSelectMode() && delegate->checkboxRect(visualRect(index)).contains(pos)) {
                Q_EMIT checkboxToggled(index.data(DocumentBuilderModel::EntryIdRole).value<QUuid>());
                QListView::mousePressEvent(event);
                return;
            }
            if (!delegate->checkboxRect(visualRect(index)).contains(pos)
                && !DocumentBuilderDelegate::deleteBadgeRect(visualRect(index)).contains(pos)) {
                m_pressedRow = index.row();
                m_pressPos = pos;
            }
        }
    }
    QListView::mousePressEvent(event);
}

void DocumentBuilderView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        updateEntryDrag(event->position().toPoint());
        return;
    }
    if (m_pressedRow >= 0 && (event->buttons() & Qt::LeftButton)
        && (event->position().toPoint() - m_pressPos).manhattanLength() > QApplication::startDragDistance()) {
        beginEntryDrag();
        if (m_dragging) {
            updateEntryDrag(event->position().toPoint());
            return;
        }
        m_pressedRow = -1;
    }
    QListView::mouseMoveEvent(event);
}

void DocumentBuilderView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint pos = event->position().toPoint();
        if (viewport()->rect().contains(pos)) {
            m_dropRow = dropRowFor(pos);
            finishEntryDrag(true);
        } else {
            // Released over the toolbar, the sidebar or outside the window: no reorder.
            finishEntryDrag(false);
        }
        return;
    }
    m_pressedRow = -1;

    const QModelIndex index = indexAt(event->position().toPoint());
    const auto *listModel = qobject_cast<DocumentBuilderModel *>(model());
    if (index.isValid() && listModel && event->button() == Qt::LeftButton) {
        if (listModel->isAddTile(index.row())) {
            if (visualRect(index).contains(event->position().toPoint())) {
                Q_EMIT addTileActivated();
                return;
            }
        } else if (DocumentBuilderDelegate::deleteBadgeRect(visualRect(index)).contains(event->position().toPoint())) {
            Q_EMIT deleteRequested(QList<int>() << index.row());
            return;
        }
    }
    QListView::mouseReleaseEvent(event);
}

void DocumentBuilderView::keyPressEvent(QKeyEvent *event)
{
    if (m_dragging && event->key() == Qt::Key_Escape) {
        finishEntryDrag(false);
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const QList<int> rows = selectedEntryRows();
        if (!rows.isEmpty()) {
            Q_EMIT deleteRequested(rows);
            return;
        }
    }

    // Ctrl+Plus / Ctrl+Minus zoom the tile grid (§Phase 6).
    if ((event->modifiers() & Qt::ControlModifier) &&
        (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Minus)) {
        setTileScale(m_tileScale + (event->key() == Qt::Key_Plus ? 0.1 : -0.1));
        return;
    }

    // Enter/Space on the add tile opens the file dialog, matching the mouse behaviour (§10.2).
    const QModelIndex current = currentIndex();
    if (current.isValid() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space)) {
        const auto *listModel = qobject_cast<const DocumentBuilderModel *>(model());
        if (listModel && listModel->isAddTile(current.row())) {
            Q_EMIT addTileActivated();
            return;
        }
    }

    QListView::keyPressEvent(event);
}

void DocumentBuilderView::wheelEvent(QWheelEvent *event)
{
    // Ctrl+scroll zooms the tile grid (§Phase 6).
    if ((event->modifiers() & Qt::ControlModifier) && event->angleDelta().y() != 0) {
        setTileScale(m_tileScale + (event->angleDelta().y() > 0 ? 0.1 : -0.1));
        event->accept();
        return;
    }
    QListView::wheelEvent(event);
}

DocumentBuilderWidget::DocumentBuilderWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("documentBuilder"));
    buildUi();
}

DocumentBuilderWidget::~DocumentBuilderWidget() = default;

DocumentBuilderModel *DocumentBuilderWidget::model() const
{
    return m_model;
}

void DocumentBuilderWidget::buildUi()
{
    setStyleSheet(QStringLiteral("QWidget#documentBuilder { background: #14181e; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("documentBuilderToolbar"));
    toolbar->setStyleSheet(QStringLiteral(
        "QWidget#documentBuilderToolbar { background: #202630; border-bottom: 1px solid #3b4653; }"
        "QWidget#documentBuilderToolbar QToolButton { color: #c4ced8; background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 6px 10px; }"
        "QWidget#documentBuilderToolbar QToolButton:hover { color: #ffffff; background: #303a46; }"
        "QWidget#documentBuilderToolbar QToolButton:disabled { color: #5b6673; }"
        "QWidget#documentBuilderToolbar QLabel { color: #c4ced8; }"));
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(6);

    auto *backButton = new QToolButton(toolbar);
    backButton->setText(i18n("Back"));
    backButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    backButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    backButton->setObjectName(QStringLiteral("documentBuilderBackButton"));
    backButton->setAccessibleName(i18n("Back to the document"));
    connect(backButton, &QToolButton::clicked, this, &DocumentBuilderWidget::closeRequested);
    toolbarLayout->addWidget(backButton);

    auto *title = new QLabel(i18n("Document Builder"), toolbar);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    toolbarLayout->addWidget(title);

    // Which document the build started from. Informational only, never used for I/O.
    m_fileLabel = new QLabel(toolbar);
    m_fileLabel->setObjectName(QStringLiteral("documentBuilderFileLabel"));
    m_fileLabel->setStyleSheet(QStringLiteral("color: #8a99a8;"));
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    toolbarLayout->addWidget(m_fileLabel);

    toolbarLayout->addStretch(1);

    m_countLabel = new QLabel(toolbar);
    toolbarLayout->addWidget(m_countLabel);

    m_addButton = new QToolButton(toolbar);
    m_addButton->setText(i18n("Add Files"));
    m_addButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_addButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_addButton->setObjectName(QStringLiteral("documentBuilderAddButton"));
    m_addButton->setAccessibleName(i18n("Add files to the document being built"));
    connect(m_addButton, &QToolButton::clicked, this, &DocumentBuilderWidget::addFilesViaDialog);
    toolbarLayout->addWidget(m_addButton);

    m_clearButton = new QToolButton(toolbar);
    m_clearButton->setText(i18n("Clear All"));
    m_clearButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-all")));
    m_clearButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_clearButton->setObjectName(QStringLiteral("documentBuilderClearButton"));
    m_clearButton->setAccessibleName(i18n("Remove every page from the builder"));
    connect(m_clearButton, &QToolButton::clicked, this, &DocumentBuilderWidget::clearWithConfirmation);
    toolbarLayout->addWidget(m_clearButton);

    m_removeButton = new QToolButton(toolbar);
    m_removeButton->setText(i18n("Remove Selected"));
    m_removeButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_removeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_removeButton->setObjectName(QStringLiteral("documentBuilderRemoveButton"));
    m_removeButton->setAccessibleName(i18n("Remove the selected pages"));
    m_removeButton->setEnabled(false);
    connect(m_removeButton, &QToolButton::clicked, this, [this] { deleteRows(m_view->selectedEntryRows()); });
    toolbarLayout->addWidget(m_removeButton);

    m_configureButton = new QToolButton(toolbar);
    m_configureButton->setText(i18n("Configure"));
    m_configureButton->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    m_configureButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_configureButton->setObjectName(QStringLiteral("documentBuilderConfigureButton"));
    m_configureButton->setAccessibleName(i18n("Change how the pages are shown"));
    m_configureButton->setToolTip(i18n("Change how the pages are shown"));
    connect(m_configureButton, &QToolButton::clicked, this, [this] { showConfigureMenu(m_configureButton->mapToGlobal(m_configureButton->rect().bottomLeft())); });
    toolbarLayout->addWidget(m_configureButton);

    m_saveButton = new QToolButton(toolbar);
    m_saveButton->setText(i18n("Save"));
    m_saveButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    m_saveButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_saveButton->setObjectName(QStringLiteral("documentBuilderSaveButton"));
    m_saveButton->setAccessibleName(i18n("Save the built pages to the open document"));
    m_saveButton->setEnabled(false);
    connect(m_saveButton, &QToolButton::clicked, this, &DocumentBuilderWidget::saveToTarget);
    toolbarLayout->addWidget(m_saveButton);

    m_exportButton = new QToolButton(toolbar);
    m_exportButton->setText(i18n("Export"));
    m_exportButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    m_exportButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_exportButton->setObjectName(QStringLiteral("documentBuilderExportButton"));
    m_exportButton->setAccessibleName(i18n("Export the built document"));
    m_exportButton->setPopupMode(QToolButton::InstantPopup);
    m_exportMenu = new QMenu(m_exportButton);
    m_exportAllAction = m_exportMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Export All Pages…"));
    connect(m_exportAllAction, &QAction::triggered, this, &DocumentBuilderWidget::exportAllPages);
    m_exportSelectedAction = m_exportMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), i18n("Export Selected Pages…"));
    connect(m_exportSelectedAction, &QAction::triggered, this, &DocumentBuilderWidget::exportSelectedPages);
    m_exportButton->setMenu(m_exportMenu);
    m_exportButton->setEnabled(false);
    toolbarLayout->addWidget(m_exportButton);

    layout->addWidget(toolbar);

    // Non-blocking feedback for import/export results and one-time warnings (Phase 5b / 7.5). It only
    // takes space once it has something to say.
    m_messageWidget = new KMessageWidget(this);
    m_messageWidget->setObjectName(QStringLiteral("documentBuilderMessageWidget"));
    m_messageWidget->setCloseButtonVisible(true);
    m_messageWidget->setWordWrap(true);
    m_messageWidget->hide();
    layout->addWidget(m_messageWidget);

    m_emptyLabel = new QLabel(i18n("Add PDFs or images to start building a document."), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: #8a99a8; padding: 8px;"));
    layout->addWidget(m_emptyLabel);

    m_view = new DocumentBuilderView(this);
    m_view->setAccessibleName(i18n("Pages in the document being built"));
    m_model = new DocumentBuilderModel(this);
    m_view->setModel(m_model);
    layout->addWidget(m_view, 1);

    // Selection bar: shown at the bottom when checkbox select mode is on.
    m_selectionBar = new SelectionBar(this);
    layout->addWidget(m_selectionBar);
    connect(m_selectionBar, &SelectionBar::doneClicked, this, [this] { setSelectMode(false); });
    connect(m_selectionBar, &SelectionBar::selectAllClicked, this, &DocumentBuilderWidget::selectAllEntries);
    connect(m_selectionBar, &SelectionBar::deleteClicked, this, [this] {
        deleteRows(m_view->selectedEntryRows());
        setSelectMode(false);
    });
    connect(m_selectionBar, &SelectionBar::duplicateClicked, this, [this] {
        duplicateRows();
        setSelectMode(false);
    });

    connect(m_view, &DocumentBuilderView::addTileActivated, this, &DocumentBuilderWidget::handleAddTile);
    connect(m_view, &DocumentBuilderView::deleteRequested, this, &DocumentBuilderWidget::deleteRows);
    connect(m_view, &DocumentBuilderView::contextMenuRequested, this, &DocumentBuilderWidget::showContextMenu);
    connect(m_view, &DocumentBuilderView::filesDropped, this, &DocumentBuilderWidget::addFiles);
    connect(m_view, &DocumentBuilderView::remoteFilesDropped, this, [this](const QStringList &urls) {
        QMessageBox::information(this,
                                 i18n("Document Builder"),
                                 i18n("Only local files can be added. These were ignored: %1", urls.join(QLatin1Char('\n'))));
    });
    connect(m_model, &DocumentBuilderModel::countsChanged, this, &DocumentBuilderWidget::updateCountLabel);
    connect(m_model, &DocumentBuilderModel::countsChanged, m_view, &DocumentBuilderView::scheduleThumbnailFill);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DocumentBuilderWidget::updateSelectionActions);
    connect(m_view, &DocumentBuilderView::checkboxToggled, this, &DocumentBuilderWidget::toggleEntry);

    applySettings();
    updateCountLabel();
    updateSelectionActions();
    m_view->scheduleThumbnailFill();
}

void DocumentBuilderWidget::updateSelectionActions()
{
    const bool hasRows = !m_view->selectedEntryRows().isEmpty();
    m_removeButton->setEnabled(hasRows && !m_exporting);
    if (m_exportSelectedAction) {
        m_exportSelectedAction->setEnabled(hasRows && !m_exporting);
    }
}

QStringList DocumentBuilderWidget::supportedImagePatterns()
{
    // Advertise exactly what the installed Qt image plugins can read. If there is no image plugin
    // (or no SVG plugin), those formats are simply not offered.
    QStringList patterns;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QByteArray &format : formats) {
        patterns.append(QStringLiteral("*.") + QString::fromLatin1(format).toLower());
    }
    patterns.removeDuplicates();
    patterns.sort();
    return patterns;
}

QString DocumentBuilderWidget::imageFileFilter()
{
    const QStringList patterns = supportedImagePatterns();

    QString filter;
    if (!patterns.isEmpty()) {
        filter = i18n("Images (%1)", patterns.join(QLatin1Char(' '))) + QLatin1String(";;");
    }
    filter += i18n("All files (*)");
    return filter;
}

void DocumentBuilderWidget::addFilesViaDialog()
{
    // One entry point for everything (§10.2): the user does not have to classify a file first, the
    // model decides from the file itself whether it is a document or an image.
    const QStringList images = supportedImagePatterns();
    const QString imagePatterns = images.join(QLatin1Char(' '));

    QStringList filters;
    if (!images.isEmpty()) {
        filters.append(i18n("Documents and images (*.pdf %1)", imagePatterns));
    }
    filters.append(i18n("PDF documents (*.pdf)"));
    if (!images.isEmpty()) {
        filters.append(i18n("Images (%1)", imagePatterns));
    }
    filters.append(i18n("All files (*)"));

    const QStringList paths = QFileDialog::getOpenFileNames(this, i18n("Add Files"), QString(), filters.join(QLatin1String(";;")));
    if (!paths.isEmpty()) {
        addFiles(paths);
    }
}

void DocumentBuilderWidget::editImagePageSettings()
{
    const QList<int> rows = m_view->selectedEntryRows();
    const QList<QUuid> ids = m_view->selectedEntryIds();
    if (rows.isEmpty() || !m_model->selectionHasImages(ids)) {
        return;
    }

    const BuilderEntry *first = m_model->entryAt(rows.first());
    if (!first) {
        return;
    }

    ImagePageSettingsDialog dialog(entrySettings(*first), imageSizeForPath(first->sourcePath), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_model->setImageSettings(ids, dialog.settings());
    }
}

void DocumentBuilderWidget::handleAddTile()
{
    addFilesViaDialog();
}

void DocumentBuilderWidget::addFiles(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }

    // The file itself decides how it is handled, not the menu entry the user picked (§10.2). The
    // suffix is the common case; a PDF that was renamed or has no extension still starts with
    // "%PDF-" within the first 1024 bytes (ISO 32000 allows leading bytes).
    const auto looksLikePdf = [](const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        return file.read(1024).contains(QByteArrayLiteral("%PDF-"));
    };

    // Importing several files (or one big PDF) reads page metadata and starts rendering thumbnails,
    // which can take a moment: show that the builder is busy instead of looking frozen (§2.7).
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // More than one file gets a real, cancelable-between-files progress dialog. A single file only
    // gets the busy cursor: there is nothing to cancel between.
    QScopedPointer<QProgressDialog> progress;
    if (paths.size() > 1) {
        progress.reset(new QProgressDialog(i18n("Adding files…"), i18n("Cancel"), 0, paths.size(), this));
        progress->setWindowTitle(i18n("Document Builder"));
        progress->setWindowModality(Qt::ApplicationModal);
        progress->setMinimumDuration(0);
        progress->setAutoClose(false);
        progress->setAutoReset(false);
        progress->setValue(0);
        QCoreApplication::processEvents();
    }

    QStringList errors;
    for (int position = 0; position < paths.size(); ++position) {
        if (progress && progress->wasCanceled()) {
            break; // cancel between whole files, never mid-decode
        }
        const QString path = paths.at(position);
        const QFileInfo info(path);
        if (progress) {
            progress->setLabelText(i18n("Adding “%1”…", info.fileName()));
            progress->setValue(position);
            QCoreApplication::processEvents();
        }

        if (!info.exists()) {
            errors.append(i18n("“%1” does not exist.", info.fileName()));
            continue;
        }

        const QString suffix = info.suffix().toLower();
        if (suffix == QLatin1String("pdf") || looksLikePdf(path)) {
            QString error;
            int added = m_model->addPdf(path, &error);
            if (added < 0) {
                const QString canonical = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
                BuilderSource *source = m_model->sourceForPath(canonical);
                // Encrypted file: ask for the password, up to three times (§2.3). The password is
                // stored once on the source and never written to a message or a log.
                for (int attempt = 0; attempt < 3 && added < 0 && source && source->passwordRequired(); ++attempt) {
                    bool ok = false;
                    const QString password = QInputDialog::getText(this,
                                                                   i18n("Password Required"),
                                                                   attempt == 0 ? i18n("Enter the password for “%1”:", info.fileName())
                                                                                : i18n("Wrong password. Try again for “%1”:", info.fileName()),
                                                                   QLineEdit::Password,
                                                                   QString(),
                                                                   &ok);
                    if (!ok) {
                        break; // cancel skips this file
                    }
                    error.clear();
                    added = m_model->addPdfWithPassword(path, password, &error);
                }
            }
            if (added < 0 && !error.isEmpty()) {
                errors.append(error);
            }
        } else {
            QString error;
            if (m_model->addImage(path, &error) < 0) {
                if (!error.isEmpty()) {
                    errors.append(error);
                }
            } else {
                maybeWarnAboutLargeImage(path);
            }
        }
    }

    if (progress) {
        progress->setValue(paths.size());
        progress.reset();
    }
    QApplication::restoreOverrideCursor();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, i18n("Document Builder"), errors.join(QLatin1Char('\n')));
    }
    updateCountLabel();
}

void DocumentBuilderWidget::deleteRows(const QList<int> &rows)
{
    if (rows.isEmpty()) {
        return;
    }

    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());

    // After the removal the selection sits on a row that no longer exists; move it to whatever took
    // the place of the first removed page.
    const int firstRemoved = sorted.last();
    for (int row : sorted) {
        m_model->removeRows(row, 1);
    }
    const QList<QUuid> ids = m_model->entryIds();
    if (!ids.isEmpty()) {
        const int target = qBound(0, firstRemoved, ids.size() - 1);
        m_view->selectEntryIds(QList<QUuid>() << ids.at(target));
    }
    updateSelectionActions();
}

void DocumentBuilderWidget::clearWithConfirmation()
{
    if (m_model->pageCount() == 0) {
        return;
    }

    const auto answer = QMessageBox::question(this,
                                              i18n("Clear Document Builder"),
                                              i18n("Remove all %1 pages from the builder?", m_model->pageCount()),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        clear();
    }
}

void DocumentBuilderWidget::showContextMenu(const QPoint &globalPos)
{
    const QList<int> rows = m_view->selectedEntryRows();
    const QList<QUuid> ids = m_view->selectedEntryIds();
    const bool hasImages = m_model->selectionHasImages(ids);

    QMenu menu(this);
    QAction *addAction = menu.addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Add Files…"));
    QAction *deleteAction = nullptr;
    QAction *duplicateAction = nullptr;
    QAction *setupAction = nullptr;
    if (!rows.isEmpty()) {
        menu.addSeparator();
        deleteAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18np("Remove page", "Remove %1 pages", rows.size()));
        duplicateAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18np("Duplicate page", "Duplicate %1 pages", rows.size()));
        menu.addSeparator();
        if (hasImages) {
            menu.addSeparator();
            setupAction = menu.addAction(QIcon::fromTheme(QStringLiteral("image-x-generic")), i18n("Page Setup…"));
        }
    }
    menu.addSeparator();
    QAction *selectAllAction = menu.addAction(i18n("Select All"));
    QAction *selectModeAction = nullptr;
    if (m_selectMode) {
        selectModeAction = menu.addAction(i18n("Exit Selection Mode"));
    } else if (!rows.isEmpty()) {
        selectModeAction = menu.addAction(i18n("Enter Selection Mode"));
    }

    QAction *chosen = menu.exec(globalPos);
    if (chosen == addAction) {
        addFilesViaDialog();
    } else if (chosen && chosen == deleteAction) {
        deleteRows(rows);
    } else if (chosen == duplicateAction) {
        duplicateRows();
    } else if (chosen == setupAction) {
        editImagePageSettings();
    } else if (chosen == selectAllAction) {
        m_view->selectAll();
    } else if (chosen == selectModeAction) {
        setSelectMode(!m_selectMode);
    }
    updateSelectionActions();
}

void DocumentBuilderWidget::duplicateRows()
{
    const QList<QUuid> ids = m_view->selectedEntryIds();
    if (ids.isEmpty() || m_model->pageCount() == 0) {
        return;
    }

    const QList<QUuid> newIds = m_model->duplicateEntries(ids);
    if (!newIds.isEmpty()) {
        m_view->selectEntryIds(newIds);
        m_view->setFocus();
    }
    updateSelectionActions();
}

void DocumentBuilderWidget::setSelectMode(bool on)
{
    if (m_selectMode == on) {
        return;
    }
    m_selectMode = on;
    auto *delegate = qobject_cast<DocumentBuilderDelegate *>(m_view->itemDelegate());
    if (delegate) {
        delegate->setSelectMode(on);
        delegate->setSelectedEntryIds(m_selectedEntryIds);
    }
    m_selectionBar->setVisible(on);
    m_selectionBar->setSelectionCount(m_selectedEntryIds.size());
    // When leaving select mode, clear the selection set.
    if (!on) {
        m_selectedEntryIds.clear();
    }
    m_view->viewport()->update();
    updateSelectionActions();
}

void DocumentBuilderWidget::toggleEntry(const QUuid &id)
{
    if (m_selectedEntryIds.contains(id)) {
        m_selectedEntryIds.remove(id);
    } else {
        m_selectedEntryIds.insert(id);
    }
    auto *delegate = qobject_cast<DocumentBuilderDelegate *>(m_view->itemDelegate());
    if (delegate) {
        delegate->setSelectedEntryIds(m_selectedEntryIds);
    }
    m_selectionBar->setSelectionCount(m_selectedEntryIds.size());
    m_view->viewport()->update();
}

void DocumentBuilderWidget::selectAllEntries()
{
    m_selectedEntryIds.clear();
    for (const QUuid &id : m_model->entryIds()) {
        m_selectedEntryIds.insert(id);
    }
    auto *delegate = qobject_cast<DocumentBuilderDelegate *>(m_view->itemDelegate());
    if (delegate) {
        delegate->setSelectedEntryIds(m_selectedEntryIds);
    }
    m_selectionBar->setSelectionCount(m_selectedEntryIds.size());
    m_view->viewport()->update();
}

// --- Phase 5b: export -------------------------------------------------------------------------

void DocumentBuilderWidget::exportAllPages()
{
    exportPages(QList<QUuid>());
}

void DocumentBuilderWidget::exportSelectedPages()
{
    exportPages(m_view->selectedEntryIds());
}

void DocumentBuilderWidget::saveToTarget()
{
    if (m_exporting || m_saveTarget.isEmpty() || m_model->pageCount() == 0) {
        return;
    }

    QString reason;
    if (!DocumentBuilderExport::canExport(&reason)) {
        KMessageBox::error(this, reason, i18n("Save Not Available"));
        return;
    }

    const QList<DocumentBuilderExport::Page> pages = m_model->pagesForExport(QList<QUuid>());
    if (pages.isEmpty()) {
        return;
    }
    if (!confirmSourceChanges()) {
        return;
    }

    // Save is the one action that deliberately replaces one of its own sources, so it gets its own
    // confirmation that says exactly what is replaced and what is lost (§10.3).
    const auto answer = KMessageBox::warningContinueCancel(
        this,
        i18n("This replaces “%1” with the %2 pages in the builder.\n\nThe document is reloaded afterwards and the builder closes. Unsaved changes to the open document are lost.",
             QFileInfo(m_saveTarget).fileName(),
             m_model->pageCount()),
        i18n("Save to Document"),
        KGuiItem(i18n("Replace Document"), QStringLiteral("document-save")),
        KStandardGuiItem::cancel());
    if (answer != KMessageBox::Continue) {
        return;
    }

    QString error;
    bool ok = false;
    {
        // Same non-cancelable busy state as Export: the writer runs on this thread.
        setExportBusy(true);
        const auto restore = qScopeGuard([this] { setExportBusy(false); });
        ok = DocumentBuilderExport::exportPdf(pages, m_saveTarget, &error, /*allowSourceOverwrite=*/true);
    }

    if (!ok) {
        KMessageBox::error(this, error.isEmpty() ? i18n("The document could not be saved.") : error, i18n("Save Failed"));
        return;
    }

    // The part reloads the replaced document; the builder cannot keep referring to page numbers of
    // a file whose contents it just changed.
    Q_EMIT documentSaved(m_saveTarget);
}

void DocumentBuilderWidget::updateExportActions()
{
    QString reason;
    const bool available = DocumentBuilderExport::canExport(&reason);
    const bool hasPages = m_model->pageCount() > 0;

    // Save only exists when the builder was opened from a real local PDF, and only then does it
    // write in place (§10.3). Export is always available for every other destination.
    if (m_saveButton) {
        m_saveButton->setVisible(!m_saveTarget.isEmpty());
        m_saveButton->setEnabled(available && hasPages && !m_exporting && !m_saveTarget.isEmpty());
        if (!available) {
            m_saveButton->setToolTip(reason);
        } else if (!hasPages) {
            m_saveButton->setToolTip(i18n("Add pages before saving."));
        } else if (!m_saveTarget.isEmpty()) {
            m_saveButton->setToolTip(i18n("Replace “%1” with the built pages.", QFileInfo(m_saveTarget).fileName()));
        }
    }
    if (m_exportButton) {
        m_exportButton->setEnabled(available && hasPages && !m_exporting);
        if (!available) {
            m_exportButton->setToolTip(reason);
        } else if (!hasPages) {
            m_exportButton->setToolTip(i18n("Add pages before exporting."));
        } else {
            m_exportButton->setToolTip(i18n("Save the built document as a PDF."));
        }
    }
    if (m_exportAllAction) {
        m_exportAllAction->setEnabled(available && hasPages && !m_exporting);
    }
    if (m_exportSelectedAction) {
        m_exportSelectedAction->setEnabled(available && !m_view->selectedEntryRows().isEmpty() && !m_exporting);
    }
}

bool DocumentBuilderWidget::confirmSourceChanges()
{
    if (!m_model->anySourceChangedOnDisk()) {
        return true;
    }

    // §3.2 rule 7: the export reads the bytes on disk now, so say so rather than export stale-looking
    // content silently.
    const auto answer = KMessageBox::warningContinueCancel(this,
                                                           i18n("One or more of the files this document is built from changed on disk after they were added. "
                                                                "The exported pages will use their current contents."),
                                                           i18n("Source Files Changed"),
                                                           KGuiItem(i18n("Export Anyway")),
                                                           KStandardGuiItem::cancel(),
                                                           QStringLiteral("documentBuilderSourceChangedWarning"));
    return answer == KMessageBox::Continue;
}

QString DocumentBuilderWidget::suggestedExportPath() const
{
    QString base = m_baseName;
    if (base.isEmpty()) {
        const QStringList sources = m_model->sourcePaths();
        for (const QString &source : sources) {
            const QString name = QFileInfo(source).completeBaseName();
            if (!name.isEmpty()) {
                base = name;
                break;
            }
        }
    }
    if (base.isEmpty()) {
        base = i18n("document");
    }

    const QString fileName = base + QStringLiteral("-built.pdf");
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return directory.isEmpty() ? fileName : QDir(directory).filePath(fileName);
}

void DocumentBuilderWidget::exportPages(const QList<QUuid> &selection)
{
    if (m_exporting) {
        return;
    }

    QString reason;
    if (!DocumentBuilderExport::canExport(&reason)) {
        KMessageBox::error(this, reason, i18n("Export Not Available"));
        return;
    }

    const QList<DocumentBuilderExport::Page> pages = m_model->pagesForExport(selection);
    if (pages.isEmpty()) {
        KMessageBox::information(this, i18n("There are no pages to export."), i18n("Export"));
        return;
    }

    if (!confirmSourceChanges()) {
        return;
    }

    // The save dialog confirms an overwrite by itself (no QFileDialog::DontConfirmOverwrite).
    QString outputPath = QFileDialog::getSaveFileName(this, i18n("Export Document"), suggestedExportPath(), i18n("PDF documents (*.pdf)"));
    if (outputPath.isEmpty()) {
        return;
    }
    if (!outputPath.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
        outputPath += QLatin1String(".pdf");
    }

    // Refuse an output that is one of the inputs. The export unit checks this again, so the refusal
    // does not depend on this pre-flight test.
    if (DocumentBuilderExport::outputConflictsWithAnySource(outputPath, m_model->sourcePaths())) {
        KMessageBox::error(this, i18n("“%1” is one of the files being built. Choose a different output name.", QFileInfo(outputPath).fileName()), i18n("Export Refused"));
        return;
    }

    QString error;
    bool ok = false;
    {
        // Synchronous, indeterminate and not cancelable (§2.7); the guard puts everything back.
        setExportBusy(true);
        const auto restore = qScopeGuard([this] { setExportBusy(false); });
        ok = DocumentBuilderExport::exportPdf(pages, outputPath, &error);
    }

    if (!ok) {
        KMessageBox::error(this, error.isEmpty() ? i18n("The document could not be exported.") : error, i18n("Export Failed"));
        return;
    }

    showExportSuccess(outputPath, pages.size());
    Q_EMIT addRecentFile(outputPath);
}

void DocumentBuilderWidget::showExportSuccess(const QString &path, int pageCount)
{
    if (!m_messageWidget) {
        return;
    }

    const QList<QAction *> previous = m_messageWidget->actions();
    for (QAction *action : previous) {
        m_messageWidget->removeAction(action);
        delete action;
    }

    m_messageWidget->setMessageType(KMessageWidget::Positive);
    m_messageWidget->setText(i18np("Exported %1 page to “%2”.", "Exported %1 pages to “%2”.", pageCount, QDir::toNativeSeparators(path)));

    auto *openAction = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Open"), m_messageWidget);
    connect(openAction, &QAction::triggered, this, [this, path] { Q_EMIT openExportedDocument(path); });
    m_messageWidget->addAction(openAction);

    auto *folderAction = new QAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18n("Show in Folder"), m_messageWidget);
    connect(folderAction, &QAction::triggered, this, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
    m_messageWidget->addAction(folderAction);

    m_messageWidget->animatedShow();
}

void DocumentBuilderWidget::setExportBusy(bool busy)
{
    if (busy == m_exporting) {
        return;
    }
    m_exporting = busy;

    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        // Reordering is a manual drag, so acceptDrops() doubles as its busy switch.
        m_view->setAcceptDrops(false);
        m_addButton->setEnabled(false);
        if (m_saveButton) {
            m_saveButton->setEnabled(false);
        }

        m_exportProgress = new QProgressDialog(i18n("Exporting the document…"), QString(), 0, 0, this);
        m_exportProgress->setWindowTitle(i18n("Export"));
        m_exportProgress->setWindowModality(Qt::ApplicationModal);
        m_exportProgress->setCancelButton(nullptr); // v1 cannot interrupt the writer
        m_exportProgress->setMinimumDuration(0);
        m_exportProgress->setAutoClose(false);
        m_exportProgress->setAutoReset(false);
        m_exportProgress->show();
        // Let the dialog paint once before the writer blocks. User input stays excluded, so nothing
        // can be clicked while the export runs.
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    } else {
        if (m_exportProgress) {
            m_exportProgress->close();
            m_exportProgress->deleteLater();
            m_exportProgress.clear();
        }
        m_view->setAcceptDrops(true);
        m_addButton->setEnabled(true);
        QApplication::restoreOverrideCursor();
    }

    // These restore (or lock) the buttons whose enabled state depends on the page count, the
    // selection and the busy flag.
    updateSelectionActions();
    updateCountLabel();
    updateExportActions();
}

bool DocumentBuilderWidget::isExporting() const
{
    return m_exporting;
}

// --- Phase 7: configuration and one-time warnings ---------------------------------------------

void DocumentBuilderWidget::applySettings()
{
    const DocumentBuilderConfig::Settings settings = DocumentBuilderConfig::loadSettings();
    m_view->setColumnCount(settings.columns);
    m_view->setTileScale(DocumentBuilderConfig::thumbnailScale(settings.thumbnailSize));
    m_view->setShowFileNames(settings.showFileNames);
}

void DocumentBuilderWidget::showConfigureMenu(const QPoint &globalPos)
{
    const DocumentBuilderConfig::Settings settings = DocumentBuilderConfig::loadSettings();

    QMenu menu(this);

    auto *columnsGroup = new QActionGroup(&menu);
    columnsGroup->setExclusive(true);
    QList<QPair<QAction *, int>> columnActions;
    {
        QAction *action = menu.addAction(i18n("Columns: Auto"));
        action->setCheckable(true);
        action->setChecked(settings.columns == DocumentBuilderConfig::kAutoColumns);
        columnsGroup->addAction(action);
        columnActions.append({action, DocumentBuilderConfig::kAutoColumns});
    }
    for (int columns = DocumentBuilderConfig::kMinColumns; columns <= DocumentBuilderConfig::kMaxColumns; ++columns) {
        QAction *action = menu.addAction(i18np("%1 column", "%1 columns", columns));
        action->setCheckable(true);
        action->setChecked(settings.columns == columns);
        columnsGroup->addAction(action);
        columnActions.append({action, columns});
    }

    menu.addSeparator();

    auto *sizeGroup = new QActionGroup(&menu);
    sizeGroup->setExclusive(true);
    QAction *smallAction = menu.addAction(i18n("Small Thumbnails"));
    smallAction->setCheckable(true);
    smallAction->setChecked(settings.thumbnailSize == DocumentBuilderConfig::ThumbnailSize::Small);
    sizeGroup->addAction(smallAction);
    QAction *mediumAction = menu.addAction(i18n("Medium Thumbnails"));
    mediumAction->setCheckable(true);
    mediumAction->setChecked(settings.thumbnailSize == DocumentBuilderConfig::ThumbnailSize::Medium);
    sizeGroup->addAction(mediumAction);
    QAction *largeAction = menu.addAction(i18n("Large Thumbnails"));
    largeAction->setCheckable(true);
    largeAction->setChecked(settings.thumbnailSize == DocumentBuilderConfig::ThumbnailSize::Large);
    sizeGroup->addAction(largeAction);

    menu.addSeparator();

    QAction *namesAction = menu.addAction(i18n("Show File Names"));
    namesAction->setCheckable(true);
    namesAction->setChecked(settings.showFileNames);

    if (menu.exec(globalPos) == nullptr) {
        return;
    }

    // One read-back path for all three groups: whatever is checked now is the new state.
    DocumentBuilderConfig::Settings updated = settings;
    for (const auto &pair : std::as_const(columnActions)) {
        if (pair.first->isChecked()) {
            updated.columns = pair.second;
        }
    }
    if (smallAction->isChecked()) {
        updated.thumbnailSize = DocumentBuilderConfig::ThumbnailSize::Small;
    } else if (largeAction->isChecked()) {
        updated.thumbnailSize = DocumentBuilderConfig::ThumbnailSize::Large;
    } else {
        updated.thumbnailSize = DocumentBuilderConfig::ThumbnailSize::Medium;
    }
    updated.showFileNames = namesAction->isChecked();

    if (updated == settings) {
        return;
    }
    DocumentBuilderConfig::saveSettings(updated);
    applySettings();
}

void DocumentBuilderWidget::maybeWarnAboutLargeImage(const QString &path)
{
    if (m_largeImageWarningShown || !m_messageWidget) {
        return;
    }
    const qint64 pixels = imagePixelCount(path);
    if (pixels < kLargeImagePixels) {
        return;
    }
    m_largeImageWarningShown = true;
    m_messageWidget->setMessageType(KMessageWidget::Warning);
    m_messageWidget->setText(i18n("“%1” has more than 100 megapixels and is stored at its full resolution, which can make the exported PDF very large.",
                                  QFileInfo(path).fileName()));
    m_messageWidget->animatedShow();
}

void DocumentBuilderWidget::updateCountLabel()
{
    const int count = m_model->pageCount();
    m_countLabel->setText(i18np("%1 page", "%1 pages", count));
    m_clearButton->setEnabled(count > 0 && !m_exporting);
    m_emptyLabel->setVisible(count == 0);
    updateExportActions();
    Q_EMIT pageCountChanged(count);
}

bool DocumentBuilderWidget::seedFromPdf(const QString &path)
{
    return seedFromPages(path, QList<int>());
}

bool DocumentBuilderWidget::seedFromPages(const QString &path, const QList<int> &pages)
{
    if (m_seeded) {
        return false;
    }
    m_seeded = true;

    const QFileInfo info(path);

    QString error;
    const bool seeded = m_model->addPdf(path, &error, pages) >= 0;
    if (seeded) {
        // Only a document that actually seeded the builder gets named; a non-PDF stays an empty
        // builder with the generic hint.
        m_baseName = info.completeBaseName();
        if (m_fileLabel) {
            m_fileLabel->setText(i18n("from “%1”", info.fileName()));
        }
    }
    updateCountLabel();
    return seeded;
}

bool DocumentBuilderWidget::isSeeded() const
{
    return m_seeded;
}

int DocumentBuilderWidget::pageCount() const
{
    return m_model->pageCount();
}

void DocumentBuilderWidget::setSaveTarget(const QString &path)
{
    m_saveTarget = path;
    updateExportActions();
}

QString DocumentBuilderWidget::saveTargetPath() const
{
    return m_saveTarget;
}

QList<QUuid> DocumentBuilderWidget::selectedEntryIds() const
{
    return m_view->selectedEntryIds();
}

void DocumentBuilderWidget::clear()
{
    m_model->clear();
    updateCountLabel();
    updateSelectionActions();
}

void DocumentBuilderWidget::reset()
{
    clear();
    m_seeded = false;
    m_baseName.clear();
    m_saveTarget.clear();
    if (m_fileLabel) {
        m_fileLabel->setText(QString());
    }
    updateExportActions();
}
