/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Document Builder: turning the ordered list of pages into a real PDF.

    This unit deliberately has **no GUI** so it can be driven and reasoned about on its own. It is the
    only place in `okularpart` that uses QPDF, and the link is guarded by `HAVE_QPDF` at the CMake
    level (see §1.5 of the plan): the symbol table of `okularpart.so` must not grow a second public
    copy of QPDF next to the Poppler generator's.

    Two rules from the plan are enforced here rather than promised:

      * **Atomic output** (§1.7): everything is written to a unique temp file *in the destination
        directory*, verified by reopening it, and only then `::rename`d over the destination. Any
        failure leaves the destination byte-identical, and the temp is always removed.
      * **User files are read-only** (§3.2 rule 1): a source is never opened for writing, and an output
        that resolves to a source is refused before anything is created. The only exception is the
        builder's explicit Save-over-the-open-document action, which passes
        `allowSourceOverwrite = true` and then reloads that document.
*/

#ifndef DOCUMENTBUILDEREXPORT_H
#define DOCUMENTBUILDEREXPORT_H

#include <QColor>
#include <QList>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>

#include "okularpart_export.h"

/// What one entry of the build starts from.
enum class BuilderKind {
    PdfPage,
    Image,
};

/// How an image is placed on its page.
enum class ImageFit {
    FitPage,      ///< scale the image so it fits inside the page margins
    FillPage,     ///< scale the image so it covers the page (may crop)
    OriginalSize, ///< the page is sized to the image at `dpi`, plus the margins
};

/// Export/placement metadata for one page, independent of the model objects. This is the only
/// currency between the model, the export unit and the tests.
namespace DocumentBuilderExport
{
struct Page {
    BuilderKind kind = BuilderKind::PdfPage;
    QString sourcePath;  ///< absolute, local
    QString displayName; ///< caption / diagnostics only; never used for I/O
    int sourcePage = -1; ///< 0-based page inside the source PDF
    QSizeF pageSize;     ///< points; images only (a PDF page keeps its own MediaBox)
    QColor background = Qt::white;
    ImageFit fit = ImageFit::FitPage;
    qreal margin = 18.0; ///< points; images only
    int dpi = 96;        ///< OriginalSize images
    QString password;    ///< transient: read from BuilderSource at export time, never logged
};

/// Rectangle (in points, relative to the page's top-left corner) where the image is drawn.
/// Always preserves the image aspect ratio; honours the fit mode, the margin and the dpi.
OKULARPART_EXPORT QRectF contentRectForImage(const Page &page, const QSize &imageSizePixels);

/// Page size used by ImageFit::OriginalSize: the image at @p dpi plus the margins on both sides.
/// Returns an invalid size when @p imageSizePixels is not usable.
OKULARPART_EXPORT QSizeF originalPageSizeForImage(const QSize &imageSizePixels, qreal margin, int dpi);

/// False (with a translated @p reason) when this build has no QPDF support, so the UI can disable the
/// export entry point and say why instead of failing at the last moment.
OKULARPART_EXPORT bool canExport(QString *reason = nullptr);

/// Writes @p pages, in the given order, to @p outputPath. Returns true on success; on failure
/// @p errorText holds a translated, user-facing reason and the destination is left exactly as it was.
///
/// @p allowSourceOverwrite exists for exactly one caller: the builder's Save button, which replaces
/// the document the builder was seeded from. It skips the "the output is one of the sources" refusal;
/// the caller is responsible for warning the user and for reloading that document afterwards. The
/// output is still written to a temp file and renamed into place, and all sources are only read
/// before the rename, so a source is never observed half-written.
OKULARPART_EXPORT bool exportPdf(const QList<Page> &pages, const QString &outputPath, QString *errorText = nullptr, bool allowSourceOverwrite = false);

/// True when @p outputPath resolves to @p sourcePath. Compares canonical paths, and on POSIX the
/// device/inode pair, so a symlink or a relative spelling of the same file is caught too.
OKULARPART_EXPORT bool outputConflictsWithSource(const QString &outputPath, const QString &sourcePath);

/// True when @p outputPath resolves to any of @p sourcePaths.
OKULARPART_EXPORT bool outputConflictsWithAnySource(const QString &outputPath, const QList<QString> &sourcePaths);
}

#endif // DOCUMENTBUILDEREXPORT_H
