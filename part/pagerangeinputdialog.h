/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Document Builder: "Add pages from PDF…" asks for a page range such as "1-4,7,9-12".

    The parser is a static method so it can be unit-tested without a window.
*/

#ifndef PAGERANGEINPUTDIALOG_H
#define PAGERANGEINPUTDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

#include "okularpart_export.h"

class QLabel;
class QLineEdit;
class QPushButton;

class OKULARPART_EXPORT PageRangeInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PageRangeInputDialog(int pageCount, QWidget *parent = nullptr);

    /// The pages chosen by the user, 0-based and in the order they were written.
    QList<int> selectedPages() const;

    /// Parses a 1-based range expression ("1-4,7,9-12") into 0-based page indices.
    ///
    /// Whitespace is tolerated anywhere, surrounding commas are ignored and repeated pages are kept
    /// only once, in first-seen order. Returns an empty list and fills @p errorText when the
    /// expression is empty, malformed, reversed (e.g. "4-1") or mentions a page outside
    /// 1..@p pageCount. Never silently drops or duplicates a page.
    static QList<int> parsePages(const QString &text, int pageCount, QString *errorText);

private:
    void validateAndAccept();

    int m_pageCount = 0;
    QLineEdit *m_edit = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_okButton = nullptr;
};

#endif // PAGERANGEINPUTDIALOG_H
