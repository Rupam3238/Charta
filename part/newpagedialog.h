/*
    SPDX-FileCopyrightText: 2024 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _OKULAR_NEWPAGE_DIALOG_H_
#define _OKULAR_NEWPAGE_DIALOG_H_

#include <QDialog>
#include <QColor>
#include <QWidget>

namespace Okular
{

class NewPageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewPageDialog(QWidget *parent = nullptr);
    ~NewPageDialog() override;

    struct NewPageData {
        double width;
        double height;
        QColor backgroundColor;
        int count;
    };

    NewPageData data() const;

private:
    void updateColorButton();
    class Private;
    Private *const d;
};

} // namespace Okular

#endif // _OKULAR_NEWPAGE_DIALOG_H_