/*
    SPDX-FileCopyrightText: 2024 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_NEWPAGE_DIALOG_H_
#define _OKULAR_NEWPAGE_DIALOG_H_

#include <QColor>
#include <QDialog>
#include <QSizeF>
#include <QWidget>

namespace Okular
{

/**
 * Dialog asking for the size, orientation, background color and number of blank pages to add.
 * All the sizes are in points (1/72 inch).
 */
class NewPageDialog : public QDialog
{
    Q_OBJECT
public:
    /// Where the new pages are inserted relative to the page the dialog was opened for.
    enum InsertPosition {
        InsertAfter,  ///< after that page
        InsertBefore, ///< before that page
    };

    /// What the dialog is used for: adding pages to the open document, or creating a new one.
    enum Mode {
        AddPages,       ///< add blank pages to the document already open in the part (the default)
        CreateDocument, ///< create a brand-new PDF made only of these pages
    };

    /**
     * @param referenceSize size (in points, as displayed) of the page next to which the new pages
     *                      are added. When valid it is offered as the default "Same as ..." size.
     * @param position      position offered by default; the user can still change it in the dialog
     * @param mode          AddPages (default) keeps the current behavior; CreateDocument hides the
     *                      Position row (there is no page to be before or after), retitles the
     *                      dialog and labels the accept button "Create"
     */
    explicit NewPageDialog(QWidget *parent = nullptr, const QSizeF &referenceSize = QSizeF(), InsertPosition position = InsertAfter, Mode mode = AddPages);
    ~NewPageDialog() override;

    struct NewPageData {
        double width;
        double height;
        QColor backgroundColor;
        int count;
        /// whether the new pages go before the reference page instead of after it
        bool insertBefore = false;
    };

    NewPageData data() const;

    void accept() override;

private:
    void updateColorButton();
    void applyPreset(int index);
    void applyOrientation(bool landscape);
    void syncControlsFromSize();
    class Private;
    Private *const d;
};

} // namespace Okular

#endif // _OKULAR_NEWPAGE_DIALOG_H_
