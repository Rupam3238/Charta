/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "documentbuildersource.h"

#include <config-okular.h>

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPainter>
#include <QUrl>

#include <utility>

#include "../core/document.h"
#include "../core/generator.h"
#include "../core/page.h"
#include "../gui/pagepainter.h"

#if HAVE_QPDF
// clang-format off
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
// clang-format on
#endif

namespace
{
// Upper bound on cached thumbnails per source, oldest evicted first. A 1000 page document must not
// keep 1000 full-size images alive just because the user scrolled through the grid once.
constexpr int kThumbnailCacheCap = 128;
// A thumbnail request the document cancelled (because a higher-priority request pre-empted it) never
// notifies. Treating such a pending entry as gone after this long is what lets the grid ask again
// instead of showing a permanently blank card.
constexpr qint64 kPendingTimeoutMs = 5000;
}

BuilderSource::BuilderSource(const QString &canonicalPath, QObject *parent)
    : QObject(parent)
    , m_path(canonicalPath)
{
    m_clock.start();
}

BuilderSource::~BuilderSource()
{
    if (m_document) {
        m_document->removeObserver(this);
        delete m_document;
        m_document = nullptr;
    }
}

QSizeF BuilderSource::pageSize(int page) const
{
    if (page < 0 || page >= m_pageSizes.size()) {
        return QSizeF(595.0, 842.0);
    }
    return m_pageSizes.at(page);
}

void BuilderSource::rememberFingerprint()
{
    const QFileInfo info(m_path);
    m_importedSize = info.size();
    m_importedMTime = info.lastModified().toMSecsSinceEpoch();
}

bool BuilderSource::hasChangedOnDisk() const
{
    if (m_importedMTime < 0) {
        return false;
    }
    const QFileInfo info(m_path);
    return !info.exists() || info.size() != m_importedSize || info.lastModified().toMSecsSinceEpoch() != m_importedMTime;
}

bool BuilderSource::loadMetadata(QString *errorText)
{
    return loadMetadataWithPassword(m_password, errorText);
}

bool BuilderSource::loadMetadataWithPassword(const QString &password, QString *errorText)
{
    m_password = password;
    m_pageSizes.clear();
    m_pageCount = 0;
    m_valid = false;
    m_passwordRequired = false;
    m_errorString.clear();

    if (!QFileInfo::exists(m_path)) {
        m_errorString = tr("The file does not exist.");
        if (errorText) {
            *errorText = m_errorString;
        }
        return false;
    }

#if HAVE_QPDF
    const bool ok = readMetadataWithQpdf();
#else
    const bool ok = readMetadataWithDocument();
#endif

    if (!ok) {
        if (m_errorString.isEmpty()) {
            m_errorString = tr("The file could not be read as a PDF.");
        }
        if (errorText) {
            *errorText = m_errorString;
        }
        return false;
    }

    m_pageCount = m_pageSizes.size();
    if (m_pageCount == 0) {
        m_valid = false;
        m_errorString = tr("The file has no pages.");
        if (errorText) {
            *errorText = m_errorString;
        }
        return false;
    }

    rememberFingerprint();
    m_valid = true;
    return true;
}

bool BuilderSource::readMetadataWithQpdf()
{
#if HAVE_QPDF
    QPDF pdf;
    try {
        const QByteArray pathBytes = QFile::encodeName(m_path);
        const QByteArray passwordBytes = m_password.toUtf8();
        pdf.processFile(pathBytes.constData(), m_password.isEmpty() ? nullptr : passwordBytes.constData());
        pdf.pushInheritedAttributesToPage();

        const std::vector<QPDFPageObjectHelper> pages = QPDFPageDocumentHelper(pdf).getAllPages();
        for (QPDFPageObjectHelper page : pages) {
            double width = 595.0;
            double height = 842.0;
            QPDFObjectHandle box = page.getMediaBox();
            if (box.isArray() && box.getArrayNItems() == 4) {
                const double llx = box.getArrayItem(0).getNumericValue();
                const double lly = box.getArrayItem(1).getNumericValue();
                const double urx = box.getArrayItem(2).getNumericValue();
                const double ury = box.getArrayItem(3).getNumericValue();
                width = qAbs(urx - llx);
                height = qAbs(ury - lly);
            }

            int rotate = 0;
            QPDFObjectHandle rotateHandle = page.getAttribute("/Rotate", false);
            if (!rotateHandle.isNull() && rotateHandle.isInteger()) {
                rotate = rotateHandle.getIntValue();
            }
            rotate = ((rotate % 360) + 360) % 360;

            QSizeF size(width, height);
            if (rotate == 90 || rotate == 270) {
                size.transpose();
            }
            m_pageSizes.append(size);
        }
    } catch (const QPDFExc &e) {
        const QString message = QString::fromStdString(e.what());
        if (message.contains(QStringLiteral("password"), Qt::CaseInsensitive) || message.contains(QStringLiteral("encrypt"), Qt::CaseInsensitive)) {
            m_passwordRequired = true;
            m_errorString = tr("This PDF is password protected.");
        } else {
            m_errorString = tr("The file is not a valid PDF. %1").arg(message);
        }
        return false;
    } catch (const std::exception &e) {
        m_errorString = tr("The file could not be read. %1").arg(QString::fromStdString(e.what()));
        return false;
    }
    return !m_pageSizes.isEmpty();
#else
    return readMetadataWithDocument();
#endif
}

bool BuilderSource::readMetadataWithDocument()
{
    Okular::Document document(nullptr);
    const QMimeDatabase db;
    if (document.openDocument(m_path, QUrl::fromLocalFile(m_path), db.mimeTypeForFile(m_path), m_password) != Okular::Document::OpenSuccess) {
        m_errorString = tr("The file could not be opened.");
        return false;
    }

    const uint count = document.pages();
    for (uint i = 0; i < count; ++i) {
        const Okular::Page *page = document.page(static_cast<int>(i));
        m_pageSizes.append(page ? QSizeF(page->width(), page->height()) : QSizeF(595.0, 842.0));
    }
    return !m_pageSizes.isEmpty();
}

void BuilderSource::ensureRendering()
{
    if (m_document || m_openAttempted || !m_valid) {
        return;
    }
    m_openAttempted = true;

    m_document = new Okular::Document(nullptr);
    const QMimeDatabase db;
    const Okular::Document::OpenResult result = m_document->openDocument(m_path, QUrl::fromLocalFile(m_path), db.mimeTypeForFile(m_path), m_password);
    if (result != Okular::Document::OpenSuccess) {
        delete m_document;
        m_document = nullptr;
        return;
    }
    m_document->addObserver(this);
}

void BuilderSource::requestThumbnails(const QList<QPair<int, QSize>> &pages, qreal dpr, int priority)
{
    if (!m_valid || pages.isEmpty()) {
        return;
    }

    ensureRendering();
    if (!m_document) {
        return;
    }

    QList<Okular::PixmapRequest *> requests;
    requests.reserve(pages.size());
    QList<int> ready;

    for (const auto &page : pages) {
        const int number = page.first;
        const QSize logicalSize = page.second;
        if (number < 0 || number >= m_pageCount || logicalSize.isEmpty()) {
            continue;
        }
        if (m_thumbnails.contains(number)) {
            continue;
        }

        const Okular::Page *okularPage = m_document->page(number);
        if (!okularPage) {
            continue;
        }

        const auto pending = m_pendingSizes.constFind(number);
        if (okularPage->hasPixmap(this, logicalSize.width(), logicalSize.height())) {
            // The document still has the pixmap from an earlier request: no notification will come,
            // so draw it right away.
            m_pendingSizes.insert(number, qMakePair(logicalSize, dpr));
            m_pendingSince.insert(number, m_clock.elapsed());
            ready.append(number);
            continue;
        }
        if (pending != m_pendingSizes.constEnd() && pending->first == logicalSize && isThumbnailPending(number)) {
            continue; // already in flight at exactly this size
        }

        m_pendingSizes.insert(number, qMakePair(logicalSize, dpr));
        m_pendingSince.insert(number, m_clock.elapsed());
        requests.append(new Okular::PixmapRequest(this, number, logicalSize.width(), logicalSize.height(), dpr, priority, Okular::PixmapRequest::Asynchronous));
    }

    if (!requests.isEmpty()) {
        // NoOption, not the one-argument overload: that one passes RemoveAllPrevious and would
        // delete every pending request of this source, i.e. every other page of the grid.
        m_document->requestPixmaps(requests, Okular::Document::NoOption);
    }

    for (int number : std::as_const(ready)) {
        renderThumbnail(number);
    }
}

void BuilderSource::renderThumbnail(int page)
{
    const auto it = m_pendingSizes.constFind(page);
    if (it == m_pendingSizes.constEnd()) {
        return;
    }
    const QSize logicalSize = it->first;
    const qreal dpr = it->second;
    m_pendingSizes.erase(m_pendingSizes.find(page));
    m_pendingSince.remove(page);

    const Okular::Page *okularPage = m_document ? m_document->page(page) : nullptr;
    if (!okularPage || logicalSize.isEmpty()) {
        return;
    }

    QImage image(qRound(logicalSize.width() * dpr), qRound(logicalSize.height() * dpr), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::white);

    QPainter painter(&image);
    const int flags = PagePainter::Accessibility | PagePainter::Highlights | PagePainter::Annotations;
    PagePainter::paintPageOnPainter(&painter, okularPage, this, flags, logicalSize.width(), logicalSize.height(), QRect(0, 0, logicalSize.width(), logicalSize.height()));
    painter.end();

    m_thumbnails.insert(page, image);
    if (!m_thumbnailOrder.contains(page)) {
        m_thumbnailOrder.append(page);
    }
    while (m_thumbnailOrder.size() > kThumbnailCacheCap) {
        const int oldest = m_thumbnailOrder.takeFirst();
        m_thumbnails.remove(oldest);
    }
    Q_EMIT thumbnailReady(page);
}

void BuilderSource::clearCache()
{
    m_thumbnails.clear();
    m_thumbnailOrder.clear();
    m_pendingSizes.clear();
    m_pendingSince.clear();
}

bool BuilderSource::isThumbnailPending(int page) const
{
    const auto it = m_pendingSince.constFind(page);
    if (it == m_pendingSince.constEnd()) {
        return false;
    }
    return m_clock.elapsed() - it.value() < kPendingTimeoutMs;
}

void BuilderSource::notifySetup(const QList<Okular::Page *> &pages, int setupFlags)
{
    Q_UNUSED(pages)
    Q_UNUSED(setupFlags)
}

void BuilderSource::notifyViewportChanged(bool smoothMove)
{
    Q_UNUSED(smoothMove)
}

void BuilderSource::notifyPageChanged(int page, int flags)
{
    if ((flags & Okular::DocumentObserver::Pixmap) && m_pendingSizes.contains(page)) {
        renderThumbnail(page);
    }
}

void BuilderSource::notifyPagePixmapChanged(int page, quint64 revision)
{
    Q_UNUSED(revision)
    if (m_pendingSizes.contains(page)) {
        renderThumbnail(page);
    }
}

void BuilderSource::notifyCurrentPageChanged(int previous, int current)
{
    Q_UNUSED(previous)
    Q_UNUSED(current)
}

bool BuilderSource::canUnloadPixmap(int page) const
{
    // The document is only ever used to produce thumbnails, and each finished pixmap is copied into
    // m_thumbnails immediately. Letting the document drop the originals is what keeps a 1000 page
    // source from holding 1000 full pixmaps in memory at the same time.
    Q_UNUSED(page)
    return true;
}
