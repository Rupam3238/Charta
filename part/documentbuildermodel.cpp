/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#include "documentbuildermodel.h"

#include <QApplication>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QMimeData>
#include <QPainter>
#include <QSet>

#include <utility>

#include "documentbuildersource.h"

#include "../gui/priorities.h"

namespace
{
const char *const kEntryMimeType = "application/x-okular-document-builder-entry";

/// How many image page previews the model keeps. Mirrors BuilderSource's per-page cap: the oldest
/// entry is dropped and re-read on demand.
constexpr int kImageThumbnailCacheCap = 128;

/// True when the EXIF orientation turns the image a quarter turn, i.e. when width and height swap.
bool transformationSwapsAxes(QImageIOHandler::Transformations transformations)
{
    // Set for Rotate90, Rotate270, MirrorAndRotate90 and FlipAndRotate90, clear for the rest.
    return transformations.testFlag(QImageIOHandler::TransformationRotate90);
}

QString canonicalPathFor(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}
}

DocumentBuilderModel::DocumentBuilderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

DocumentBuilderModel::~DocumentBuilderModel()
{
    qDeleteAll(m_sources);
    m_sources.clear();
}

int DocumentBuilderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size() + 1; // + the trailing add tile
}

bool DocumentBuilderModel::isEmpty() const
{
    return m_entries.isEmpty();
}

int DocumentBuilderModel::pageCount() const
{
    return m_entries.size();
}

int DocumentBuilderModel::addTileRow() const
{
    return m_entries.size();
}

bool DocumentBuilderModel::isAddTile(int row) const
{
    return row == m_entries.size();
}

const BuilderEntry *DocumentBuilderModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries.at(row);
}

QUuid DocumentBuilderModel::entryIdAt(int row) const
{
    const BuilderEntry *entry = entryAt(row);
    return entry ? entry->id : QUuid();
}

int DocumentBuilderModel::rowForId(const QUuid &id) const
{
    if (id.isNull()) {
        return -1;
    }
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id == id) {
            return row;
        }
    }
    return -1;
}

QList<QUuid> DocumentBuilderModel::entryIds() const
{
    QList<QUuid> ids;
    ids.reserve(m_entries.size());
    for (const BuilderEntry &entry : m_entries) {
        ids.append(entry.id);
    }
    return ids;
}

QStringList DocumentBuilderModel::sourcePaths() const
{
    QStringList paths;
    for (const BuilderEntry &entry : m_entries) {
        if (!entry.sourcePath.isEmpty() && !paths.contains(entry.sourcePath)) {
            paths.append(entry.sourcePath);
        }
    }
    return paths;
}

BuilderSource *DocumentBuilderModel::sourceForPath(const QString &canonicalPath) const
{
    return m_sources.value(canonicalPath, nullptr);
}

bool DocumentBuilderModel::sourceInUse(const QString &canonicalPath) const
{
    for (const BuilderEntry &entry : m_entries) {
        if (entry.sourcePath == canonicalPath) {
            return true;
        }
    }
    return false;
}

void DocumentBuilderModel::releaseUnusedSources()
{
    const QStringList keys = m_sources.keys();
    for (const QString &key : keys) {
        if (!sourceInUse(key)) {
            delete m_sources.take(key);
        }
    }
}

void DocumentBuilderModel::wireSource(BuilderSource *source)
{
    connect(source, &BuilderSource::thumbnailReady, this, [this, source](int page) {
        for (int row = 0; row < m_entries.size(); ++row) {
            const BuilderEntry &entry = m_entries.at(row);
            if (entry.kind == BuilderKind::PdfPage && entry.sourcePath == source->path() && entry.sourcePage == page) {
                Q_EMIT dataChanged(index(row), index(row), {ThumbnailRole});
            }
        }
    });
}

BuilderSource *DocumentBuilderModel::ensureSource(const QString &canonicalPath)
{
    BuilderSource *source = m_sources.value(canonicalPath, nullptr);
    if (source) {
        return source;
    }

    source = new BuilderSource(canonicalPath, this);
    QString error;
    source->loadMetadata(&error);
    m_sources.insert(canonicalPath, source);
    wireSource(source);
    return source;
}

int DocumentBuilderModel::insertPdfEntries(const QString &canonicalPath, const QList<int> &pages, BuilderSource *source)
{
    const QString displayName = QFileInfo(canonicalPath).fileName();
    const int first = m_entries.size();

    beginInsertRows(QModelIndex(), first, first + pages.size() - 1);
    for (int page : pages) {
        BuilderEntry entry;
        entry.id = QUuid::createUuid();
        entry.kind = BuilderKind::PdfPage;
        entry.sourcePath = canonicalPath;
        entry.displayName = displayName;
        entry.sourcePage = page;
        entry.pageSize = source->pageSize(page);
        m_entries.append(entry);
    }
    endInsertRows();

    Q_EMIT countsChanged();
    return pages.size();
}

int DocumentBuilderModel::addPdf(const QString &path, QString *errorText, const QList<int> &pages)
{
    const QString canonical = canonicalPathFor(path);
    BuilderSource *source = ensureSource(canonical);
    if (!source) {
        if (errorText) {
            *errorText = tr("Could not read “%1”.").arg(QFileInfo(path).fileName());
        }
        return -1;
    }
    if (!source->isValid()) {
        if (errorText) {
            *errorText = source->errorString();
        }
        releaseUnusedSources();
        return -1;
    }

    QList<int> wanted = pages;
    if (wanted.isEmpty()) {
        for (int i = 0; i < source->pageCount(); ++i) {
            wanted.append(i);
        }
    }
    for (int page : wanted) {
        if (page < 0 || page >= source->pageCount()) {
            if (errorText) {
                *errorText = tr("Page %1 does not exist in “%2”.").arg(page + 1).arg(QFileInfo(path).fileName());
            }
            releaseUnusedSources();
            return -1;
        }
    }

    return insertPdfEntries(canonical, wanted, source);
}

int DocumentBuilderModel::addPdfWithPassword(const QString &path, const QString &password, QString *errorText, const QList<int> &pages)
{
    const QString canonical = canonicalPathFor(path);
    BuilderSource *source = ensureSource(canonical);
    QString error;
    if (!source->loadMetadataWithPassword(password, &error)) {
        if (errorText) {
            *errorText = error;
        }
        releaseUnusedSources();
        return -1;
    }

    QList<int> wanted = pages;
    if (wanted.isEmpty()) {
        for (int i = 0; i < source->pageCount(); ++i) {
            wanted.append(i);
        }
    }
    for (int page : wanted) {
        if (page < 0 || page >= source->pageCount()) {
            if (errorText) {
                *errorText = tr("Page %1 does not exist in “%2”.").arg(page + 1).arg(QFileInfo(path).fileName());
            }
            releaseUnusedSources();
            return -1;
        }
    }

    return insertPdfEntries(canonical, wanted, source);
}

int DocumentBuilderModel::addPdfPages(const QString &path, const QList<int> &pages, QString *errorText)
{
    return addPdf(path, errorText, pages);
}

QSize DocumentBuilderModel::imageDisplaySize(const QString &path)
{
    // Animated formats (GIF, WEBP, ...) contribute their first frame: the builder reads a file once
    // with QImageReader::read(), which never advances an animation, and treats it as one page.
    QImageReader reader(path);
    // EXIF-rotated photos must be measured after the rotation, or the page aspect comes out wrong.
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        return QSize();
    }
    // QImageReader::size() reports the *stored* pixel size: the EXIF orientation is only applied
    // when the image is read, so a quarter-turn has to be applied here by hand.
    QSize size = reader.size();
    if (!size.isValid()) {
        return QSize();
    }
    if (transformationSwapsAxes(reader.transformation())) {
        size.transpose();
    }
    return size;
}

QSizeF DocumentBuilderModel::originalSizePage(const QSize &imageSizePixels, qreal margin, int dpi)
{
    return DocumentBuilderExport::originalPageSizeForImage(imageSizePixels, margin, dpi);
}

ImagePageSettings DocumentBuilderModel::defaultImageSettings(const QSize &imageSizePixels)
{
    ImagePageSettings settings;
    // A4, turned on its side when the image is wider than tall.
    const bool landscape = imageSizePixels.isValid() && imageSizePixels.height() > 0 && imageSizePixels.width() > imageSizePixels.height();
    settings.pageSize = landscape ? QSizeF(842.0, 595.0) : QSizeF(595.0, 842.0);
    return settings;
}

int DocumentBuilderModel::addImage(const QString &path, QString *errorText, const ImagePageSettings &settings)
{
    const QFileInfo info(path);
    const QSize pixelSize = imageDisplaySize(path);
    if (!pixelSize.isValid() || pixelSize.width() <= 0 || pixelSize.height() <= 0) {
        if (errorText) {
            *errorText = tr("“%1” is not a supported image, or its size could not be read.").arg(info.fileName());
        }
        return -1;
    }

    ImagePageSettings effective = settings;
    if (!effective.pageSize.isValid() || effective.pageSize.width() <= 0.0 || effective.pageSize.height() <= 0.0) {
        effective.pageSize = defaultImageSettings(pixelSize).pageSize;
    }
    if (effective.fit == ImageFit::OriginalSize) {
        const QSizeF original = originalSizePage(pixelSize, effective.margin, effective.dpi);
        if (original.isValid()) {
            effective.pageSize = original;
        }
    }

    const QString canonical = canonicalPathFor(path);
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    BuilderEntry entry;
    entry.id = QUuid::createUuid();
    entry.kind = BuilderKind::Image;
    entry.sourcePath = canonical;
    entry.displayName = QFileInfo(canonical).fileName();
    entry.sourcePage = -1;
    entry.pageSize = effective.pageSize;
    entry.background = effective.background;
    entry.fit = effective.fit;
    entry.margin = effective.margin;
    entry.dpi = effective.dpi;
    m_entries.append(entry);
    endInsertRows();

    Q_EMIT countsChanged();
    return 1;
}

int DocumentBuilderModel::addImages(const QStringList &paths, QString *errorText)
{
    int added = 0;
    for (const QString &path : paths) {
        QString error;
        if (addImage(path, &error) > 0) {
            ++added;
        } else if (errorText && errorText->isEmpty()) {
            *errorText = error;
        }
    }
    return added == 0 ? -1 : added;
}

QVariant DocumentBuilderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }
    const int row = index.row();

    if (role == IsAddTileRole) {
        return row == m_entries.size();
    }

    if (row == m_entries.size()) {
        // The trailing "add pages" tile.
        switch (role) {
        case Qt::DisplayRole:
        case CaptionRole:
        case Qt::ToolTipRole:
        case Qt::AccessibleTextRole:
            return tr("Add pages…");
        case Qt::AccessibleDescriptionRole:
            return tr("Adds PDFs or images to the document being built.");
        default:
            return QVariant();
        }
    }

    const BuilderEntry &entry = m_entries.at(row);
    switch (role) {
    case Qt::DisplayRole:
    case CaptionRole:
        return captionFor(entry);
    case Qt::ToolTipRole:
        if (entry.kind == BuilderKind::PdfPage) {
            return tr("%1 — page %2").arg(entry.displayName).arg(entry.sourcePage + 1);
        }
        return entry.displayName;
    case Qt::AccessibleTextRole:
        // What a screen reader announces for a card (Phase 7.7).
        if (entry.kind == BuilderKind::PdfPage) {
            return tr("%1, page %2 of the source document").arg(entry.displayName).arg(entry.sourcePage + 1);
        }
        return tr("Image %1").arg(entry.displayName);
    case Qt::AccessibleDescriptionRole:
        return tr("Page %1 of the document being built.").arg(row + 1);
    case ThumbnailRole:
        const_cast<DocumentBuilderModel *>(this)->ensureThumbnail(row);
        if (entry.kind == BuilderKind::Image) {
            return m_imageThumbnails.value(entry.id);
        }
        if (BuilderSource *source = m_sources.value(entry.sourcePath, nullptr)) {
            return source->thumbnail(entry.sourcePage);
        }
        return QVariant();
    case PageSizeRole:
        return entry.pageSize;
    case EntryIdRole:
        return entry.id;
    case SourcePathRole:
        return entry.sourcePath;
    case SourcePageRole:
        return entry.sourcePage;
    default:
        return QVariant();
    }
}

Qt::ItemFlags DocumentBuilderModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }
    if (index.row() == m_entries.size()) {
        // The add tile is clickable but never selectable, draggable or removable.
        return Qt::ItemIsEnabled;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QString DocumentBuilderModel::captionFor(const BuilderEntry &entry) const
{
    if (entry.kind == BuilderKind::PdfPage) {
        return tr("%1 · %2").arg(entry.displayName).arg(entry.sourcePage + 1);
    }
    return entry.displayName;
}

QSize DocumentBuilderModel::thumbnailLogicalSize(const BuilderEntry &entry) const
{
    const QSizeF pageSize = entry.pageSize.isValid() && entry.pageSize.width() > 0.0 ? entry.pageSize : QSizeF(595.0, 842.0);
    const int width = qMax(32, m_thumbnailWidth);
    int height = qRound(pageSize.height() / pageSize.width() * width);
    // The lower bound is deliberately small: a very wide page must keep its (flat) aspect instead
    // of being squashed into a taller box.
    height = qBound(8, height, 4000);
    return QSize(width, height);
}

void DocumentBuilderModel::ensureThumbnail(int row)
{
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    const BuilderEntry &entry = m_entries.at(row);

    if (entry.kind == BuilderKind::Image) {
        if (m_imageThumbnails.contains(entry.id)) {
            return;
        }

        // The card shows the whole *page*, not the raw bitmap: that is what makes the aspect honest
        // and shows the fit mode and margins (4.5). The bitmap is read scaled (4.3).
        const QSize pixelSize = imageDisplaySize(entry.sourcePath);
        if (!pixelSize.isValid() || pixelSize.width() <= 0 || pixelSize.height() <= 0) {
            return;
        }

        const QSize logical = thumbnailLogicalSize(entry);
        const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
        const QSize device(qMax(1, qRound(logical.width() * dpr)), qMax(1, qRound(logical.height() * dpr)));
        const qreal pageWidth = (entry.pageSize.isValid() && entry.pageSize.width() > 0.0) ? entry.pageSize.width() : 595.0;
        const qreal scale = logical.width() / pageWidth; // device-independent pixels per point

        DocumentBuilderExport::Page page;
        page.kind = BuilderKind::Image;
        page.sourcePath = entry.sourcePath;
        page.sourcePage = -1;
        page.pageSize = entry.pageSize;
        page.background = entry.background;
        page.fit = entry.fit;
        page.margin = entry.margin;
        page.dpi = entry.dpi;

        const QRectF content = DocumentBuilderExport::contentRectForImage(page, pixelSize);
        QRectF target(content.x() * scale, content.y() * scale, content.width() * scale, content.height() * scale);
        target = target.intersected(QRectF(0.0, 0.0, logical.width(), logical.height()));

        QImage canvas(device, QImage::Format_ARGB32_Premultiplied);
        canvas.setDevicePixelRatio(dpr);
        canvas.fill(entry.background);

        QImageReader reader(entry.sourcePath);
        reader.setAutoTransform(true);
        const QSize readSize(qMax(1, qRound(target.width() * dpr)), qMax(1, qRound(target.height() * dpr)));
        // The scaled size is applied to the stored pixels, before the EXIF orientation is applied.
        reader.setScaledSize(transformationSwapsAxes(reader.transformation()) ? readSize.transposed() : readSize);
        const QImage image = reader.read();
        if (!image.isNull()) {
            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(target, image, QRectF(image.rect()));
            painter.end();
        }

        m_imageThumbnails.insert(entry.id, canvas);
        if (!m_imageThumbnailOrder.contains(entry.id)) {
            m_imageThumbnailOrder.append(entry.id);
        }
        // Same cap as the per-source page cache, so a builder full of images cannot grow without end.
        while (m_imageThumbnailOrder.size() > kImageThumbnailCacheCap) {
            const QUuid oldest = m_imageThumbnailOrder.takeFirst();
            m_imageThumbnails.remove(oldest);
        }
        Q_EMIT dataChanged(index(row), index(row), {ThumbnailRole});
        return;
    }

    BuilderSource *source = m_sources.value(entry.sourcePath, nullptr);
    if (!source || !source->isValid() || source->hasThumbnail(entry.sourcePage)) {
        return;
    }
    // One card at a time here: this is the on-demand path taken while painting a row. The bulk
    // path (DocumentBuilderView::fillThumbnails) requests whole batches.
    requestThumbnails(QList<int>() << row, false);
}

bool DocumentBuilderModel::hasThumbnail(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return true; // the add tile has nothing to render
    }
    const BuilderEntry &entry = m_entries.at(row);
    if (entry.kind == BuilderKind::Image) {
        return m_imageThumbnails.contains(entry.id);
    }
    const BuilderSource *source = m_sources.value(entry.sourcePath, nullptr);
    return !source || source->hasThumbnail(entry.sourcePage);
}

bool DocumentBuilderModel::needsThumbnailRequest(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }
    const BuilderEntry &entry = m_entries.at(row);
    if (entry.kind == BuilderKind::Image) {
        return !m_imageThumbnails.contains(entry.id);
    }
    const BuilderSource *source = m_sources.value(entry.sourcePath, nullptr);
    if (!source || !source->isValid()) {
        return false;
    }
    return !source->hasThumbnail(entry.sourcePage) && !source->isThumbnailPending(entry.sourcePage);
}

bool DocumentBuilderModel::isThumbnailPending(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }
    const BuilderEntry &entry = m_entries.at(row);
    if (entry.kind == BuilderKind::Image) {
        return false; // image previews are produced synchronously in ensureThumbnail()
    }
    const BuilderSource *source = m_sources.value(entry.sourcePath, nullptr);
    return source && source->isValid() && source->isThumbnailPending(entry.sourcePage);
}

void DocumentBuilderModel::requestThumbnails(const QList<int> &rows, bool background)
{
    if (rows.isEmpty()) {
        return;
    }
    const int priority = background ? THUMBNAILS_PRELOAD_PRIO : THUMBNAILS_PRIO;
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;

    // Requests have to be grouped per source: each source is a separate DocumentObserver, and a
    // batch may only contain requests of one observer.
    QHash<QString, QList<QPair<int, QSize>>> batches;
    for (int row : rows) {
        if (row < 0 || row >= m_entries.size()) {
            continue;
        }
        const BuilderEntry &entry = m_entries.at(row);
        if (entry.kind == BuilderKind::Image) {
            ensureThumbnail(row);
            continue;
        }
        BuilderSource *source = m_sources.value(entry.sourcePath, nullptr);
        if (!source || !source->isValid() || source->hasThumbnail(entry.sourcePage)) {
            continue;
        }
        batches[entry.sourcePath].append(qMakePair(entry.sourcePage, thumbnailLogicalSize(entry)));
    }

    for (auto it = batches.cbegin(); it != batches.cend(); ++it) {
        BuilderSource *source = m_sources.value(it.key(), nullptr);
        if (source) {
            source->requestThumbnails(it.value(), dpr, priority);
        }
    }
}

bool DocumentBuilderModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || count <= 0) {
        return false;
    }
    const int total = m_entries.size();
    if (row < 0 || row >= total) {
        return false;
    }
    const int end = qMin(row + count, total); // the add tile can never be removed
    const int removed = end - row;
    if (removed <= 0) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, end - 1);
    const QList<BuilderEntry> gone = m_entries.mid(row, removed);
    m_entries.remove(row, removed);
    endRemoveRows();

    for (const BuilderEntry &entry : gone) {
        m_imageThumbnails.remove(entry.id);
        m_imageThumbnailOrder.removeAll(entry.id);
    }
    releaseUnusedSources();

    Q_EMIT countsChanged();
    return true;
}

bool DocumentBuilderModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceParent.isValid() || destinationParent.isValid() || count <= 0) {
        return false;
    }
    const int total = m_entries.size();
    if (sourceRow < 0 || sourceRow + count > total) {
        return false;
    }
    if (destinationChild < 0 || destinationChild > total) {
        return false;
    }
    // The destination must not be inside the moved block, nor a no-op.
    if (destinationChild >= sourceRow && destinationChild <= sourceRow + count) {
        return false;
    }

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(), destinationChild)) {
        return false;
    }
    const QList<BuilderEntry> moved = m_entries.mid(sourceRow, count);
    m_entries.remove(sourceRow, count);
    int insertAt = destinationChild;
    if (insertAt > sourceRow) {
        insertAt -= count;
    }
    for (int i = 0; i < count; ++i) {
        m_entries.insert(insertAt + i, moved.at(i));
    }
    endMoveRows();

    Q_EMIT countsChanged();
    return true;
}

bool DocumentBuilderModel::moveEntriesBefore(const QList<QUuid> &ids, int beforeRow)
{
    if (ids.isEmpty()) {
        return false;
    }

    const QSet<QUuid> wanted(ids.constBegin(), ids.constEnd());
    QList<BuilderEntry> moving;
    QList<BuilderEntry> rest;
    moving.reserve(ids.size());
    rest.reserve(m_entries.size());
    for (const BuilderEntry &entry : std::as_const(m_entries)) {
        if (wanted.contains(entry.id)) {
            moving.append(entry);
        } else {
            rest.append(entry);
        }
    }
    if (moving.isEmpty()) {
        return false;
    }

    // beforeRow is a row of the old list. The moved entries that sit before it disappear from
    // `rest`, so the insertion point shifts left by that many.
    int insertAt = qBound(0, beforeRow, m_entries.size());
    for (int row = 0; row < insertAt; ++row) {
        if (wanted.contains(m_entries.at(row).id)) {
            --insertAt;
        }
    }
    insertAt = qBound(0, insertAt, rest.size());

    // Non-contiguous selections cannot be expressed with a single moveRows(): rebuild the flat
    // list. The caches are keyed by entry id (or source page), so nothing has to be re-rendered.
    beginResetModel();
    for (int i = 0; i < moving.size(); ++i) {
        rest.insert(insertAt + i, moving.at(i));
    }
    m_entries = rest;
    endResetModel();

    Q_EMIT countsChanged();
    return true;
}

bool DocumentBuilderModel::moveEntriesByStep(const QList<QUuid> &ids, int delta)
{
    if (ids.isEmpty() || delta == 0 || m_entries.size() < 2) {
        return false;
    }

    const QSet<QUuid> wanted(ids.constBegin(), ids.constEnd());
    QList<BuilderEntry> next = m_entries;
    bool moved = false;

    if (delta < 0) {
        for (int row = 1; row < next.size(); ++row) {
            if (wanted.contains(next.at(row).id) && !wanted.contains(next.at(row - 1).id)) {
                next.swapItemsAt(row, row - 1);
                moved = true;
            }
        }
    } else {
        // Walk backwards so a swap never moves the same entry twice in one pass.
        for (int row = next.size() - 2; row >= 0; --row) {
            if (wanted.contains(next.at(row).id) && !wanted.contains(next.at(row + 1).id)) {
                next.swapItemsAt(row, row + 1);
                moved = true;
            }
        }
    }

    if (!moved) {
        return false;
    }

    // Same reasoning as moveEntriesBefore(): the caches are keyed by entry id, so a reset costs
    // nothing but the view's reselect.
    beginResetModel();
    m_entries = next;
    endResetModel();

    Q_EMIT countsChanged();
    return true;
}

QStringList DocumentBuilderModel::mimeTypes() const
{
    return QStringList() << QString::fromLatin1(kEntryMimeType);
}

Qt::DropActions DocumentBuilderModel::supportedDropActions() const
{
    // Dragging a page only ever reorders the builder; it never copies a page.
    return Qt::MoveAction;
}

bool DocumentBuilderModel::anySourceChangedOnDisk() const
{
    for (const BuilderEntry &entry : m_entries) {
        const QFileInfo info(entry.sourcePath);
        if (info.isFile()) {
            // Images have no BuilderSource; compare against nothing and simply report them as
            // unchanged unless the file disappeared.
            continue;
        }
        if (!info.exists()) {
            return true;
        }
    }

    const QStringList keys = m_sources.keys();
    for (const QString &key : keys) {
        if (const BuilderSource *source = m_sources.value(key, nullptr)) {
            if (source->hasChangedOnDisk()) {
                return true;
            }
        }
    }
    return false;
}

QMimeData *DocumentBuilderModel::mimeData(const QModelIndexList &indexes) const
{
    QByteArray payload;
    for (const QModelIndex &index : indexes) {
        if (!index.isValid() || isAddTile(index.row())) {
            continue;
        }
        payload += m_entries.at(index.row()).id.toByteArray();
        payload += '\n';
    }
    if (payload.isEmpty()) {
        return nullptr;
    }
    auto *data = new QMimeData;
    data->setData(QString::fromLatin1(kEntryMimeType), payload);
    return data;
}

void DocumentBuilderModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_imageThumbnails.clear();
    m_imageThumbnailOrder.clear();
    endResetModel();
    releaseUnusedSources();
    Q_EMIT countsChanged();
}

int DocumentBuilderModel::setImageSettings(const QList<QUuid> &ids, const ImagePageSettings &settings)
{
    if (ids.isEmpty()) {
        return 0;
    }
    const QSet<QUuid> wanted(ids.constBegin(), ids.constEnd());
    int changed = 0;
    for (int row = 0; row < m_entries.size(); ++row) {
        BuilderEntry &entry = m_entries[row];
        if (entry.kind != BuilderKind::Image || !wanted.contains(entry.id)) {
            continue;
        }

        ImagePageSettings effective = settings;
        if (effective.fit == ImageFit::OriginalSize) {
            const QSizeF original = originalSizePage(imageDisplaySize(entry.sourcePath), effective.margin, effective.dpi);
            if (original.isValid()) {
                effective.pageSize = original;
            }
        } else if (!effective.pageSize.isValid() || effective.pageSize.width() <= 0.0 || effective.pageSize.height() <= 0.0) {
            continue; // nothing sensible to apply to a Fit/Fill page
        }

        entry.pageSize = effective.pageSize;
        entry.background = effective.background;
        entry.fit = effective.fit;
        entry.margin = effective.margin;
        entry.dpi = effective.dpi;

        // The thumbnail shows the whole page, so it has to be rendered again for the new aspect.
        m_imageThumbnails.remove(entry.id);
        m_imageThumbnailOrder.removeAll(entry.id);
        Q_EMIT dataChanged(index(row), index(row), {PageSizeRole, ThumbnailRole});
        ++changed;
    }
    return changed;
}

bool DocumentBuilderModel::selectionHasImages(const QList<QUuid> &ids) const
{
    for (const BuilderEntry &entry : m_entries) {
        if (entry.kind == BuilderKind::Image && ids.contains(entry.id)) {
            return true;
        }
    }
    return false;
}

QList<DocumentBuilderExport::Page> DocumentBuilderModel::pagesForExport(const QList<QUuid> &selection) const
{
    const QSet<QUuid> wanted(selection.constBegin(), selection.constEnd());
    QList<DocumentBuilderExport::Page> pages;
    pages.reserve(m_entries.size());
    for (const BuilderEntry &entry : m_entries) {
        if (!wanted.isEmpty() && !wanted.contains(entry.id)) {
            continue;
        }
        DocumentBuilderExport::Page page;
        page.kind = entry.kind;
        page.sourcePath = entry.sourcePath;
        page.displayName = entry.displayName;
        page.sourcePage = entry.sourcePage;
        page.pageSize = entry.pageSize;
        page.background = entry.background;
        page.fit = entry.fit;
        page.margin = entry.margin;
        page.dpi = entry.dpi;
        if (entry.kind == BuilderKind::PdfPage) {
            // The one password owner is the source; it is copied in for the duration of the write only
            // and is never stored on the entry, logged or persisted (§3.2 rule 5).
            if (BuilderSource *source = m_sources.value(entry.sourcePath, nullptr)) {
                page.password = source->password();
            }
        }
        pages.append(page);
    }
    return pages;
}

void DocumentBuilderModel::setThumbnailWidth(int width)
{
    const int clamped = qMax(32, width);
    if (clamped == m_thumbnailWidth) {
        return;
    }
    m_thumbnailWidth = clamped;

    // Thumbnails are rendered once, at the width the view asked for; a new width needs new pixels.
    // Dropping the caches makes the next data(ThumbnailRole) request a fresh render at the new size.
    m_imageThumbnails.clear();
    m_imageThumbnailOrder.clear();
    for (BuilderSource *source : std::as_const(m_sources)) {
        source->clearCache();
    }
    if (!m_entries.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(m_entries.size() - 1), {ThumbnailRole});
    }
}

int DocumentBuilderModel::thumbnailWidth() const
{
    return m_thumbnailWidth;
}

