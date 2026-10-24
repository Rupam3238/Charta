/*
    SPDX-FileCopyrightText: 2013 Fabio D 'Urso <fabiodurso@hotmail.it>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QMimeDatabase>
#include <QTemporaryFile>
#include <QTest>

#include <threadweaver/queue.h>

#include "../core/annotations.h"
#include "../core/document.h"
#include "../core/document_p.h"
#include "../core/generator.h"
#include "../core/observer.h"
#include "../core/page.h"
#include "../core/rotationjob_p.h"
#include "../settings_core.h"

class DocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCloseDuringRotationJob();
    void testDocdataMigration();
    void testEvaluateKeystrokeEventChange_data();
    void testEvaluateKeystrokeEventChange();
    void testDuplicatePage();
    void testDuplicatePages();
};

// Test that we don't crash if the document is closed while a RotationJob
// is enqueued/running
void DocumentTest::testCloseDuringRotationJob()
{
    Okular::SettingsCore::instance(QStringLiteral("documenttest"));
    Okular::Document *m_document = new Okular::Document(nullptr);
    const QString testFile = QStringLiteral(KDESRCDIR "data/file1.pdf");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);

    Okular::DocumentObserver *dummyDocumentObserver = new Okular::DocumentObserver();
    m_document->addObserver(dummyDocumentObserver);

    m_document->openDocument(testFile, QUrl(), mime);
    m_document->setRotation(1);

    // Tell ThreadWeaver not to start any new job
    ThreadWeaver::Queue::instance()->suspend();

    // Request a pixmap. A RotationJob will be enqueued but not started
    Okular::PixmapRequest *pixmapReq = new Okular::PixmapRequest(dummyDocumentObserver, 0, 100, 100, qApp->devicePixelRatio(), 1, Okular::PixmapRequest::NoFeature);
    m_document->requestPixmaps({pixmapReq});

    // Delete the document
    delete m_document;

    // Resume job processing and wait for the RotationJob to finish
    ThreadWeaver::Queue::instance()->resume();
    ThreadWeaver::Queue::instance()->finish();
    qApp->processEvents();

    delete dummyDocumentObserver;
}

// Test that, if there's a XML file in docdata referring to a document, we
// detect that it must be migrated, that it doesn't get wiped out if you close
// the document without migrating and that it does get wiped out after migrating
void DocumentTest::testDocdataMigration()
{
    Okular::SettingsCore::instance(QStringLiteral("documenttest"));

    const QUrl testFileUrl = QUrl::fromLocalFile(QStringLiteral(KDESRCDIR "data/file1.pdf"));
    const QString testFilePath = testFileUrl.toLocalFile();
    const qint64 testFileSize = QFileInfo(testFilePath).size();

    // Copy XML file to the docdata/ directory
    const QString docDataPath = Okular::DocumentPrivate::docDataFileName(testFileUrl, testFileSize);
    QFile::remove(docDataPath);
    QVERIFY(QFile::copy(QStringLiteral(KDESRCDIR "data/file1-docdata.xml"), docDataPath));

    // Open our document
    Okular::Document *m_document = new Okular::Document(nullptr);
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFilePath);
    QCOMPARE(m_document->openDocument(testFilePath, testFileUrl, mime), Okular::Document::OpenSuccess);

    // Check that the annotation from file1-docdata.xml was loaded
    QCOMPARE(m_document->page(0)->annotations().size(), 1);
    QCOMPARE(m_document->page(0)->annotations().constFirst()->uniqueName(), QStringLiteral("testannot"));

    // Check that we detect that it must be migrated
    QVERIFY(m_document->isDocdataMigrationNeeded());
    m_document->closeDocument();

    // Reopen the document and check that the annotation is still present
    // (because we have not migrated)
    QCOMPARE(m_document->openDocument(testFilePath, testFileUrl, mime), Okular::Document::OpenSuccess);
    QCOMPARE(m_document->page(0)->annotations().size(), 1);
    QCOMPARE(m_document->page(0)->annotations().constFirst()->uniqueName(), QStringLiteral("testannot"));
    QVERIFY(m_document->isDocdataMigrationNeeded());

    // Do the migration
    QTemporaryFile migratedSaveFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
    QVERIFY(migratedSaveFile.open());
    migratedSaveFile.close();
    QString errorString;
    QVERIFY(m_document->saveChanges(migratedSaveFile.fileName(), &errorString));
    QVERIFY(errorString.isEmpty());
    m_document->docdataMigrationDone();
    QVERIFY(!m_document->isDocdataMigrationNeeded());
    m_document->closeDocument();

    // Now the docdata file should have no annotations, let's check
    QCOMPARE(m_document->openDocument(testFilePath, testFileUrl, mime), Okular::Document::OpenSuccess);
    QCOMPARE(m_document->page(0)->annotations().size(), 0);
    QVERIFY(!m_document->isDocdataMigrationNeeded());
    m_document->closeDocument();

    // And the new file should have 1 annotation, let's check
    QCOMPARE(m_document->openDocument(migratedSaveFile.fileName(), QUrl::fromLocalFile(migratedSaveFile.fileName()), mime), Okular::Document::OpenSuccess);
    QCOMPARE(m_document->page(0)->annotations().size(), 1);
    QVERIFY(!m_document->isDocdataMigrationNeeded());
    m_document->closeDocument();

    delete m_document;
}

void DocumentTest::testEvaluateKeystrokeEventChange_data()
{
    QTest::addColumn<QString>("oldVal");
    QTest::addColumn<QString>("newVal");
    QTest::addColumn<int>("selStart");
    QTest::addColumn<int>("selEnd");
    QTest::addColumn<QString>("expectedDiff");

    QTest::addRow("empty") << ""
                           << "" << 0 << 0 << "";
    QTest::addRow("a") << ""
                       << "a" << 0 << 0 << "a";
    QTest::addRow("ab") << "a"
                        << "b" << 0 << 1 << "b";
    QTest::addRow("ab2") << "a"
                         << "ab" << 1 << 1 << "b";
    QTest::addRow("kaesekuchen") << "Käse"
                                 << "Käsekuchen" << 4 << 4 << "kuchen";
    QTest::addRow("replace") << "kuchen"
                             << "wurst" << 0 << 6 << "wurst";
    QTest::addRow("okular") << "Oku"
                            << "Okular" << 3 << 3 << "lar";
    QTest::addRow("okular2") << "Oku"
                             << "Okular" << 0 << 3 << "Okular";
    QTest::addRow("removal1") << "a"
                              << "" << 0 << 1 << "";
    QTest::addRow("removal2") << "ab"
                              << "a" << 1 << 2 << "";
    QTest::addRow("overlapping chang") << "abcd"
                                       << "abclmnopd" << 1 << 3 << "bclmnop";
    QTest::addRow("unicode") << "☮🤌"
                             << "☮🤌❤️" << 2 << 2 << "❤️";
    QTest::addRow("unicode2") << "☮"
                              << "☮🤌❤️" << 1 << 1 << "🤌❤️";
    QTest::addRow("unicode3") << "🤍"
                              << "🤌" << 0 << 1 << "🤌";
}

void DocumentTest::testEvaluateKeystrokeEventChange()
{
    QFETCH(QString, oldVal);
    QFETCH(QString, newVal);
    QFETCH(int, selStart);
    QFETCH(int, selEnd);
    QFETCH(QString, expectedDiff);

    QCOMPARE(Okular::DocumentPrivate::evaluateKeystrokeEventChange(oldVal, newVal, selStart, selEnd), expectedDiff);
}

// Duplicate a single page: the document grows by one, the copy sits right after
// the original, its content matches the original, and the original is untouched.
void DocumentTest::testDuplicatePage()
{
    Okular::SettingsCore::instance(QStringLiteral("documenttest"));

    const QString testFile = QStringLiteral(KDESRCDIR "data/simple-multipage.pdf");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);

    Okular::Document *document = new Okular::Document(nullptr);
    QCOMPARE(document->openDocument(testFile, QUrl(), mime), Okular::Document::OpenSuccess);
    QVERIFY(document->canModifyPages());

    const uint originalCount = document->pages();
    QVERIFY(originalCount > 1);

    const int index = 5;
    QVERIFY(index < (int)originalCount);

    // Add an annotation to the original so the deep copy can be checked.
    auto *ann = new Okular::TextAnnotation();
    ann->setContents(QStringLiteral("original-annotation"));
    const_cast<Okular::Page *>(document->page(index))->addAnnotation(ann);
    const int originalAnnotations = document->page(index)->annotations().size();

    // Snapshot the original before duplicating it.
    const double originalWidth = document->page(index)->width();
    const double originalHeight = document->page(index)->height();
    const Okular::Rotation originalOrientation = document->page(index)->orientation();
    const int originalFormFields = document->page(index)->formFields().size();
    const QString originalText = document->page(index)->text();

    QVERIFY(document->duplicatePage(index));
    QCOMPARE(document->pages(), originalCount + 1);

    // The copy sits right after the original.
    QCOMPARE(document->page(index + 1)->width(), originalWidth);
    QCOMPARE(document->page(index + 1)->height(), originalHeight);
    QCOMPARE(document->page(index + 1)->orientation(), originalOrientation);
    QCOMPARE(document->page(index + 1)->annotations().size(), originalAnnotations);
    QCOMPARE(document->page(index + 1)->formFields().size(), originalFormFields);
    QCOMPARE(document->page(index + 1)->text(), originalText);

    // The original page stays at its index and is untouched.
    QCOMPARE(document->page(index)->width(), originalWidth);
    QCOMPARE(document->page(index)->height(), originalHeight);
    QCOMPARE(document->page(index)->orientation(), originalOrientation);
    QCOMPARE(document->page(index)->annotations().size(), originalAnnotations);
    QCOMPARE(document->page(index)->formFields().size(), originalFormFields);
    QCOMPARE(document->page(index)->text(), originalText);

    // The duplicate must be independent: editing it must not affect the original.
    auto *dupAnn = new Okular::TextAnnotation();
    dupAnn->setContents(QStringLiteral("duplicate-only-annotation"));
    const_cast<Okular::Page *>(document->page(index + 1))->addAnnotation(dupAnn);
    QCOMPARE(document->page(index + 1)->annotations().size(), originalAnnotations + 1);
    QCOMPARE(document->page(index)->annotations().size(), originalAnnotations);
    if (originalAnnotations > 0) {
        QVERIFY(document->page(index)->annotations().at(0) != document->page(index + 1)->annotations().at(0));
    }

    // Mutating an existing annotation on the duplicate must not mutate the original.
    QVERIFY(document->page(index + 1)->annotations().size() > 0);
    document->page(index + 1)->annotations().at(0)->setContents(QStringLiteral("changed-on-duplicate"));
    QCOMPARE(document->page(index)->annotations().at(0)->contents(), QStringLiteral("original-annotation"));

    // Adding an annotation to the original after duplication must not appear on the duplicate.
    auto *laterAnn = new Okular::TextAnnotation();
    laterAnn->setContents(QStringLiteral("later-on-original"));
    const_cast<Okular::Page *>(document->page(index))->addAnnotation(laterAnn);
    QCOMPARE(document->page(index)->annotations().size(), originalAnnotations + 1);
    QCOMPARE(document->page(index + 1)->annotations().size(), originalAnnotations + 1);

    // Invalid indices are rejected and leave the document unchanged.
    QVERIFY(!document->duplicatePage(-1));
    QVERIFY(!document->duplicatePage((int)originalCount + 10));
    QCOMPARE(document->pages(), originalCount + 1);

    delete document;
}

// Duplicate several pages at once: the document grows by N, each copy sits right
// after its original, and out-of-range indices fail the whole operation.
void DocumentTest::testDuplicatePages()
{
    Okular::SettingsCore::instance(QStringLiteral("documenttest"));

    const QString testFile = QStringLiteral(KDESRCDIR "data/simple-multipage.pdf");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);

    Okular::Document *document = new Okular::Document(nullptr);
    QCOMPARE(document->openDocument(testFile, QUrl(), mime), Okular::Document::OpenSuccess);
    QVERIFY(document->canModifyPages());

    const uint originalCount = document->pages();
    QVERIFY(originalCount > 2);

    // Duplicate the first and the last page. Indices are handled in descending
    // order, so the last page is duplicated first, then the first one.
    QVERIFY(document->duplicatePages(QList<int>() << 0 << (int)(originalCount - 1)));
    QCOMPARE(document->pages(), originalCount + 2);

    // Copy of page 0 now sits at index 1.
    QCOMPARE(document->page(1)->width(), document->page(0)->width());
    QCOMPARE(document->page(1)->height(), document->page(0)->height());
    QCOMPARE(document->page(1)->orientation(), document->page(0)->orientation());
    QCOMPARE(document->page(1)->text(), document->page(0)->text());

    // Original last page is now at index originalCount, its copy right after it.
    QCOMPARE(document->page(originalCount)->width(), document->page(originalCount - 1)->width());
    QCOMPARE(document->page(originalCount + 1)->width(), document->page(originalCount)->width());
    QCOMPARE(document->page(originalCount + 1)->orientation(), document->page(originalCount)->orientation());

    // An out-of-range index fails the whole call and changes nothing.
    QVERIFY(!document->duplicatePages(QList<int>() << 0 << 999999));
    QCOMPARE(document->pages(), originalCount + 2);

    // An empty list fails and changes nothing.
    QVERIFY(!document->duplicatePages(QList<int>()));
    QCOMPARE(document->pages(), originalCount + 2);

    delete document;
}

QTEST_MAIN(DocumentTest)
#include "documenttest.moc"
