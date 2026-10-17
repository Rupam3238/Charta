/*
    SPDX-FileCopyrightText: 2008 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_SAVEINTERFACE_H_
#define _OKULAR_SAVEINTERFACE_H_

#include "../core/okularcore_export.h"

#include <QObject>

namespace Okular
{
class AnnotationProxy;
class Page;

/**
 * @short Abstract interface for saving
 *
 * This interface defines a way to save (or help saving) the document opened
 * by the Generator.
 *
 * How to use it in a custom Generator:
 * @code
    class MyGenerator : public Okular::Generator, public Okular::SaveInterface
    {
        Q_OBJECT
        Q_INTERFACES( Okular::SaveInterface )

        ...
    };
 * @endcode
 * and - of course - implementing its methods.
 */
class OKULARCORE_EXPORT SaveInterface
{
public:
    /**
     * The possible options for the saving.
     */
    enum SaveOption {
        NoOption = 0,
        SaveChanges = 1 ///< The possibility to save with the current changes to the document.
    };
    Q_DECLARE_FLAGS(SaveOptions, SaveOption)

    SaveInterface()
    {
    }

    /**
     * Destroys the save interface.
     */
    virtual ~SaveInterface()
    {
    }

    SaveInterface(const SaveInterface &) = delete;
    SaveInterface &operator=(const SaveInterface &) = delete;

    /**
     * Query for the supported saving options.
     *
     * @note NoOption is never queried
     */
    virtual bool supportsOption(SaveOption option) const = 0;

    /**
     * Save to the specified @p fileName with the specified @p options.
     */
    virtual bool save(const QString &fileName, SaveOptions options, QString *errorText) = 0;

    /**
     * Describes a page of the document as it has to be written by applyPageLayout().
     */
    struct PageLayoutEntry {
        int sourceNumber;  ///< index of the page in the file loaded by the generator, -1 for a blank page
        double width;      ///< size in points, not rotated (only used for blank pages)
        double height;     ///< size in points, not rotated (only used for blank pages)
        QColor background; ///< background color (only used for blank pages)
        const Page *page = nullptr; ///< pointer to Okular page
    };

    /**
     * Rewrites the file @p fileName (that has just been written by save()) so that its pages are the ones in
     * @p pages, in that order: pages of the original file that are not listed are dropped, entries with a
     * negative sourceNumber become new blank pages.
     * Generators that cannot add, remove or reorder pages keep the default, that fails.
     */
    virtual bool applyPageLayout(const QString &fileName, const QList<PageLayoutEntry> &pages, QString *errorText)
    {
        Q_UNUSED(fileName)
        Q_UNUSED(pages)
        if (errorText) {
            *errorText = QStringLiteral("This format cannot save added, removed or reordered pages.");
        }
        return false;
    }

    /**
     * Returns the annotation proxy. Generators can return NULL if native
     * annotations are not supported.
     *
     * @note Returning NULL is equivalent to returning an AnnotationProxy
     *       that doesn't support any capability.
     * @since 0.15 (KDE 4.9)
     */
    virtual AnnotationProxy *annotationProxy() const = 0;
};

}

Q_DECLARE_INTERFACE(Okular::SaveInterface, "org.kde.okular.SaveInterface/0.3")
Q_DECLARE_OPERATORS_FOR_FLAGS(Okular::SaveInterface::SaveOptions)

#endif
