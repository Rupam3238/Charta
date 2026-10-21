/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

#include "pagerangeinputdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>

namespace
{
/// Splits "1 - 4" into ("1","4"); returns false when there is no usable dash.
bool splitRange(const QString &token, QString *first, QString *last)
{
    const int dash = token.indexOf(QLatin1Char('-'));
    if (dash < 0) {
        return false;
    }
    *first = token.left(dash).trimmed();
    *last = token.mid(dash + 1).trimmed();
    return true;
}
}

PageRangeInputDialog::PageRangeInputDialog(int pageCount, QWidget *parent)
    : QDialog(parent)
    , m_pageCount(pageCount)
{
    setObjectName(QStringLiteral("pageRangeInputDialog"));
    setWindowTitle(i18n("Add Pages from PDF"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    auto *hint = new QLabel(i18np("This document has one page.", "This document has %1 pages.", m_pageCount), this);
    layout->addWidget(hint);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("pageRangeEdit"));
    m_edit->setPlaceholderText(QStringLiteral("1-4,7,9-12"));
    form->addRow(i18n("Pages:"), m_edit);
    layout->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("pageRangeError"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #c0392b;"));
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(i18n("Add Pages"));
    m_okButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &PageRangeInputDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // The complaint goes away as soon as the user starts fixing the expression.
    connect(m_edit, &QLineEdit::textChanged, m_errorLabel, &QLabel::hide);

    m_edit->setFocus();
}

QList<int> PageRangeInputDialog::parsePages(const QString &text, int pageCount, QString *errorText)
{
    const auto fail = [errorText](const QString &message) {
        if (errorText) {
            *errorText = message;
        }
        return QList<int>();
    };

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return fail(i18n("Enter a page range, for example “1-4,7”."));
    }

    if (pageCount <= 0) {
        return fail(i18n("This document has no pages."));
    }

    QList<int> pages;
    const QStringList tokens = trimmed.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &rawToken : tokens) {
        const QString token = rawToken.trimmed();
        if (token.isEmpty()) {
            continue; // a stray comma is not worth an error
        }

        QString firstText;
        QString lastText;
        if (splitRange(token, &firstText, &lastText)) {
            bool okFirst = false;
            bool okLast = false;
            const int first = firstText.toInt(&okFirst);
            const int last = lastText.toInt(&okLast);
            if (!okFirst || !okLast) {
                return fail(i18n("“%1” is not a page range.", token));
            }
            if (first > last) {
                return fail(i18n("“%1” is backwards: the first page must not come after the last.", token));
            }
            if (first < 1 || last > pageCount) {
                return fail(pageCount == 1 ? i18n("“%1” is out of range: this document has a single page.", token)
                                           : i18n("“%1” is out of range: this document has %2 pages.", token, pageCount));
            }
            for (int page = first; page <= last; ++page) {
                if (!pages.contains(page - 1)) {
                    pages.append(page - 1);
                }
            }
        } else {
            bool ok = false;
            const int page = token.toInt(&ok);
            if (!ok) {
                return fail(i18n("“%1” is not a page number.", token));
            }
            if (page < 1 || page > pageCount) {
                return fail(pageCount == 1 ? i18n("Page %1 is out of range: this document has a single page.", page)
                                           : i18n("Page %1 is out of range: this document has %2 pages.", page, pageCount));
            }
            if (!pages.contains(page - 1)) {
                pages.append(page - 1);
            }
        }
    }

    if (pages.isEmpty()) {
        return fail(i18n("Enter a page range, for example “1-4,7”."));
    }

    if (errorText) {
        errorText->clear();
    }
    return pages;
}

QList<int> PageRangeInputDialog::selectedPages() const
{
    return parsePages(m_edit->text(), m_pageCount, nullptr);
}

void PageRangeInputDialog::validateAndAccept()
{
    QString error;
    if (parsePages(m_edit->text(), m_pageCount, &error).isEmpty()) {
        m_errorLabel->setText(error);
        m_errorLabel->show();
        m_edit->setFocus();
        return;
    }
    m_errorLabel->hide();
    accept();
}
