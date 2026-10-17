/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSizeF>
#include <QTemporaryDir>
#include <QTest>

#include <poppler-qt6.h>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include "../part/blankpageswriter.h"

class BlankPagesWriterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void writesRequestedPageCount();
    void writesRequestedPageSize();
    void writesRequestedBackgroundColor();
    void replacesAnExistingFile();
    void rejectsInvalidInput();
    void failedWritePreservesExistingFile();
    void rejectsUnwritableDestination();

private:
    static QColor renderedColor(Poppler::Page *page)
    {
        const QImage image = page->renderToImage(72, 72); // one pixel per point
        if (image.isNull()) {
            return QColor();
        }
        return image.pixelColor(image.width() / 2, image.height() / 2);
    }
};

void BlankPagesWriterTest::writesRequestedPageCount()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("count.pdf"));

    QVERIFY(Okular::writeBlankPagesPdf(file, 3, QSizeF(595.0, 842.0), QColor(Qt::white)));

    const std::unique_ptr<Poppler::Document> document(Poppler::Document::load(file));
    QVERIFY(document);
    QCOMPARE(document->numPages(), 3);
}

void BlankPagesWriterTest::writesRequestedPageSize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("size.pdf"));

    // A3 landscape
    QVERIFY(Okular::writeBlankPagesPdf(file, 1, QSizeF(1191.0, 842.0), QColor(Qt::white)));

    const std::unique_ptr<Poppler::Document> document(Poppler::Document::load(file));
    QVERIFY(document);
    QCOMPARE(document->numPages(), 1);
    const QSizeF size = document->page(0)->pageSizeF(); // points
    QVERIFY(qAbs(size.width() - 1191.0) < 2.0);
    QVERIFY(qAbs(size.height() - 842.0) < 2.0);
}

void BlankPagesWriterTest::writesRequestedBackgroundColor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("color.pdf"));

    QVERIFY(Okular::writeBlankPagesPdf(file, 1, QSizeF(595.0, 842.0), QColor(255, 128, 0)));

    const std::unique_ptr<Poppler::Document> document(Poppler::Document::load(file));
    QVERIFY(document);
    const QColor rendered = renderedColor(document->page(0).get());
    QVERIFY(rendered.isValid());
    QVERIFY(qAbs(rendered.red() - 255) <= 2);
    QVERIFY(qAbs(rendered.green() - 128) <= 2);
    QVERIFY(qAbs(rendered.blue() - 0) <= 2);
}

void BlankPagesWriterTest::replacesAnExistingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("existing.pdf"));

    // QFile::rename() refuses to replace an existing file, so this also proves the writer really
    // commits over the destination instead of relying on a plain rename
    {
        QFile existing(file);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QVERIFY(existing.write(QByteArrayLiteral("not a pdf")) > 0);
    }

    QVERIFY(Okular::writeBlankPagesPdf(file, 2, QSizeF(595.0, 842.0), QColor(Qt::white)));

    const std::unique_ptr<Poppler::Document> document(Poppler::Document::load(file));
    QVERIFY(document);
    QCOMPARE(document->numPages(), 2);
}

void BlankPagesWriterTest::rejectsInvalidInput()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("invalid.pdf"));

    QVERIFY(!Okular::writeBlankPagesPdf(file, 0, QSizeF(595.0, 842.0), QColor(Qt::white)));
    QVERIFY(!Okular::writeBlankPagesPdf(file, 1, QSizeF(), QColor(Qt::white)));
    QVERIFY(!Okular::writeBlankPagesPdf(file, 1, QSizeF(0.0, 842.0), QColor(Qt::white)));
    QVERIFY(!QFileInfo::exists(file));
}

void BlankPagesWriterTest::failedWritePreservesExistingFile()
{
#ifdef Q_OS_UNIX
    if (::geteuid() == 0) {
        QSKIP("Permissions are not enforced for root");
    }
#endif

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("existing.pdf"));

    const QByteArray original = QByteArrayLiteral("do not touch me");
    {
        QFile existing(file);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write(original), qint64(original.size()));
    }

    // Nothing can be written in a read only directory: the temporary file cannot be created
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QString errorText;
    QVERIFY(!Okular::writeBlankPagesPdf(file, 1, QSizeF(595.0, 842.0), QColor(Qt::white), &errorText));
    QVERIFY(!errorText.isEmpty());
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    QFile after(file);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), original);
}

void BlankPagesWriterTest::rejectsUnwritableDestination()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("no-such-directory/out.pdf"));

    QString errorText;
    QVERIFY(!Okular::writeBlankPagesPdf(file, 1, QSizeF(595.0, 842.0), QColor(Qt::white), &errorText));
    QVERIFY(!errorText.isEmpty());
    QVERIFY(!QFileInfo::exists(file));
}

QTEST_MAIN(BlankPagesWriterTest)
#include "blankpageswritertest.moc"
