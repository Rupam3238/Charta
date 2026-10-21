/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#include "blankpageswriter.h"

#include <KLocalizedString>

#include <QFileInfo>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>

namespace Okular
{

bool writeBlankPagesPdf(const QString &fileName, int pageCount, const QSizeF &pageSize, const QColor &background, QString *errorText)
{
    const auto fail = [errorText](const QString &reason) {
        if (errorText) {
            *errorText = reason;
        }
        return false;
    };

    if (pageCount < 1) {
        return fail(i18n("At least one page is needed."));
    }
    if (!pageSize.isValid() || pageSize.width() < 1.0 || pageSize.height() < 1.0) {
        return fail(i18n("The page size is not valid."));
    }

    // QSaveFile writes to a hidden temporary file and only moves it over the destination on
    // commit(), so a file already there survives every failure and there is never a moment where
    // the destination is missing.
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(file.errorString());
    }

    {
        // One device unit is one point, so the background covers exactly the page
        QPdfWriter writer(&file);
        writer.setPageSize(QPageSize(pageSize, QPageSize::Point));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        writer.setResolution(72);

        QPainter painter;
        if (!painter.begin(&writer)) {
            return fail(i18n("The document could not be created."));
        }

        const QColor fill = background.isValid() ? background : QColor(Qt::white);
        const QRect pageRect(0, 0, writer.width(), writer.height());
        for (int i = 0; i < pageCount; ++i) {
            if (i > 0 && !writer.newPage()) {
                painter.end();
                return fail(i18n("A page could not be added to the document."));
            }
            painter.fillRect(pageRect, fill);
        }

        if (!painter.end()) {
            return fail(i18n("The document could not be finished."));
        }
    } // QPdfWriter writes the catalogue and the cross reference table in its destructor

    // Any write error seen by the device makes commit() fail as well, so a truncated document never
    // replaces what is already there
    if (!file.commit()) {
        return fail(file.errorString());
    }

    if (QFileInfo(fileName).size() <= 0) {
        return fail(i18n("The document is empty."));
    }

    return true;
}

} // namespace Okular
