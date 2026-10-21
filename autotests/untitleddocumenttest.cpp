/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 Charta contributors (licensing party; AI-assisted)
*/

// Tests for the part side of "Create a PDF": a brand-new document that has no location yet lives in
// a private temporary file, saving it moves the document to the chosen location and removes the
// temporary file, and closing the document removes the temporary file as well.
//
// These drive Okular::Part directly (the same way parttest does), so the whole lifecycle can be
// checked without a user interface.

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSignalSpy>
#include <QSizeF>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include <KParts/ReadWritePart>

#include "../part/blankpageswriter.h"
#include "../part/part.h"

// A part that can be told to refuse to close, as when the user says "Cancel" to the "save your
// changes?" prompt
class PartThatCanRefuseToClose : public Okular::Part
{
    Q_OBJECT

public:
    PartThatCanRefuseToClose(QObject *parent, const QVariantList &args)
        : Okular::Part(parent, args)
    {
    }

    bool refuseToClose = false;

    bool queryClose() override
    {
        return !refuseToClose;
    }
};

class UntitledDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void handoffMatchesTheShellContract();
    void createsTheRequestedPages();
    void releasesTheTempFileWhenClosed();
    void keepsForeignDirectories();
    void savingMovesTheDocumentAndStopsAskingForALocation();
    void failedSaveKeepsTheDocumentAndItsTempFile();
    void twoUntitledDocumentsDoNotInterfere();
    void ordinaryDocumentDoesNotInheritTheState();
    void abortedOpenRemovesTheOrphanFile();

private:
    // Parts are deliberately never destroyed: destroying an Okular::Part is broken in this tree for
    // reasons that have nothing to do with this feature. ~Part deletes the document, and only then
    // the base class deletes the viewer widget, whose sidebar table of contents calls
    // Document::removeObserver() on the document that is already gone. parttest segfaults in exactly
    // the same place, even when a single of its tests is run. Every assertion below happens while the
    // part is still alive, so leaking them is enough to test the lifecycle.
    template<class PartType = Okular::Part> static PartType *newPart()
    {
        return new PartType(nullptr, {});
    }

    // Writes a document the way Part::slotCreateNewDocument() does: <temp>/charta-untitled-<name>/Untitled.pdf
    static QString makeTempDocument(const QString &name, int pages = 1)
    {
        const QString dir = QDir(QDir::tempPath()).filePath(QStringLiteral("charta-untitled-%1").arg(name));
        QDir(dir).removeRecursively(); // leftovers from an interrupted earlier run
        if (!QDir().mkpath(dir)) {
            return QString();
        }

        const QString path = QDir(dir).filePath(QStringLiteral("Untitled.pdf"));
        if (!Okular::writeBlankPagesPdf(path, pages, QSizeF(595.0, 842.0), QColor(Qt::white))) {
            return QString();
        }
        return path;
    }

    // Runs function() and fails if it opens a modal dialog. The dialog is also closed so that a
    // regression cannot hang the whole test suite.
    template<class Function> void runWithoutDialogs(Function function)
    {
        m_dialogAppeared = false;
        QTimer dialogWatcher;
        connect(&dialogWatcher, &QTimer::timeout, this, [this] {
            if (QWidget *dialog = QApplication::activeModalWidget()) {
                m_dialogAppeared = true;
                dialog->close();
            }
        });
        dialogWatcher.start(50);

        function();

        dialogWatcher.stop();
        QVERIFY(!m_dialogAppeared);
    }

    bool m_dialogAppeared = false;
};

void UntitledDocumentTest::handoffMatchesTheShellContract()
{
    Okular::Part *part = newPart();

    // The shell is not linked against the part, it drives it by name: these names are the contract
    // between the two (Shell::createNewDocument, Shell::connectPart, Shell::applyOptionsToPart)
    QVERIFY(part->metaObject()->indexOfSlot("slotCreateNewDocument()") != -1);
    QVERIFY(part->metaObject()->indexOfSlot("setUntitledDocument(QString)") != -1);
    QVERIFY(part->metaObject()->indexOfSignal("requestOpenNewUntitledDocument(QString)") != -1);
    QVERIFY(part->metaObject()->indexOfSignal("requestAddRecentFile(QString)") != -1);

    // ... and the handoff works when it is made the way the shell makes it
    const QString path = makeTempDocument(QStringLiteral("by-name"));
    QVERIFY(!path.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(part, "setUntitledDocument", Q_ARG(QString, path)));
    part->openDocument(path);

    QVERIFY(part->isUntitledDocument());
    QCOMPARE(part->url(), QUrl::fromLocalFile(path));

    QVERIFY(part->closeUrl());
}

void UntitledDocumentTest::createsTheRequestedPages()
{
    const QString path = makeTempDocument(QStringLiteral("pages"), 3);
    QVERIFY(!path.isEmpty());

    Okular::Part *part = newPart();
    part->setUntitledDocument(path); // the shell does this just before opening
    part->openDocument(path);

    QCOMPARE(part->url(), QUrl::fromLocalFile(path));
    QCOMPARE(part->pages(), 3u);
    QVERIFY(part->isUntitledDocument());
    QVERIFY(QFileInfo::exists(path));

    QVERIFY(part->closeUrl()); // leave the temporary directory behind for nobody
}

void UntitledDocumentTest::releasesTheTempFileWhenClosed()
{
    const QString path = makeTempDocument(QStringLiteral("close"));
    QVERIFY(!path.isEmpty());
    const QString dir = QFileInfo(path).absolutePath();

    Okular::Part *part = newPart();
    part->setUntitledDocument(path);
    part->openDocument(path);
    QVERIFY(part->isUntitledDocument());
    QVERIFY(QFileInfo(dir).isDir());

    // What the shell does when the tab is closed. Note that the part survives when it was the last
    // tab, which is why this releases the temporary file and not the destructor alone.
    QVERIFY(part->closeUrl());

    QVERIFY(!part->isUntitledDocument());
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(!QFileInfo::exists(dir));
}

void UntitledDocumentTest::keepsForeignDirectories()
{
    // A file that is not in one of our private directories: removing the file is fine, removing the
    // directory it lives in is not
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Untitled.pdf"));
    QVERIFY(Okular::writeBlankPagesPdf(path, 1, QSizeF(595.0, 842.0), QColor(Qt::white)));

    Okular::Part *part = newPart();
    part->setUntitledDocument(path);
    part->openDocument(path);
    QVERIFY(part->isUntitledDocument());

    QVERIFY(part->closeUrl());

    QVERIFY(!part->isUntitledDocument());
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(QFileInfo::exists(dir.path()));
}

void UntitledDocumentTest::savingMovesTheDocumentAndStopsAskingForALocation()
{
    const QString path = makeTempDocument(QStringLiteral("save"), 2);
    QVERIFY(!path.isEmpty());
    const QString tempDir = QFileInfo(path).absolutePath();

    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    const QString saved = destination.filePath(QStringLiteral("saved.pdf"));

    Okular::Part *part = newPart();
    QSignalSpy recentFilesSpy(part, &Okular::Part::requestAddRecentFile);
    part->setUntitledDocument(path);
    part->openDocument(path);
    QVERIFY(part->isUntitledDocument());

    QVERIFY(part->saveAs(QUrl::fromLocalFile(saved)));

    // The document has a location now
    QCOMPARE(part->url(), QUrl::fromLocalFile(saved));
    QVERIFY(!part->isUntitledDocument());
    QVERIFY(QFileInfo::exists(saved));
    QCOMPARE(part->pages(), 2u);

    // The temporary copy is gone, and the shell was told where the document went
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(!QFileInfo::exists(tempDir));
    QCOMPARE(recentFilesSpy.count(), 1);
    QCOMPARE(recentFilesSpy.first().first().toString(), saved);

    // Ctrl+S saves silently from now on: a leftover untitled state would open the Save As dialog
    bool savedAgain = false;
    runWithoutDialogs([&] {
        savedAgain = part->saveFile();
    });
    QVERIFY(savedAgain);
}

void UntitledDocumentTest::failedSaveKeepsTheDocumentAndItsTempFile()
{
#ifdef Q_OS_UNIX
    if (::geteuid() == 0) {
        QSKIP("Permissions are not enforced for root");
    }
#endif

    const QString path = makeTempDocument(QStringLiteral("failed-save"));
    QVERIFY(!path.isEmpty());

    Okular::Part *part = newPart();
    part->setUntitledDocument(path);
    part->openDocument(path);
    QVERIFY(part->isUntitledDocument());

    // A destination that cannot be written
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    const QString target = destination.filePath(QStringLiteral("out.pdf"));
    QVERIFY(QFile::setPermissions(destination.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    // The save failure reports itself in a message box
    QTimer::singleShot(0, [] {
        if (auto *messageBox = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            messageBox->accept();
        }
    });
    const bool saved = part->saveAs(QUrl::fromLocalFile(target));
    QVERIFY(QFile::setPermissions(destination.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    // The document is untouched: still open, still untitled, temporary file still there
    QVERIFY(!saved);
    QVERIFY(part->isUntitledDocument());
    QCOMPARE(part->url(), QUrl::fromLocalFile(path));
    QCOMPARE(part->pages(), 1u);
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(!QFileInfo::exists(target));

    QVERIFY(part->closeUrl());
}

void UntitledDocumentTest::twoUntitledDocumentsDoNotInterfere()
{
    const QString first = makeTempDocument(QStringLiteral("two-a"));
    const QString second = makeTempDocument(QStringLiteral("two-b"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());

    Okular::Part *partA = newPart();
    partA->setUntitledDocument(first);
    partA->openDocument(first);
    Okular::Part *partB = newPart();
    partB->setUntitledDocument(second);
    partB->openDocument(second);
    QVERIFY(partA->isUntitledDocument());
    QVERIFY(partB->isUntitledDocument());

    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    QVERIFY(partA->saveAs(QUrl::fromLocalFile(destination.filePath(QStringLiteral("a.pdf")))));

    // Only A was saved: B keeps its own temporary file
    QVERIFY(!QFileInfo::exists(first));
    QVERIFY(QFileInfo::exists(second));
    QVERIFY(partB->isUntitledDocument());
    QCOMPARE(partB->url(), QUrl::fromLocalFile(second));
    QCOMPARE(partB->pages(), 1u);

    QVERIFY(partB->closeUrl());
}

void UntitledDocumentTest::ordinaryDocumentDoesNotInheritTheState()
{
    const QString normal = QStringLiteral(KDESRCDIR "data/file1.pdf");
    const QString path = makeTempDocument(QStringLiteral("ordinary"));
    QVERIFY(!path.isEmpty());

    Okular::Part *part = newPart();
    part->setUntitledDocument(path);
    part->openDocument(path);
    QVERIFY(part->isUntitledDocument());

    // Opening an ordinary file: the shell passes the (empty) pending path on every open
    part->setUntitledDocument(QString());
    part->openDocument(normal);

    QVERIFY(!part->isUntitledDocument());
    QCOMPARE(part->url(), QUrl::fromLocalFile(normal));
    QVERIFY(!QFileInfo::exists(path)); // released when the document was replaced

    bool saved = false;
    runWithoutDialogs([&] {
        saved = part->saveFile();
    });
    QVERIFY(saved);

    QVERIFY(part->closeUrl());
    QVERIFY(QFileInfo::exists(normal)); // an ordinary file is never removed
}

void UntitledDocumentTest::abortedOpenRemovesTheOrphanFile()
{
    const QString shown = makeTempDocument(QStringLiteral("abort-shown"));
    const QString orphan = makeTempDocument(QStringLiteral("abort-orphan"));
    QVERIFY(!shown.isEmpty() && !orphan.isEmpty());

    PartThatCanRefuseToClose *part = newPart<PartThatCanRefuseToClose>();
    part->setUntitledDocument(shown);
    part->openDocument(shown);
    QVERIFY(part->isUntitledDocument());
    QCOMPARE(part->url(), QUrl::fromLocalFile(shown));

    // The part now refuses to close, so the open is aborted: the file that was just written for it
    // would never have an owner
    part->refuseToClose = true;
    part->setUntitledDocument(orphan);
    KParts::ReadWritePart *asReadWritePart = part;
    QVERIFY(!asReadWritePart->openUrl(QUrl::fromLocalFile(orphan)));

    QVERIFY(!QFileInfo::exists(orphan));
    QVERIFY(!QFileInfo::exists(QFileInfo(orphan).absolutePath()));

    // The document that was showing is untouched
    QCOMPARE(part->url(), QUrl::fromLocalFile(shown));
    QVERIFY(part->isUntitledDocument());
    QVERIFY(QFileInfo::exists(shown));

    part->refuseToClose = false;
    QVERIFY(part->closeUrl());
}

QTEST_MAIN(UntitledDocumentTest)
#include "untitleddocumenttest.moc"
