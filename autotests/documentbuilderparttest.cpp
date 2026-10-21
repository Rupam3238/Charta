/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)

    Tests for the part side of the Document Builder: the full-window page, its entry points (the
    Options bar button and the shell contract), the chrome it hides while it is up, and seeding it
    from the document that is currently open.

    These drive Okular::Part directly, the same way parttest does, so no window is needed.
*/

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>

#include "../part/documentbuilder.h"
#include "../part/documentbuildermodel.h"
#include "../part/imagepagesettingsdialog.h"
#include "../part/pagerangeinputdialog.h"
#include "../part/part.h"
#include "../settings.h"

class DocumentBuilderPartTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void shellContractMatchesTheSlotNames();
    void optionsBarHasABuilderButton();
    void optionsBarButtonShowsTheBuilder();
    void backButtonLeavesTheBuilder();
    void builderWithoutADocumentStartsEmpty();
    void builderIsSeededFromTheOpenDocument();
    void seedingHappensOnlyOnce();
    void chromeIsRestoredWhenTheBuilderIsLeft();
    void backOnAnEmptyBuilderAsksTheShellToCloseTheTab();
    void backOnASeededBuilderOnlyLeavesThePage();
    void closingTheDocumentResetsTheBuilder();

    // Phase 4: the two dialogs that back up "Add pages from PDF…" and "Image Page Setup…".
    void imagePageSetupDialogRoundTripsItsSettings();
    void imagePageSetupDialogSwitchesPresetAndOrientation();
    void imagePageSetupDialogLocksThePageSizeForOriginalSize();
    void pageRangeDialogRefusesBadInputAndAcceptsGoodInput();
    void imageFilterOnlyAdvertisesReadableFormats();

private:
    // Parts are deliberately never destroyed: ~Part is broken in this tree for reasons that have
    // nothing to do with this feature (parttest and untitleddocumenttest leak them for the same
    // reason). Every assertion below happens while the part is still alive.
    static Okular::Part *newPart()
    {
        return new Okular::Part(nullptr, {});
    }

    static QString dataFile(const QString &name)
    {
        return QStringLiteral(KDESRCDIR "data/") + name;
    }

    // The part's widgets do not hang off the part QObject: they are inside the viewer widget that
    // setWidget() installed (the sidebar container), so searches start from there.
    static QWidget *widgetOf(Okular::Part *part)
    {
        return part->widget();
    }

    static QStackedWidget *stackOf(Okular::Part *part)
    {
        return part->widget()->findChild<QStackedWidget *>(QStringLiteral("mainStack"));
    }
};

void DocumentBuilderPartTest::initTestCase()
{
    // Okular::Settings derives from SettingsCore, so a part cannot be constructed after a bare
    // SettingsCore has been made: initialize the derived one, exactly like parttest does.
    Okular::Settings::instance(QStringLiteral("documentbuilderparttest"));
}

void DocumentBuilderPartTest::shellContractMatchesTheSlotNames()
{
    Okular::Part *part = newPart();

    // The shell is not linked against the part: it drives the builder by name. These are the names
    // the two sides agreed on (Shell::openDocumentBuilder, Shell::connectPart).
    QVERIFY(part->metaObject()->indexOfSlot("slotOpenDocumentBuilder()") != -1);
    QVERIFY(part->metaObject()->indexOfSlot("slotCloseDocumentBuilder()") != -1);
    QVERIFY(part->metaObject()->indexOfSlot("isDocumentBuilderVisible()") != -1);
    QVERIFY(part->metaObject()->indexOfSignal("requestDocumentBuilderVisibilityChanged(bool)") != -1);

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QVERIFY(part->isDocumentBuilderVisible());
    QVERIFY(QMetaObject::invokeMethod(part, "slotCloseDocumentBuilder"));
    QVERIFY(!part->isDocumentBuilderVisible());
}

void DocumentBuilderPartTest::optionsBarHasABuilderButton()
{
    Okular::Part *part = newPart();

    auto *button = widgetOf(part)->findChild<QToolButton *>(QStringLiteral("optionsBuilderButton"));
    QVERIFY(button != nullptr);
    QVERIFY(button->toolTip().contains(QStringLiteral("Document Builder")));
    QVERIFY(!button->isCheckable());
}

void DocumentBuilderPartTest::optionsBarButtonShowsTheBuilder()
{
    Okular::Part *part = newPart();

    QStackedWidget *stack = stackOf(part);
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->currentIndex(), 0);

    auto *button = widgetOf(part)->findChild<QToolButton *>(QStringLiteral("optionsBuilderButton"));
    QVERIFY(button != nullptr);
    button->click();

    QVERIFY(part->isDocumentBuilderVisible());
    QCOMPARE(stack->currentIndex(), 1);
    QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("documentBuilder"));
}

void DocumentBuilderPartTest::backButtonLeavesTheBuilder()
{
    Okular::Part *part = newPart();

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QVERIFY(part->isDocumentBuilderVisible());

    auto *back = widgetOf(part)->findChild<QToolButton *>(QStringLiteral("documentBuilderBackButton"));
    QVERIFY(back != nullptr);
    back->click();

    QVERIFY(!part->isDocumentBuilderVisible());
    QCOMPARE(stackOf(part)->currentIndex(), 0);
}

void DocumentBuilderPartTest::builderWithoutADocumentStartsEmpty()
{
    Okular::Part *part = newPart();
    QVERIFY(!part->isDocumentBuilderVisible());

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));

    QVERIFY(part->isDocumentBuilderVisible());
    QCOMPARE(part->documentBuilderPageCount(), 0);

    // ... and the empty builder still offers its add tile.
    auto *builder = widgetOf(part)->findChild<DocumentBuilderWidget *>(QStringLiteral("documentBuilder"));
    QVERIFY(builder != nullptr);
    QVERIFY(builder->model() != nullptr);
    QCOMPARE(builder->model()->rowCount(), 1);
    QVERIFY(builder->model()->isAddTile(0));
}

void DocumentBuilderPartTest::builderIsSeededFromTheOpenDocument()
{
    Okular::Part *part = newPart();
    part->openDocument(dataFile(QStringLiteral("simple-multipage.pdf")));
    QCOMPARE(part->pages(), 40u);

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));

    QVERIFY(part->isDocumentBuilderVisible());
    QCOMPARE(part->documentBuilderPageCount(), 40);
}

void DocumentBuilderPartTest::seedingHappensOnlyOnce()
{
    Okular::Part *part = newPart();
    part->openDocument(dataFile(QStringLiteral("file1.pdf")));
    QCOMPARE(part->pages(), 1u);

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QCOMPARE(part->documentBuilderPageCount(), 1);

    QVERIFY(QMetaObject::invokeMethod(part, "slotCloseDocumentBuilder"));
    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));

    // Entering the builder again must not import the document a second time.
    QCOMPARE(part->documentBuilderPageCount(), 1);
}

void DocumentBuilderPartTest::chromeIsRestoredWhenTheBuilderIsLeft()
{
    Okular::Part *part = newPart();

    QStackedWidget *stack = stackOf(part);
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->count(), 2);

    // The document view is one page of the stack, the builder the other; the find bar and the
    // bottom bar are siblings of the stack and are hidden while the builder is up.
    auto *findBar = widgetOf(part)->findChild<QWidget *>(QStringLiteral("findBar"));
    QVERIFY(findBar != nullptr);
    auto *bottomBar = widgetOf(part)->findChild<QWidget *>(QStringLiteral("bottomBar"));
    QVERIFY(bottomBar != nullptr);

    const bool findBarWasHidden = findBar->isHidden();
    const bool bottomBarWasHidden = bottomBar->isHidden();

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QVERIFY(findBar->isHidden());
    QVERIFY(bottomBar->isHidden());

    QVERIFY(QMetaObject::invokeMethod(part, "slotCloseDocumentBuilder"));
    QCOMPARE(findBar->isHidden(), findBarWasHidden);
    QCOMPARE(bottomBar->isHidden(), bottomBarWasHidden);
}

void DocumentBuilderPartTest::backOnAnEmptyBuilderAsksTheShellToCloseTheTab()
{
    Okular::Part *part = newPart();
    QSignalSpy closeSpy(part, &Okular::Part::requestCloseEmptyDocumentBuilder);

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QVERIFY(part->isDocumentBuilderVisible());
    QCOMPARE(part->documentBuilderPageCount(), 0);

    QVERIFY(QMetaObject::invokeMethod(part, "slotCloseDocumentBuilder"));
    QVERIFY(!part->isDocumentBuilderVisible());
    QCOMPARE(closeSpy.count(), 1);
}

void DocumentBuilderPartTest::backOnASeededBuilderOnlyLeavesThePage()
{
    Okular::Part *part = newPart();
    part->openDocument(dataFile(QStringLiteral("file1.pdf")));
    QSignalSpy closeSpy(part, &Okular::Part::requestCloseEmptyDocumentBuilder);

    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QCOMPARE(part->documentBuilderPageCount(), 1);

    QVERIFY(QMetaObject::invokeMethod(part, "slotCloseDocumentBuilder"));
    QVERIFY(!part->isDocumentBuilderVisible());
    // A builder that holds a document's pages is not thrown away.
    QCOMPARE(closeSpy.count(), 0);
    QCOMPARE(part->documentBuilderPageCount(), 1);
}

void DocumentBuilderPartTest::closingTheDocumentResetsTheBuilder()
{
    Okular::Part *part = newPart();
    part->openDocument(dataFile(QStringLiteral("file1.pdf")));
    QVERIFY(QMetaObject::invokeMethod(part, "slotOpenDocumentBuilder"));
    QCOMPARE(part->documentBuilderPageCount(), 1);

    QVERIFY(part->closeUrl());

    QVERIFY(!part->isDocumentBuilderVisible());
    QCOMPARE(part->documentBuilderPageCount(), 0);
}

void DocumentBuilderPartTest::imagePageSetupDialogRoundTripsItsSettings()
{
    ImagePageSettings initial;
    initial.pageSize = QSizeF(842.0, 595.0); // A4, landscape
    initial.background = QColor(0x10, 0x20, 0x30);
    initial.fit = ImageFit::FillPage;
    initial.margin = 12.0;
    initial.dpi = 96;

    ImagePageSettingsDialog dialog(initial, QSize(1500, 300));

    // The preset and the orientation are recovered from the page size it was given.
    auto *preset = dialog.findChild<QComboBox *>(QStringLiteral("imagePagePresetCombo"));
    auto *orientation = dialog.findChild<QComboBox *>(QStringLiteral("imagePageOrientationCombo"));
    QVERIFY(preset != nullptr);
    QVERIFY(orientation != nullptr);
    QCOMPARE(preset->currentText(), QStringLiteral("A4"));
    QCOMPARE(orientation->currentIndex(), 1); // Landscape

    const ImagePageSettings result = dialog.settings();
    QCOMPARE(result.pageSize, QSizeF(842.0, 595.0));
    QCOMPARE(result.background, QColor(0x10, 0x20, 0x30));
    QVERIFY(result.fit == ImageFit::FillPage);
    QCOMPARE(result.margin, 12.0);
    QCOMPARE(result.dpi, 96);
}

void DocumentBuilderPartTest::imagePageSetupDialogSwitchesPresetAndOrientation()
{
    ImagePageSettingsDialog dialog(ImagePageSettings(), QSize(600, 600));

    auto *preset = dialog.findChild<QComboBox *>(QStringLiteral("imagePagePresetCombo"));
    auto *orientation = dialog.findChild<QComboBox *>(QStringLiteral("imagePageOrientationCombo"));
    auto *width = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("imagePageWidthSpin"));
    auto *height = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("imagePageHeightSpin"));
    QVERIFY(preset != nullptr);
    QVERIFY(orientation != nullptr);
    QVERIFY(width != nullptr);
    QVERIFY(height != nullptr);

    // Letter (index 3) turned on its side.
    preset->setCurrentIndex(3);
    orientation->setCurrentIndex(1);
    QCOMPARE(dialog.settings().pageSize, QSizeF(792.0, 612.0));
    // Named presets derive the size, so the spins are not meant to be typed in.
    QVERIFY(width->isReadOnly());

    // The bounds of 4.4 are enforced by the spins themselves.
    QCOMPARE(width->minimum(), ImagePageSettingsDialog::kMinDimension);
    QCOMPARE(width->maximum(), ImagePageSettingsDialog::kMaxDimension);

    preset->setCurrentIndex(ImagePageSettingsDialog::presets().size()); // Custom
    QVERIFY(!width->isReadOnly());
    width->setValue(10.0); // below the minimum
    QCOMPARE(dialog.settings().pageSize.width(), ImagePageSettingsDialog::kMinDimension);
}

void DocumentBuilderPartTest::imagePageSetupDialogLocksThePageSizeForOriginalSize()
{
    ImagePageSettingsDialog dialog(ImagePageSettings(), QSize(1500, 300));

    auto *fit = dialog.findChild<QComboBox *>(QStringLiteral("imagePageFitCombo"));
    auto *preset = dialog.findChild<QComboBox *>(QStringLiteral("imagePagePresetCombo"));
    auto *width = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("imagePageWidthSpin"));
    QVERIFY(fit != nullptr);
    QVERIFY(preset != nullptr);
    QVERIFY(width != nullptr);

    fit->setCurrentIndex(2); // Original size
    QVERIFY(dialog.settings().fit == ImageFit::OriginalSize);
    // The page follows the image, so offering a page size here would be misleading.
    QVERIFY(!preset->isEnabled());
    QVERIFY(!width->isEnabled());
}

void DocumentBuilderPartTest::pageRangeDialogRefusesBadInputAndAcceptsGoodInput()
{
    PageRangeInputDialog dialog(7);

    auto *edit = dialog.findChild<QLineEdit *>(QStringLiteral("pageRangeEdit"));
    auto *error = dialog.findChild<QLabel *>(QStringLiteral("pageRangeError"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY(edit != nullptr);
    QVERIFY(error != nullptr);
    QVERIFY(buttons != nullptr);
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    QVERIFY(ok != nullptr);
    QVERIFY(error->isHidden());

    QSignalSpy acceptedSpy(&dialog, &QDialog::accepted);

    edit->setText(QStringLiteral("3-1"));
    ok->click();
    QCOMPARE(acceptedSpy.count(), 0); // still open
    QVERIFY(!error->isHidden());
    QVERIFY(!error->text().isEmpty());

    // Typing again clears the complaint ...
    edit->setText(QStringLiteral("1-3,7"));
    QVERIFY(error->isHidden());

    // ... and a good range is accepted, 0-based, in the order it was written.
    ok->click();
    QCOMPARE(acceptedSpy.count(), 1);
    QCOMPARE(dialog.selectedPages(), QList<int>() << 0 << 1 << 2 << 6);

    // A page past the end is refused: the document only has 7.
    edit->setText(QStringLiteral("8"));
    ok->click();
    QCOMPARE(acceptedSpy.count(), 1);
    QVERIFY(error->text().contains(QStringLiteral("7")));
}

void DocumentBuilderPartTest::imageFilterOnlyAdvertisesReadableFormats()
{
    const QString filter = DocumentBuilderWidget::imageFileFilter();

    // Every format the installed Qt image plugins report must be offered ...
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    QVERIFY(!formats.isEmpty());
    for (const QByteArray &format : formats) {
        QVERIFY2(filter.contains(QStringLiteral("*.") + QString::fromLatin1(format).toLower()),
                 qPrintable(QStringLiteral("filter is missing %1: %2").arg(QString::fromLatin1(format), filter)));
    }

    // ... and nothing may be advertised that no plugin can read.
    const QRegularExpression pattern(QStringLiteral("\\*\\.([a-z0-9]+)"));
    auto matches = pattern.globalMatch(filter);
    int checked = 0;
    while (matches.hasNext()) {
        const QString extension = matches.next().captured(1);
        if (extension == QLatin1String("pdf")) {
            continue; // a separate entry point handles PDFs
        }
        ++checked;
        QVERIFY2(formats.contains(extension.toLatin1()), qPrintable(extension + QStringLiteral(" is advertised but unreadable")));
    }
    QVERIFY(checked > 0);
}

QTEST_MAIN(DocumentBuilderPartTest)

#include "documentbuilderparttest.moc"
