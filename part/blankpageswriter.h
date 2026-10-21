/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#ifndef _OKULAR_BLANK_PAGES_WRITER_H_
#define _OKULAR_BLANK_PAGES_WRITER_H_

#include "okularpart_export.h"

#include <QColor>
#include <QSizeF>
#include <QString>

namespace Okular
{

/**
 * Writes a brand-new PDF made of @p pageCount blank pages of @p pageSize points, each filled with
 * @p background.
 *
 * The destination is replaced atomically: when anything fails, a file already present at
 * @p fileName is left untouched. The result is an ordinary PDF, so every other Charta feature
 * works on it unchanged.
 *
 * New documents are written to "<temp>/charta-untitled-<uuid>/Untitled.pdf": the directory is
 * private to that one document, and the file name is what the tab and the window caption show
 * while the document has no location. Part::cleanupUntitledTempFile() relies on that shape when
 * deciding whether a directory may be removed.
 *
 * @param fileName    destination file
 * @param pageCount   number of pages, must be at least 1
 * @param pageSize    page size in points (1/72 inch)
 * @param background  page fill color; an invalid color means white
 * @param errorText   optional; set to a human readable reason when the function returns false
 * @return true on success
 */
OKULARPART_EXPORT bool writeBlankPagesPdf(const QString &fileName, int pageCount, const QSizeF &pageSize, const QColor &background, QString *errorText = nullptr);

} // namespace Okular

#endif // _OKULAR_BLANK_PAGES_WRITER_H_
