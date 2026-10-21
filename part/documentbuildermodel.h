/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)

    Document Builder: the ordered list of pages that make up the document being built.

    The model is the single source of truth for the builder. It stores a flat, ordered list of
    BuilderEntry items: one per page that will end up in the exported document, plus a permanent
    trailing "add pages" tile that is not part of the document.

    Entries that come from a PDF share a BuilderSource (one per source file), which owns the file
    metadata and lazily renders thumbnails. Entries that come from an image are self-contained.
*/

#ifndef DOCUMENTBUILDERMODEL_H
#define DOCUMENTBUILDERMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QUuid>
#include <QVariant>

#include "documentbuilderexport.h"
#include "okularpart_export.h"

class BuilderSource;

/// Page setup for one image page (Phase 4). An invalid @c pageSize means "derive it": A4 portrait, or
/// A4 landscape when the image is wider than tall.
struct ImagePageSettings {
    QSizeF pageSize; ///< in points; invalid -> derived from the image
    QColor background = Qt::white;
    ImageFit fit = ImageFit::FitPage;
    qreal margin = 18.0; ///< points
    int dpi = 96;
};

/// One page in the document being built.
struct BuilderEntry {
    QUuid id;
    BuilderKind kind = BuilderKind::PdfPage;
    QString sourcePath; ///< canonical path of the source file
    QString displayName; ///< file name, for the tooltip/caption
    int sourcePage = -1; ///< 0-based page index inside the source PDF (PdfPage only)
    QSizeF pageSize;     ///< in points
    QColor background = Qt::white;
    ImageFit fit = ImageFit::FitPage;
    qreal margin = 18.0;
    int dpi = 96;
};

class OKULARPART_EXPORT DocumentBuilderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ThumbnailRole = Qt::UserRole + 1,
        PageSizeRole,
        CaptionRole,
        IsAddTileRole,
        EntryIdRole,
        SourcePathRole,
        SourcePageRole,
    };

    explicit DocumentBuilderModel(QObject *parent = nullptr);
    ~DocumentBuilderModel() override;

    // --- QAbstractListModel ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;

    // --- Queries ---
    bool isEmpty() const;
    int pageCount() const;  ///< number of pages (rows minus the add tile)
    int addTileRow() const; ///< row of the trailing add tile, always pageCount()
    bool isAddTile(int row) const;
    const BuilderEntry *entryAt(int row) const;
    QList<QUuid> entryIds() const;
    QUuid entryIdAt(int row) const;
    int rowForId(const QUuid &id) const;
    QStringList sourcePaths() const;
    BuilderSource *sourceForPath(const QString &canonicalPath) const;

    /// True when any PDF source behind the current entries changed on disk since it was imported.
    /// Images are checked too. Used to warn before exporting (§3.5 / Phase 5).
    bool anySourceChangedOnDisk() const;

    // --- Import ---
    /// Imports every page of a PDF (or the given 0-based @p pages) at the end of the document.
    /// Returns the number of pages added, or -1 on failure (then @p errorText, if given, is set).
    int addPdf(const QString &path, QString *errorText, const QList<int> &pages = QList<int>());
    /// Same, with the arguments in the order the "(Add) pages from PDF" flow uses them.
    int addPdfPages(const QString &path, const QList<int> &pages, QString *errorText);

    /// Adds one image page. @p settings defaults to A4 portrait/landscape (see ImagePageSettings).
    int addImage(const QString &path, QString *errorText, const ImagePageSettings &settings = ImagePageSettings());
    int addImages(const QStringList &paths, QString *errorText);

    /// Defaults for a freshly imported image: A4 portrait, or landscape when the image is wider than
    /// tall; 18 pt margin, white background, FitPage, 96 dpi.
    static ImagePageSettings defaultImageSettings(const QSize &imageSizePixels);

    /// Re-applies page setup to the image entries whose ids are given. Row order is preserved; PDF
    /// pages are ignored. Returns the number of entries that changed.
    int setImageSettings(const QList<QUuid> &ids, const ImagePageSettings &settings);

    /// True when at least one of the given ids is an image entry.
    bool selectionHasImages(const QList<QUuid> &ids) const;

    /// Retries a source that failed because it needs a password.
    int addPdfWithPassword(const QString &path, const QString &password, QString *errorText, const QList<int> &pages = QList<int>());

    // --- Mutation ---
    void clear();

    /// Moves every entry whose id is in @p ids so that they sit together before the entry that is
    /// currently at @p beforeRow (pageCount() appends). Works for non-contiguous selections too,
    /// which moveRows() cannot express.
    bool moveEntriesBefore(const QList<QUuid> &ids, int beforeRow);

    /// Moves every selected entry one slot up (@p delta < 0) or down (@p delta > 0), swapping with
    /// its unselected neighbour. A non-contiguous selection steps item by item instead of being
    /// gathered into one block. Returns false when nothing could move.
    bool moveEntriesByStep(const QList<QUuid> &ids, int delta);

    // --- Export description ---
    QList<DocumentBuilderExport::Page> pagesForExport(const QList<QUuid> &selection = QList<QUuid>()) const;

    // --- Thumbnails ---
    void setThumbnailWidth(int width);
    int thumbnailWidth() const;

    /// True when the card already has its preview (the add tile always counts as ready).
    bool hasThumbnail(int row) const;
    /// True when a preview is neither cached nor already being generated, i.e. when asking again
    /// would actually start work.
    bool needsThumbnailRequest(int row) const;
    /// True while a preview for @p row is in flight. A cancelled request stops counting as pending
    /// after a few seconds (see BuilderSource), so the view keeps re-checking the visible rows.
    bool isThumbnailPending(int row) const;
    /// Requests previews for @p rows. @p background cards are queued behind the visible ones.
    void requestThumbnails(const QList<int> &rows, bool background = false);

Q_SIGNALS:
    /// Emitted whenever pages are added, removed, reordered or cleared.
    void countsChanged();

private:
    BuilderSource *ensureSource(const QString &canonicalPath);
    void releaseUnusedSources();
    void wireSource(BuilderSource *source);
    void ensureThumbnail(int row);
    QSize thumbnailLogicalSize(const BuilderEntry &entry) const;
    QString captionFor(const BuilderEntry &entry) const;
    int insertPdfEntries(const QString &canonicalPath, const QList<int> &pages, BuilderSource *source);
    bool sourceInUse(const QString &canonicalPath) const;
    /// Pixel size of an image as it will be displayed (EXIF orientation applied); invalid on failure.
    static QSize imageDisplaySize(const QString &path);
    /// Page size derived for ImageFit::OriginalSize, or an invalid size when it cannot be computed.
    static QSizeF originalSizePage(const QSize &imageSizePixels, qreal margin, int dpi);

    QList<BuilderEntry> m_entries;
    QHash<QString, BuilderSource *> m_sources;
    mutable QHash<QUuid, QImage> m_imageThumbnails;
    mutable QList<QUuid> m_imageThumbnailOrder; ///< insertion order, oldest first, for the LRU cap
    int m_thumbnailWidth = 150;
};

#endif // DOCUMENTBUILDERMODEL_H
