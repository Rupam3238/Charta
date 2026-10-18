/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Tests for the Document Builder model: the ordered list of pages, source sharing, page
    metadata read through QPDF, thumbnail rendering through a headless Okular::Document, reordering
    and removal, and the export description.

    Everything is driven directly through DocumentBuilderModel, so no window is needed.
*/

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "../part/documentbuildermodel.h"
#include "../part/documentbuildersource.h"
#include "../part/pagerangeinputdialog.h"
#include "../settings_core.h"

namespace
{
/// Minimal EXIF APP1 segment carrying only the Orientation tag (see TIFF/EXIF spec).
QByteArray exifApp1(quint8 orientation)
{
    QByteArray tiff;
    tiff.append("II", 2);   // little endian
    tiff.append(char(0x2A));
    tiff.append(char(0x00)); // magic 42
    tiff.append(char(0x08));
    tiff.append(3, '\0');    // offset to IFD0 = 8

    tiff.append(char(0x01));
    tiff.append(char(0x00)); // one IFD entry
    tiff.append(char(0x12));
    tiff.append(char(0x01)); // tag 0x0112 = Orientation
    tiff.append(char(0x03));
    tiff.append(char(0x00)); // type 3 = SHORT
    tiff.append(char(0x01));
    tiff.append(3, '\0');    // count = 1
    tiff.append(char(orientation));
    tiff.append(3, '\0');    // value, left justified
    tiff.append(4, '\0');    // no next IFD

    const QByteArray payload = QByteArrayLiteral("Exif") + QByteArray(2, '\0') + tiff;
    const quint16 length = quint16(payload.size() + 2);

    QByteArray segment;
    segment.append(char(0xFF));
    segment.append(char(0xE1));
    segment.append(char(length >> 8));
    segment.append(char(length & 0xFF));
    segment.append(payload);
    return segment;
}

/// Writes a solid JPEG and injects an EXIF orientation tag into it.
bool writeJpegWithOrientation(const QString &path, const QSize &size, quint8 orientation, const QColor &color)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);

    QImageWriter writer(path, "jpeg");
    if (!writer.write(image)) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 4 || quint8(data[0]) != 0xFF || quint8(data[1]) != 0xD8) {
        return false;
    }

    // Keep a JFIF APP0 segment first when the writer emitted one, then add the EXIF segment.
    int insertAt = 2;
    if (quint8(data[2]) == 0xFF && quint8(data[3]) == 0xE0 && data.size() > 6) {
        insertAt = 4 + ((quint8(data[4]) << 8) | quint8(data[5]));
    }
    data.insert(insertAt, exifApp1(orientation));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

bool almostEqual(qreal a, qreal b, qreal epsilon = 0.01)
{
    return qAbs(a - b) <= epsilon;
}
}

class DocumentBuilderModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void emptyModelHasOnlyTheAddTile();
    void addTileIsNeverRemovable();
    void addPdfImportsAllPages();
    void addPdfImportsChosenPagesInOrder();
    void addPdfKeepsPagesOfTheSameSourceShared();
    void sourcesAreReleasedWhenNoPageUsesThem();
    void addImageCreatesOnePage();
    void addMissingOrBrokenFilesFailsCleanly();
    void encryptedPdfIsReadable();
    void moveRowsReordersPages();
    void moveRowsRejectsInvalidTargets();
    void removeRangeOnlyShrinksTheEntries();
    void countsChangedIsEmitted();
    void pagesForExportFollowsTheModelOrder();
    void pagesForExportHonoursTheSelection();
    void pagesForExportFollowsReordering();
    void sameSourceBehindTwoPathsIsSharedOnce();
    void corruptPdfIsRejected();
    void importingLeavesTheSourceFileUntouched();
    void detectsThatASourceFileChangedOnDisk();
    void rendersPdfThumbnail();
    void rendersImageThumbnail();
    void thumbnailFollowsAPageThatMovesWhileRendering();
    void supportsOnlyMoveDrops();

    // --- Phase 4: images, page ranges, per-page setup ---
    void addImageUsesTheRequestedPageSetup();
    void addImageDerivesLandscapeA4FromAWideImage();
    void originalSizePageFollowsTheImage();
    void imageContentRectPreservesAspect();
    void unreadableImageIsRejectedAndLeavesTheModelAlone();
    void imageThumbnailFollowsThePageSetup();
    void setImageSettingsTouchesOnlyTheChosenImages();
    void exifOrientationIsAppliedToImagePages();
    void pageRangeParserTable();
    void addPdfPagesAddsExactlyTheRequestedPages();

private:
    static QString dataFile(const QString &name)
    {
        return QStringLiteral(KDESRCDIR "data/") + name;
    }

    static QList<int> sourcePages(DocumentBuilderModel &model)
    {
        QList<int> pages;
        for (int row = 0; row < model.pageCount(); ++row) {
            pages.append(model.data(model.index(row, 0), DocumentBuilderModel::SourcePageRole).toInt());
        }
        return pages;
    }
};

void DocumentBuilderModelTest::initTestCase()
{
    Okular::SettingsCore::instance(QStringLiteral("documentbuildermodeltest"));
}

void DocumentBuilderModelTest::emptyModelHasOnlyTheAddTile()
{
    DocumentBuilderModel model;

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.pageCount(), 0);
    QVERIFY(model.isEmpty());
    QCOMPARE(model.addTileRow(), 0);
    QVERIFY(model.isAddTile(0));
    QVERIFY(model.data(model.index(0, 0), DocumentBuilderModel::IsAddTileRole).toBool());
    QVERIFY(model.sourcePaths().isEmpty());
}

void DocumentBuilderModelTest::addTileIsNeverRemovable()
{
    DocumentBuilderModel model;

    // The only row is the add tile, so nothing can be removed.
    QVERIFY(!model.removeRows(0, 1));
    QCOMPARE(model.rowCount(), 1);

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error), 1);
    QVERIFY(error.isEmpty());
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.isAddTile(1));

    // Asking to delete more rows than exist is clamped to the real entries: the add tile stays.
    QVERIFY(model.removeRows(0, 5));
    QCOMPARE(model.pageCount(), 0);
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.isAddTile(0));
}

void DocumentBuilderModelTest::addPdfImportsAllPages()
{
    DocumentBuilderModel model;

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error), 40);
    QVERIFY(error.isEmpty());
    QCOMPARE(model.pageCount(), 40);
    QCOMPARE(model.rowCount(), 41);
    QVERIFY(model.isAddTile(40));
    QCOMPARE(model.sourcePaths().size(), 1);
    QCOMPARE(sourcePages(model).size(), 40);
    QCOMPARE(sourcePages(model).first(), 0);
    QCOMPARE(sourcePages(model).last(), 39);

    // The page size comes from the PDF MediaBox (A4 here) and is positive.
    const QSizeF size = model.data(model.index(0, 0), DocumentBuilderModel::PageSizeRole).toSizeF();
    QVERIFY(size.isValid());
    QVERIFY(size.width() > 400.0 && size.width() < 700.0);
    QVERIFY(size.height() > 700.0 && size.height() < 1000.0);
}

void DocumentBuilderModelTest::addPdfImportsChosenPagesInOrder()
{
    DocumentBuilderModel model;

    QString error;
    // The requested order is kept, not sorted.
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file2.pdf")), &error, QList<int>() << 1 << 0), 2);
    QVERIFY(sourcePages(model) == (QList<int>() << 1 << 0));
}

void DocumentBuilderModelTest::addPdfKeepsPagesOfTheSameSourceShared()
{
    DocumentBuilderModel model;

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0), 1);
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 1), 1);

    QCOMPARE(model.pageCount(), 2);
    QCOMPARE(model.sourcePaths().size(), 1);

    const QString path = model.sourcePaths().first();
    BuilderSource *source = model.sourceForPath(path);
    QVERIFY(source != nullptr);
    QCOMPARE(source->pageCount(), 40);

    // Both entries refer to the very same BuilderSource object.
    QCOMPARE(model.sourceForPath(path), source);
}

void DocumentBuilderModelTest::sourcesAreReleasedWhenNoPageUsesThem()
{
    DocumentBuilderModel model;

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0), 1);
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 1), 1);
    const QString path = model.sourcePaths().first();

    QPointer<BuilderSource> source = model.sourceForPath(path);
    QVERIFY(!source.isNull());

    // Removing one of the two pages keeps the source alive.
    QVERIFY(model.removeRows(0, 1));
    QCOMPARE(model.sourcePaths().size(), 1);
    QVERIFY(!source.isNull());

    // Removing the last page releases (and deletes) the source.
    QVERIFY(model.removeRows(0, 1));
    QVERIFY(model.sourcePaths().isEmpty());
    QVERIFY(source.isNull());
    QVERIFY(model.sourceForPath(path) == nullptr);
}

void DocumentBuilderModelTest::addImageCreatesOnePage()
{
    DocumentBuilderModel model;

    QString error;
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error), 1);
    QVERIFY(error.isEmpty());
    QCOMPARE(model.pageCount(), 1);

    const QSizeF size = model.data(model.index(0, 0), DocumentBuilderModel::PageSizeRole).toSizeF();
    QVERIFY(size.isValid());
    QCOMPARE(model.data(model.index(0, 0), DocumentBuilderModel::SourcePageRole).toInt(), -1);

    // An image has no PDF source document behind it.
    QCOMPARE(model.sourcePaths().size(), 1);
    QVERIFY(model.sourceForPath(model.sourcePaths().first()) == nullptr);
}

void DocumentBuilderModelTest::addMissingOrBrokenFilesFailsCleanly()
{
    DocumentBuilderModel model;
    QString error;

    QCOMPARE(model.addPdf(dataFile(QStringLiteral("does-not-exist.pdf")), &error), -1);
    QVERIFY(!error.isEmpty());
    QCOMPARE(model.pageCount(), 0);

    // A page that does not exist in the file is refused as well.
    error.clear();
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error, QList<int>() << 7), -1);
    QVERIFY(!error.isEmpty());
    QCOMPARE(model.pageCount(), 0);

    // ... and no source is left behind for a failed import.
    QVERIFY(model.sourcePaths().isEmpty());
}

void DocumentBuilderModelTest::encryptedPdfIsReadable()
{
    DocumentBuilderModel model;
    QString error;

    // protected.pdf is encrypted but has an empty user password, so it opens like any other file.
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("protected.pdf")), &error), 1);
    QVERIFY(error.isEmpty());
    QCOMPARE(model.pageCount(), 1);
}

void DocumentBuilderModelTest::moveRowsReordersPages()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1 << 2), 3);
    QVERIFY(sourcePages(model) == (QList<int>() << 0 << 1 << 2));

    // Move the first page to the end.
    QVERIFY(model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3));
    QVERIFY(sourcePages(model) == (QList<int>() << 1 << 2 << 0));
    // The add tile is still the last row.
    QVERIFY(model.isAddTile(model.rowCount() - 1));

    // Move the last page back to the front.
    QVERIFY(model.moveRows(QModelIndex(), 2, 1, QModelIndex(), 0));
    QVERIFY(sourcePages(model) == (QList<int>() << 0 << 1 << 2));

    // Move a block of two pages to the end.
    QVERIFY(model.moveRows(QModelIndex(), 0, 2, QModelIndex(), 3));
    QVERIFY(sourcePages(model) == (QList<int>() << 2 << 0 << 1));
}

void DocumentBuilderModelTest::moveRowsRejectsInvalidTargets()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1 << 2), 3);

    QVERIFY(!model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 0));  // no-op
    QVERIFY(!model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 1));  // inside the moved block / no-op
    QVERIFY(!model.moveRows(QModelIndex(), 1, 2, QModelIndex(), 2));  // inside the moved block
    QVERIFY(!model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 99)); // beyond the pages
    QVERIFY(!model.moveRows(QModelIndex(), 5, 1, QModelIndex(), 0));  // beyond the pages
    QVERIFY(sourcePages(model) == (QList<int>() << 0 << 1 << 2));
}

void DocumentBuilderModelTest::countsChangedIsEmitted()
{
    DocumentBuilderModel model;
    QSignalSpy spy(&model, &DocumentBuilderModel::countsChanged);

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error), 1);
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 0) == false); // no-op, no signal
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.removeRows(0, 1));
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);
}

void DocumentBuilderModelTest::pagesForExportFollowsTheModelOrder()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1), 2);
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error), 1);

    const QList<DocumentBuilderExport::Page> pages = model.pagesForExport();
    QCOMPARE(pages.size(), 3);
    QVERIFY(pages[0].kind == BuilderKind::PdfPage);
    QCOMPARE(pages[0].sourcePage, 0);
    QCOMPARE(pages[1].sourcePage, 1);
    QVERIFY(pages[2].kind == BuilderKind::Image);
    QVERIFY(!pages[0].sourcePath.isEmpty());
    QVERIFY(!pages[0].pageSize.isEmpty());
}

void DocumentBuilderModelTest::pagesForExportHonoursTheSelection()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1 << 2), 3);

    const QUuid second = model.entryIdAt(1);
    const QUuid third = model.entryIdAt(2);
    const QList<DocumentBuilderExport::Page> pages = model.pagesForExport(QList<QUuid>() << third << second);
    QCOMPARE(pages.size(), 2);
    // The model order wins, not the order of the selection.
    QCOMPARE(pages[0].sourcePage, 1);
    QCOMPARE(pages[1].sourcePage, 2);
}

void DocumentBuilderModelTest::removeRangeOnlyShrinksTheEntries()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1 << 2 << 3 << 4), 5);

    // Remove a range from the middle: only the entries shrink, the add tile stays last.
    QVERIFY(model.removeRows(1, 2));
    QCOMPARE(model.pageCount(), 3);
    QVERIFY(sourcePages(model) == (QList<int>() << 0 << 3 << 4));
    QCOMPARE(model.rowCount(), 4);
    QVERIFY(model.isAddTile(3));
}

void DocumentBuilderModelTest::pagesForExportFollowsReordering()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1 << 2), 3);

    QVERIFY(model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3));

    const QList<DocumentBuilderExport::Page> pages = model.pagesForExport();
    QCOMPARE(pages.size(), 3);
    QCOMPARE(pages[0].sourcePage, 1);
    QCOMPARE(pages[1].sourcePage, 2);
    QCOMPARE(pages[2].sourcePage, 0);
}

void DocumentBuilderModelTest::sameSourceBehindTwoPathsIsSharedOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString link = dir.filePath(QStringLiteral("alias.pdf"));
    QVERIFY(QFile::link(dataFile(QStringLiteral("file1.pdf")), link));
    QVERIFY(QFileInfo(link).isSymLink());

    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error), 1);
    QCOMPARE(model.addPdf(link, &error), 1);

    // Both paths resolve to the same file, so there is one BuilderSource behind two pages.
    QCOMPARE(model.pageCount(), 2);
    QCOMPARE(model.sourcePaths().size(), 1);
}

void DocumentBuilderModelTest::corruptPdfIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("garbage.pdf"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not a PDF at all\n");
    file.close();

    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(path, &error), -1);
    QVERIFY(!error.isEmpty());
    QCOMPARE(model.pageCount(), 0);
    QVERIFY(model.sourcePaths().isEmpty());
}

void DocumentBuilderModelTest::importingLeavesTheSourceFileUntouched()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QStringLiteral("file1.pdf"));
    QVERIFY(QFile::copy(dataFile(QStringLiteral("file1.pdf")), copy));

    const QFileInfo before(copy);

    DocumentBuilderModel model;
    model.setThumbnailWidth(100);
    QString error;
    QCOMPARE(model.addPdf(copy, &error), 1);

    // Rendering a thumbnail opens the file read-only; nothing may have been written back.
    const QModelIndex index = model.index(0, 0);
    model.data(index, DocumentBuilderModel::ThumbnailRole);
    QTRY_VERIFY_WITH_TIMEOUT(!model.data(index, DocumentBuilderModel::ThumbnailRole).value<QImage>().isNull(), 20000);

    const QFileInfo after(copy);
    QCOMPARE(after.size(), before.size());
    QCOMPARE(after.lastModified(), before.lastModified());
}

void DocumentBuilderModelTest::detectsThatASourceFileChangedOnDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QStringLiteral("file1.pdf"));
    QVERIFY(QFile::copy(dataFile(QStringLiteral("file1.pdf")), copy));

    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addPdf(copy, &error), 1);

    BuilderSource *source = model.sourceForPath(model.sourcePaths().first());
    QVERIFY(source != nullptr);
    QVERIFY(!source->hasChangedOnDisk());
    QVERIFY(!model.anySourceChangedOnDisk());

    // Appending a byte changes the size the import recorded.
    QFile file(copy);
    QVERIFY(file.open(QIODevice::Append));
    QCOMPARE(file.write("x"), qint64(1));
    file.close();

    QVERIFY(source->hasChangedOnDisk());
    QVERIFY(model.anySourceChangedOnDisk());
}

void DocumentBuilderModelTest::supportsOnlyMoveDrops()
{
    DocumentBuilderModel model;

    // Dragging a page reorders it, it never duplicates it.
    QCOMPARE(model.supportedDropActions(), Qt::MoveAction);
    QCOMPARE(model.mimeTypes().size(), 1);

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error), 1);

    // The add tile is never draggable, so it can never be copied out.
    const QModelIndex addTile = model.index(model.addTileRow(), 0);
    QVERIFY(!(model.flags(addTile) & Qt::ItemIsDragEnabled));
    QVERIFY(!model.mimeData({addTile}));
    QVERIFY(model.mimeData({model.index(0, 0)}));
}

void DocumentBuilderModelTest::thumbnailFollowsAPageThatMovesWhileRendering()
{
    DocumentBuilderModel model;
    model.setThumbnailWidth(100);
    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("simple-multipage.pdf")), &error, QList<int>() << 0 << 1 << 2), 3);

    // Kick off the render of the last page, move that page to the front while it is in flight, then
    // let it finish: the card that holds the page must end up with the thumbnail.
    const int movedPage = model.data(model.index(2, 0), DocumentBuilderModel::SourcePageRole).toInt();
    QVERIFY(model.data(model.index(2, 0), DocumentBuilderModel::ThumbnailRole).value<QImage>().isNull());
    QVERIFY(model.moveRows(QModelIndex(), 2, 1, QModelIndex(), 0));
    QCOMPARE(model.data(model.index(0, 0), DocumentBuilderModel::SourcePageRole).toInt(), movedPage);

    QTRY_VERIFY_WITH_TIMEOUT(!model.data(model.index(0, 0), DocumentBuilderModel::ThumbnailRole).value<QImage>().isNull(), 20000);
}

void DocumentBuilderModelTest::rendersPdfThumbnail()
{
    DocumentBuilderModel model;
    model.setThumbnailWidth(120);

    QString error;
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error), 1);

    const QModelIndex index = model.index(0, 0);
    QVERIFY(model.data(index, DocumentBuilderModel::ThumbnailRole).value<QImage>().isNull());

    // Rendering happens in a worker thread through a headless Okular::Document; wait for it.
    QTRY_VERIFY_WITH_TIMEOUT(!model.data(index, DocumentBuilderModel::ThumbnailRole).value<QImage>().isNull(), 20000);

    const QImage image = model.data(index, DocumentBuilderModel::ThumbnailRole).value<QImage>();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 10);
    QVERIFY(image.height() > 10);

    // file1.pdf has text on it, so the rendered thumbnail must not be blank.
    bool nonWhite = false;
    for (int y = 0; y < image.height() && !nonWhite; y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() < 240 || pixel.green() < 240 || pixel.blue() < 240) {
                nonWhite = true;
                break;
            }
        }
    }
    QVERIFY(nonWhite);
}

void DocumentBuilderModelTest::rendersImageThumbnail()
{
    DocumentBuilderModel model;
    model.setThumbnailWidth(120);

    QString error;
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error), 1);

    const QImage image = model.data(model.index(0, 0), DocumentBuilderModel::ThumbnailRole).value<QImage>();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 10);
    // 1500x300 is wide, so the thumbnail must be wider than it is tall.
    QVERIFY(image.width() > image.height());
}

void DocumentBuilderModelTest::addImageUsesTheRequestedPageSetup()
{
    DocumentBuilderModel model;

    ImagePageSettings settings;
    settings.pageSize = QSizeF(612.0, 792.0); // US Letter
    settings.background = QColor(0x20, 0x30, 0x40);
    settings.fit = ImageFit::FillPage;
    settings.margin = 24.0;
    settings.dpi = 150;

    QString error;
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error, settings), 1);
    QVERIFY(error.isEmpty());
    QCOMPARE(model.pageCount(), 1);
    QCOMPARE(model.rowCount(), 2);

    const BuilderEntry *entry = model.entryAt(0);
    QVERIFY(entry != nullptr);
    QVERIFY(entry->kind == BuilderKind::Image);
    QCOMPARE(entry->pageSize, QSizeF(612.0, 792.0));
    QCOMPARE(entry->background, QColor(0x20, 0x30, 0x40));
    QVERIFY(entry->fit == ImageFit::FillPage);
    QCOMPARE(entry->margin, 24.0);
    QCOMPARE(entry->dpi, 150);
    QCOMPARE(entry->sourcePage, -1);

    // The image keeps its own aspect ratio inside the requested page.
    const QList<DocumentBuilderExport::Page> pages = model.pagesForExport();
    QCOMPARE(pages.size(), 1);
    const QRectF content = DocumentBuilderExport::contentRectForImage(pages.first(), QSize(1500, 300));
    QVERIFY(almostEqual(content.width() / content.height(), 1500.0 / 300.0, 1e-6));
    // Fill covers the whole available area.
    QVERIFY(content.width() >= 612.0 - 2 * 24.0 - 0.01);
    QVERIFY(content.height() >= 792.0 - 2 * 24.0 - 0.01);
}

void DocumentBuilderModelTest::addImageDerivesLandscapeA4FromAWideImage()
{
    DocumentBuilderModel model;
    QString error;

    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error), 1); // wide
    QCOMPARE(model.addImage(dataFile(QStringLiteral("300x1500.png")), &error), 1); // tall

    // A4, turned to match the image.
    QCOMPARE(model.entryAt(0)->pageSize, QSizeF(842.0, 595.0));
    QCOMPARE(model.entryAt(1)->pageSize, QSizeF(595.0, 842.0));
    // ... and the other defaults of 4.2.
    QCOMPARE(model.entryAt(0)->margin, 18.0);
    QCOMPARE(model.entryAt(0)->dpi, 96);
    QCOMPARE(model.entryAt(0)->background, QColor(Qt::white));
    QVERIFY(model.entryAt(0)->fit == ImageFit::FitPage);
}

void DocumentBuilderModelTest::originalSizePageFollowsTheImage()
{
    DocumentBuilderModel model;

    ImagePageSettings settings;
    settings.fit = ImageFit::OriginalSize;
    settings.margin = 18.0;
    settings.dpi = 96;

    QString error;
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error, settings), 1);

    // 1500 px at 96 dpi -> 1125 pt, plus the margin on both sides.
    QCOMPARE(model.entryAt(0)->pageSize, QSizeF(1500.0 * 72.0 / 96.0 + 36.0, 300.0 * 72.0 / 96.0 + 36.0));

    const QRectF content = DocumentBuilderExport::contentRectForImage(model.pagesForExport().first(), QSize(1500, 300));
    QVERIFY(almostEqual(content.width(), 1500.0 * 72.0 / 96.0));
    QVERIFY(almostEqual(content.height(), 300.0 * 72.0 / 96.0));
}

void DocumentBuilderModelTest::imageContentRectPreservesAspect()
{
    DocumentBuilderExport::Page page;
    page.kind = BuilderKind::Image;
    page.pageSize = QSizeF(595.0, 842.0);
    page.margin = 18.0;
    page.dpi = 96;

    const QSize pixels(1500, 300); // 5:1
    const qreal aspect = 1500.0 / 300.0;
    const qreal availableWidth = 595.0 - 2 * 18.0;
    const qreal availableHeight = 842.0 - 2 * 18.0;

    page.fit = ImageFit::FitPage;
    QRectF rect = DocumentBuilderExport::contentRectForImage(page, pixels);
    QVERIFY(almostEqual(rect.width() / rect.height(), aspect, 1e-6));
    QVERIFY(rect.width() <= availableWidth + 0.01);
    QVERIFY(rect.height() <= availableHeight + 0.01);
    QVERIFY(almostEqual(rect.center().x(), 595.0 / 2.0));
    QVERIFY(almostEqual(rect.center().y(), 842.0 / 2.0));

    page.fit = ImageFit::FillPage;
    rect = DocumentBuilderExport::contentRectForImage(page, pixels);
    QVERIFY(almostEqual(rect.width() / rect.height(), aspect, 1e-6));
    QVERIFY(rect.width() >= availableWidth - 0.01);
    QVERIFY(rect.height() >= availableHeight - 0.01);

    page.fit = ImageFit::OriginalSize;
    rect = DocumentBuilderExport::contentRectForImage(page, pixels);
    QVERIFY(almostEqual(rect.width(), 1500.0 * 72.0 / 96.0));
    QVERIFY(almostEqual(rect.height(), 300.0 * 72.0 / 96.0));

    // A page with no usable image size still yields the available area rather than a degenerate rect.
    rect = DocumentBuilderExport::contentRectForImage(page, QSize());
    QVERIFY(almostEqual(rect.width(), availableWidth));
    QVERIFY(almostEqual(rect.height(), availableHeight));
}

void DocumentBuilderModelTest::unreadableImageIsRejectedAndLeavesTheModelAlone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("broken.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not an image at all\n");
    file.close();

    DocumentBuilderModel model;
    QString error;
    QVERIFY(model.addImage(path, &error) < 0);
    QVERIFY(!error.isEmpty());
    QCOMPARE(model.pageCount(), 0);
    QCOMPARE(model.rowCount(), 1);
}

void DocumentBuilderModelTest::imageThumbnailFollowsThePageSetup()
{
    DocumentBuilderModel model;
    model.setThumbnailWidth(100);

    QString error;
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error), 1);

    const QModelIndex index = model.index(0, 0);
    const QImage portrait = model.data(index, DocumentBuilderModel::ThumbnailRole).value<QImage>();
    QVERIFY(!portrait.isNull());
    // The card shows the page (A4 landscape there), so its aspect follows the page, not the file.
    QCOMPARE(model.entryAt(0)->pageSize, QSizeF(842.0, 595.0));
    QVERIFY(almostEqual(qreal(portrait.width()) / portrait.height(), 842.0 / 595.0, 0.15));

    ImagePageSettings settings;
    settings.fit = ImageFit::OriginalSize;
    settings.margin = 18.0;
    settings.dpi = 96;
    QCOMPARE(model.setImageSettings(QList<QUuid>() << model.entryIdAt(0), settings), 1);

    const QSizeF pageSize = model.entryAt(0)->pageSize;
    const QImage wide = model.data(index, DocumentBuilderModel::ThumbnailRole).value<QImage>();
    QVERIFY(!wide.isNull());
    QVERIFY(wide.width() > wide.height());
    QVERIFY(almostEqual(qreal(wide.width()) / wide.height(), pageSize.width() / pageSize.height(), 0.15));
}

void DocumentBuilderModelTest::setImageSettingsTouchesOnlyTheChosenImages()
{
    DocumentBuilderModel model;
    QString error;
    QCOMPARE(model.addImage(dataFile(QStringLiteral("1500x300.png")), &error), 1);
    QCOMPARE(model.addImage(dataFile(QStringLiteral("300x1500.png")), &error), 1);
    QCOMPARE(model.addPdf(dataFile(QStringLiteral("file1.pdf")), &error), 1);

    const QSizeF untouched = model.entryAt(1)->pageSize;
    const QSizeF pdfSize = model.entryAt(2)->pageSize;

    ImagePageSettings settings;
    settings.pageSize = QSizeF(400.0, 400.0);
    settings.background = QColor(Qt::yellow);
    settings.margin = 10.0;

    // The PDF page is in the list too and must be ignored.
    QCOMPARE(model.setImageSettings(QList<QUuid>() << model.entryIdAt(0) << model.entryIdAt(2), settings), 1);

    QCOMPARE(model.entryAt(0)->pageSize, QSizeF(400.0, 400.0));
    QCOMPARE(model.entryAt(0)->background, QColor(Qt::yellow));
    QCOMPARE(model.entryAt(1)->pageSize, untouched);
    QVERIFY(model.entryAt(2)->kind == BuilderKind::PdfPage);
    QCOMPARE(model.entryAt(2)->pageSize, pdfSize);
    QCOMPARE(model.entryAt(2)->background, QColor(Qt::white));
}

void DocumentBuilderModelTest::exifOrientationIsAppliedToImagePages()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("rotated.jpg"));
    // 40x80 portrait pixels; EXIF orientation 6 = "rotate 90 degrees clockwise for display".
    QVERIFY(writeJpegWithOrientation(path, QSize(40, 80), 6, Qt::red));

    QImageReader autoReader(path);
    autoReader.setAutoTransform(true);
    const QImage displayed = autoReader.read();
    QImageReader plainReader(path);
    plainReader.setAutoTransform(false);
    const QImage plain = plainReader.read();
    if (displayed.isNull() || plain.isNull() || displayed.size() == plain.size()) {
        QSKIP("This Qt build does not apply EXIF orientation to JPEG reads; check EXIF by hand.");
    }
    // The stored pixels are portrait (40x80) but the displayed image is landscape (80x40).
    QCOMPARE(displayed.size(), plain.size().transposed());

    // With Original size the page is derived from the *displayed* size, so the EXIF rotation is
    // visible in the page: it must follow the landscape read, not the portrait bitmap.
    DocumentBuilderModel model;
    ImagePageSettings settings;
    settings.fit = ImageFit::OriginalSize;
    settings.margin = 18.0;
    settings.dpi = 96;

    QString error;
    QCOMPARE(model.addImage(path, &error, settings), 1);

    const qreal pointsPerPixel = 72.0 / 96.0;
    const QSizeF expected(displayed.width() * pointsPerPixel + 36.0, displayed.height() * pointsPerPixel + 36.0);
    QCOMPARE(model.entryAt(0)->pageSize, expected);
    QVERIFY(model.entryAt(0)->pageSize.width() > model.entryAt(0)->pageSize.height());
}

void DocumentBuilderModelTest::pageRangeParserTable()
{
    const int pageCount = 12;

    auto parse = [pageCount](const QString &text, QString *error) {
        return PageRangeInputDialog::parsePages(text, pageCount, error);
    };

    QString error;

    QCOMPARE(parse(QStringLiteral("1"), &error), QList<int>() << 0);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1-4"), &error), QList<int>() << 0 << 1 << 2 << 3);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1,1,2"), &error), QList<int>() << 0 << 1); // de-duplicated, first-seen order
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1-4,7,9-12"), &error), QList<int>() << 0 << 1 << 2 << 3 << 6 << 8 << 9 << 10 << 11);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1 - 4"), &error), QList<int>() << 0 << 1 << 2 << 3);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("  1,  2  "), &error), QList<int>() << 0 << 1);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1,2,"), &error), QList<int>() << 0 << 1); // trailing comma tolerated
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1,,2"), &error), QList<int>() << 0 << 1);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("12"), &error), QList<int>() << 11);
    QVERIFY(error.isEmpty());
    QCOMPARE(parse(QStringLiteral("1-12"), &error).size(), 12);
    QVERIFY(error.isEmpty());

    // Rejections: never silently drop or reorder.
    QVERIFY(parse(QStringLiteral("4-1"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral("0"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral("13"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(QStringLiteral("12"))); // names the valid count
    QVERIFY(parse(QStringLiteral("1-13"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QString(), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral("   "), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral("abc"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral("1-"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral("-3"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(parse(QStringLiteral(","), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void DocumentBuilderModelTest::addPdfPagesAddsExactlyTheRequestedPages()
{
    DocumentBuilderModel model;
    QString error;

    QCOMPARE(model.addPdfPages(dataFile(QStringLiteral("simple-multipage.pdf")), QList<int>() << 4 << 0 << 2, &error), 3);
    QVERIFY(error.isEmpty());
    QCOMPARE(model.pageCount(), 3);
    QCOMPARE(sourcePages(model), QList<int>() << 4 << 0 << 2);
    QCOMPARE(model.rowCount(), 4);
    QVERIFY(model.isAddTile(3));

    // A page that does not exist is refused and leaves the model as it was.
    QVERIFY(model.addPdfPages(dataFile(QStringLiteral("simple-multipage.pdf")), QList<int>() << 999, &error) < 0);
    QVERIFY(!error.isEmpty());
    QCOMPARE(model.pageCount(), 3);
    QCOMPARE(sourcePages(model), QList<int>() << 4 << 0 << 2);
}

QTEST_MAIN(DocumentBuilderModelTest)

#include "documentbuildermodeltest.moc"
