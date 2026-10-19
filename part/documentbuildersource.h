/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Document Builder: one imported source file (a PDF) and everything the builder knows about it.

    A BuilderSource owns
      * the page metadata read at import time (page count + page sizes, via QPDF), and
      * lazily, a headless Okular::Document used to render thumbnails.

    It is also the DocumentObserver for that render document, so it can turn a finished pixmap
    request into a cached thumbnail. It never writes to the source file.
*/

#ifndef DOCUMENTBUILDERSOURCE_H
#define DOCUMENTBUILDERSOURCE_H

#include <QHash>
#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QSizeF>
#include <QString>

#include "../core/observer.h"

#include "okularpart_export.h"

namespace Okular
{
class Document;
class Page;
}

class OKULARPART_EXPORT BuilderSource : public QObject, public Okular::DocumentObserver
{
    Q_OBJECT

public:
    explicit BuilderSource(const QString &canonicalPath, QObject *parent = nullptr);
    ~BuilderSource() override;

    QString path() const
    {
        return m_path;
    }

    QString password() const
    {
        return m_password;
    }
    void setPassword(const QString &password)
    {
        m_password = password;
    }

    bool isValid() const
    {
        return m_valid;
    }
    QString errorString() const
    {
        return m_errorString;
    }

    int pageCount() const
    {
        return m_pageCount;
    }
    QSizeF pageSize(int page) const;

    /// Reads the page count and page sizes. Does not open a render document.
    /// Returns false and fills @p errorText on failure (missing file, not a PDF, corrupt, no pages).
    bool loadMetadata(QString *errorText);

    /// Same, but retries decryption with @p password (used for encrypted files).
    bool loadMetadataWithPassword(const QString &password, QString *errorText);

    /// True when the last failure was a wrong/missing password.
    bool passwordRequired() const
    {
        return m_passwordRequired;
    }

    /// True when the file on disk (size or modification time) differs from what was imported.
    /// Used to warn before exporting from a file that changed underneath us.
    bool hasChangedOnDisk() const;

    // --- Rendering (lazy) ---
    void ensureRendering();

    /// Requests previews for several pages at once. Each item is (page, logical size in
    /// device-independent pixels); @p dpr is only carried so the generator can render sharply.
    ///
    /// Batching is not an optimisation: Okular::Document::requestPixmaps() replaces the whole
    /// pending set of the requesting observer unless NoOption is passed, so requesting one page at
    /// a time would cancel every request in flight and leave all but the last page blank.
    void requestThumbnails(const QList<QPair<int, QSize>> &pages, qreal dpr, int priority);

    bool hasThumbnail(int page) const
    {
        return m_thumbnails.contains(page);
    }
    /// True while a request for @p page is in flight. Used so the view does not re-request it.
    /// A request that the document cancelled never notifies, so an entry older than a few seconds is
    /// reported as gone: the caller asks again instead of leaving the page blank forever.
    bool isThumbnailPending(int page) const;
    QImage thumbnail(int page) const
    {
        return m_thumbnails.value(page);
    }
    void clearCache();

    // --- Okular::DocumentObserver ---
    void notifySetup(const QList<Okular::Page *> &pages, int setupFlags) override;
    void notifyViewportChanged(bool smoothMove) override;
    void notifyPageChanged(int page, int flags) override;
    void notifyPagePixmapChanged(int page, quint64 revision) override;
    void notifyCurrentPageChanged(int previous, int current) override;
    bool canUnloadPixmap(int page) const override;

Q_SIGNALS:
    void thumbnailReady(int page);

private:
    void renderThumbnail(int page);
    bool readMetadataWithQpdf();
    bool readMetadataWithDocument();
    void rememberFingerprint();

    QString m_path;
    QString m_password;
    bool m_valid = false;
    bool m_passwordRequired = false;
    QString m_errorString;
    int m_pageCount = 0;
    QList<QSizeF> m_pageSizes;

    // Identity of the imported revision of the file, taken at import time (§2.4).
    qint64 m_importedSize = -1;
    qint64 m_importedMTime = -1;

    QHash<int, QImage> m_thumbnails;
    QList<int> m_thumbnailOrder; ///< insertion order, oldest first, for the LRU cap
    QHash<int, QPair<QSize, qreal>> m_pendingSizes;
    QHash<int, qint64> m_pendingSince; ///< m_clock value when each pending request was made
    QElapsedTimer m_clock;

    Okular::Document *m_document = nullptr;
    bool m_openAttempted = false;
};

#endif // DOCUMENTBUILDERSOURCE_H
