/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#include "documentbuilderexport.h"

#include <config-okular.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryFile>

#include <exception>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <KLocalizedString>

#if HAVE_QPDF
// clang-format off
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
// clang-format on
#endif

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
/// A page bigger than this (in megapixels) is worth warning about: it is embedded at full resolution.
constexpr qint64 kWarnImageMegapixels = 100;

/// Refuses an output that would overwrite one of the inputs: reading and writing the same file would
/// corrupt it, and the user asked for an output, not an in-place edit.
QString conflictReason(const QString &sourcePath)
{
    return i18n("“%1” is one of the files being built. Choose a different output name.", QFileInfo(sourcePath).fileName());
}

#if HAVE_QPDF
/// Writes one image page to a throwaway PDF and returns the path, or an empty string on failure.
/// QPdfWriter is used (not QPDF) because the image must be placed with fit/margin/background rules
/// that are already expressed in points; the temporary PDF is then copied page-wise like any source.
QString writeImagePagePdf(const DocumentBuilderExport::Page &page, const QString &directory, QString *errorText)
{
    QImageReader reader(page.sourcePath);
    reader.setAutoTransform(true);
    const QSize pixelSize = reader.size();
    if (!pixelSize.isValid() || pixelSize.width() <= 0 || pixelSize.height() <= 0) {
        if (errorText) {
            *errorText = i18n("“%1” could not be read as an image.", QFileInfo(page.sourcePath).fileName());
        }
        return QString();
    }

    if (qint64(pixelSize.width()) * pixelSize.height() > kWarnImageMegapixels * 1000000) {
        // Not a failure: the plan only asks for a warning at this size. Log it so a bug report has it.
        qWarning("Document Builder: embedding an image of %dx%d pixels at full resolution", pixelSize.width(), pixelSize.height());
    }

    const QSizeF pageSize = (page.pageSize.isValid() && page.pageSize.width() > 0.0 && page.pageSize.height() > 0.0) ? page.pageSize : QSizeF(595.0, 842.0);

    QTemporaryFile temp(QDir(directory).filePath(QStringLiteral(".charta-page-XXXXXX.pdf")));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        if (errorText) {
            *errorText = i18n("A temporary file could not be created in “%1”.", QDir::toNativeSeparators(directory));
        }
        return QString();
    }
    const QString path = temp.fileName();
    temp.close();

    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(pageSize, QPageSize::Point));
        writer.setResolution(72);
        writer.setPageMargins(QMarginsF(0.0, 0.0, 0.0, 0.0));
        writer.setTitle(QFileInfo(page.sourcePath).fileName());

        QPainter painter(&writer);
        if (!painter.isActive()) {
            QFile::remove(path);
            if (errorText) {
                *errorText = i18n("A temporary PDF for “%1” could not be created.", QFileInfo(page.sourcePath).fileName());
            }
            return QString();
        }
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.fillRect(QRectF(QPointF(0.0, 0.0), pageSize), page.background);

        // Reading at the target resolution keeps the sharpest result the fit mode asks for.
        const QRectF content = DocumentBuilderExport::contentRectForImage(page, pixelSize);
        const QSize readSize(qMax(1, qRound(content.width())), qMax(1, qRound(content.height())));
        reader.setScaledSize(readSize);
        const QImage image = reader.read();
        if (!image.isNull()) {
            painter.drawImage(content, image, QRectF(image.rect()));
        }
        painter.end();
    }

    return path;
}
#endif
}

namespace DocumentBuilderExport
{
QRectF contentRectForImage(const Page &page, const QSize &imageSizePixels)
{
    const QSizeF pageSize = (page.pageSize.isValid() && page.pageSize.width() > 0.0 && page.pageSize.height() > 0.0) ? page.pageSize : QSizeF(595.0, 842.0);
    const qreal margin = qMax(0.0, page.margin);

    QRectF available(margin, margin, pageSize.width() - 2.0 * margin, pageSize.height() - 2.0 * margin);
    if (available.width() <= 0.0 || available.height() <= 0.0) {
        available = QRectF(0.0, 0.0, pageSize.width(), pageSize.height());
    }

    if (!imageSizePixels.isValid() || imageSizePixels.width() <= 0 || imageSizePixels.height() <= 0) {
        return available;
    }

    const qreal pointsPerPixel = 72.0 / qMax(1, page.dpi);
    const QSizeF natural(imageSizePixels.width() * pointsPerPixel, imageSizePixels.height() * pointsPerPixel);

    QSizeF shown = natural;
    switch (page.fit) {
    case ImageFit::OriginalSize:
        // The page was sized to the image; if it was not, the image keeps its natural size.
        shown = natural;
        break;
    case ImageFit::FillPage: {
        const qreal scale = qMax(available.width() / natural.width(), available.height() / natural.height());
        shown = QSizeF(natural.width() * scale, natural.height() * scale);
        break;
    }
    case ImageFit::FitPage:
    default: {
        const qreal scale = qMin(available.width() / natural.width(), available.height() / natural.height());
        shown = QSizeF(natural.width() * scale, natural.height() * scale);
        break;
    }
    }

    QRectF rect(0.0, 0.0, shown.width(), shown.height());
    rect.moveCenter(available.center());
    return rect;
}

QSizeF originalPageSizeForImage(const QSize &imageSizePixels, qreal margin, int dpi)
{
    if (!imageSizePixels.isValid() || imageSizePixels.width() <= 0 || imageSizePixels.height() <= 0) {
        return QSizeF();
    }
    const qreal pointsPerPixel = 72.0 / qMax(1, dpi);
    return QSizeF(imageSizePixels.width() * pointsPerPixel + 2.0 * qMax(0.0, margin), imageSizePixels.height() * pointsPerPixel + 2.0 * qMax(0.0, margin));
}

bool canExport(QString *reason)
{
#if HAVE_QPDF
    if (reason) {
        reason->clear();
    }
    return true;
#else
    if (reason) {
        *reason = i18n("This build of Charta was compiled without PDF writing support, so pages cannot be "
                       "assembled into a new document. Browsing and arranging pages still works.");
    }
    return false;
#endif
}

bool outputConflictsWithSource(const QString &outputPath, const QString &sourcePath)
{
    if (outputPath.isEmpty() || sourcePath.isEmpty()) {
        return false;
    }

    const QFileInfo outputInfo(outputPath);
    const QFileInfo sourceInfo(sourcePath);

    // Both spellings of the same file, symlinks resolved.
    const QString outputCanonical = outputInfo.canonicalFilePath();
    const QString sourceCanonical = sourceInfo.canonicalFilePath();
    if (!outputCanonical.isEmpty() && !sourceCanonical.isEmpty() && outputCanonical == sourceCanonical) {
        return true;
    }

#ifdef Q_OS_UNIX
    // A hard link, or a symlinked directory, would not show up above; the inode pair settles it.
    struct stat outputStat;
    struct stat sourceStat;
    if (::stat(QFile::encodeName(outputPath).constData(), &outputStat) == 0 && ::stat(QFile::encodeName(sourcePath).constData(), &sourceStat) == 0) {
        if (outputStat.st_dev == sourceStat.st_dev && outputStat.st_ino == sourceStat.st_ino) {
            return true;
        }
    }
#endif

    // The destination may not exist yet: compare it against the source as a name in its directory,
    // with the directory canonicalised so "./out.pdf" and a symlinked parent are caught.
    if (outputCanonical.isEmpty() && !sourceCanonical.isEmpty()) {
        const QString directoryCanonical = QFileInfo(outputInfo.absolutePath()).canonicalFilePath();
        if (!directoryCanonical.isEmpty()) {
            const QString resolved = directoryCanonical + QLatin1Char('/') + outputInfo.fileName();
            if (resolved == sourceCanonical) {
                return true;
            }
        }
    }

    return false;
}

bool outputConflictsWithAnySource(const QString &outputPath, const QList<QString> &sourcePaths)
{
    for (const QString &source : sourcePaths) {
        if (outputConflictsWithSource(outputPath, source)) {
            return true;
        }
    }
    return false;
}

bool exportPdf(const QList<Page> &pages, const QString &outputPath, QString *errorText, bool allowSourceOverwrite)
{
    const auto fail = [errorText](const QString &message) {
        if (errorText) {
            *errorText = message;
        }
        return false;
    };

    // --- 1. Validate before touching anything (§1.7). -----------------------------------------
    if (pages.isEmpty()) {
        return fail(i18n("There are no pages to export."));
    }
    if (outputPath.isEmpty()) {
        return fail(i18n("No output file was chosen."));
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.isAbsolute()) {
        return fail(i18n("“%1” is not an absolute path.", QDir::toNativeSeparators(outputPath)));
    }

    const QDir outputDirectory(outputInfo.absolutePath());
    if (!outputDirectory.exists()) {
        return fail(i18n("The directory “%1” does not exist.", QDir::toNativeSeparators(outputDirectory.absolutePath())));
    }
    if (!QFileInfo(outputDirectory.absolutePath()).isWritable()) {
        return fail(i18n("The directory “%1” is not writable.", QDir::toNativeSeparators(outputDirectory.absolutePath())));
    }
    if (outputInfo.exists() && !outputInfo.isWritable()) {
        return fail(i18n("“%1” is not writable.", QDir::toNativeSeparators(outputPath)));
    }

    // A source that vanished or was never there is a clean failure that names the file.
    for (const Page &page : pages) {
        if (page.sourcePath.isEmpty() || !QFileInfo::exists(page.sourcePath)) {
            return fail(i18n("The source file “%1” is missing, so nothing was written.",
                             QDir::toNativeSeparators(page.sourcePath.isEmpty() ? page.displayName : page.sourcePath)));
        }
        if (page.kind == BuilderKind::PdfPage && !allowSourceOverwrite && outputConflictsWithSource(outputPath, page.sourcePath)) {
            return fail(conflictReason(page.sourcePath));
        }
    }

#if !HAVE_QPDF
    return fail(i18n("This build of Charta was compiled without PDF writing support."));
#else
    // --- 2. A unique temp file in the destination directory (§1.7 rule 2). ---------------------
    QTemporaryFile temp(outputDirectory.filePath(QStringLiteral(".%1.charta-XXXXXX.pdf").arg(outputInfo.completeBaseName())));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return fail(i18n("A temporary file could not be created in “%1”.", QDir::toNativeSeparators(outputDirectory.absolutePath())));
    }
    const QString tempPath = temp.fileName();
    temp.close();

    // Everything that can fail below must remove the temp; one guard covers all exit paths.
    struct TempGuard {
        QString path;
        bool keep = false;
        ~TempGuard()
        {
            if (!keep && !path.isEmpty()) {
                QFile::remove(path);
            }
        }
    } guard{tempPath};

    QStringList pageTempFiles;
    struct PagesGuard {
        QStringList *list;
        ~PagesGuard()
        {
            for (const QString &path : std::as_const(*list)) {
                QFile::remove(path);
            }
        }
    } pageGuard{&pageTempFiles};

    try {
        // --- 3. Copy the pages. ----------------------------------------------------------------
        QPDF out;
        out.emptyPDF();
        QPDFPageDocumentHelper outHelper(out);

        // One QPDF per source, opened once: processFile + pushInheritedAttributesToPage are the
        // expensive parts, and the plan calls for them to happen exactly once per file.
        std::map<std::string, std::shared_ptr<QPDF>> cache;
        for (const Page &page : pages) {
            if (page.kind == BuilderKind::Image) {
                const QString pagePdf = writeImagePagePdf(page, outputDirectory.absolutePath(), errorText);
                if (pagePdf.isEmpty()) {
                    return false; // writeImagePagePdf filled errorText; the guards clean up
                }
                pageTempFiles.append(pagePdf);

                QPDF imageDocument;
                imageDocument.processFile(QFile::encodeName(pagePdf).constData());
                QPDFPageDocumentHelper imageHelper(imageDocument);
                const std::vector<QPDFPageObjectHelper> imagePages = imageHelper.getAllPages();
                if (imagePages.size() != 1) {
                    return fail(i18n("The image page for “%1” could not be prepared.", QFileInfo(page.sourcePath).fileName()));
                }
                outHelper.addPage(imagePages.front(), false);
                continue;
            }

            const std::string key = QFile::encodeName(page.sourcePath).toStdString();
            std::shared_ptr<QPDF> source = cache[key];
            if (!source) {
                source = std::make_shared<QPDF>();
                // The password is only ever passed to QPDF here; it is never logged or persisted.
                const std::string password = page.password.toStdString();
                if (!page.password.isEmpty()) {
                    source->processFile(key.c_str(), password.c_str());
                } else {
                    source->processFile(key.c_str());
                }
                source->pushInheritedAttributesToPage();
                cache[key] = source;
            }

            const std::vector<QPDFPageObjectHelper> sourcePages = QPDFPageDocumentHelper(*source).getAllPages();
            if (page.sourcePage < 0 || page.sourcePage >= int(sourcePages.size())) {
                return fail(i18n("Page %1 does not exist in “%2” any more.", page.sourcePage + 1, QFileInfo(page.sourcePath).fileName()));
            }
            outHelper.addPage(sourcePages.at(page.sourcePage), false);
        }

        // --- 4. Write the temp. ----------------------------------------------------------------
        {
            QPDFWriter writer(out, QFile::encodeName(tempPath).constData());
            writer.setStreamDataMode(qpdf_s_preserve);
            writer.setObjectStreamMode(qpdf_o_preserve);
            writer.write();
        }

        // --- 5. Verify by reopening the temp we just wrote. ------------------------------------
        {
            QPDF verify;
            verify.processFile(QFile::encodeName(tempPath).constData());
            const size_t written = QPDFPageDocumentHelper(verify).getAllPages().size();
            if (written != size_t(pages.size())) {
                return fail(i18n("The exported file did not contain the expected pages (%1 of %2). Nothing was written.",
                                 int(written),
                                 pages.size()));
            }
        }
    } catch (const std::exception &e) {
        return fail(i18n("The document could not be written: %1", QString::fromUtf8(e.what())));
    } catch (...) {
        return fail(i18n("The document could not be written for an unknown reason."));
    }

    // --- 6. Replace the destination atomically (§1.7 rule 4). ----------------------------------
#ifdef Q_OS_UNIX
    // ::rename on the same filesystem replaces an existing destination without a gap. QFile::rename
    // is documented not to overwrite, so it is deliberately not used here.
    if (::rename(QFile::encodeName(tempPath).constData(), QFile::encodeName(outputPath).constData()) != 0) {
        return fail(i18n("“%1” could not be replaced (the new file is ready, but moving it into place failed).",
                         QDir::toNativeSeparators(outputPath)));
    }
#else
    // Non-POSIX: remove + rename, which is documented as non-atomic. Logged so a bug report shows it.
    if (!QFile::remove(outputPath) && QFileInfo::exists(outputPath)) {
        return fail(i18n("“%1” could not be replaced.", QDir::toNativeSeparators(outputPath)));
    }
    if (!QFile::rename(tempPath, outputPath)) {
        return fail(i18n("“%1” could not be written.", QDir::toNativeSeparators(outputPath)));
    }
#endif

    guard.keep = true; // the temp *is* the destination now
    if (errorText) {
        errorText->clear();
    }
    return true;
#endif // HAVE_QPDF
}
}
