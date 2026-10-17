diff --git a/part/part.cpp b/part/part.cpp
index 6cd7053..298d6b2 100644
--- a/part/part.cpp
+++ b/part/part.cpp
@@ -890,6 +890,12 @@ Part::Part(QObject *parent, const QVariantList &args)
     thumbsBox->layout()->addWidget(m_thumbnailList);
     //	ThumbnailController * m_tc = new ThumbnailController( thumbsBox, m_thumbnailList );
     connect(m_thumbnailList.data(), &ThumbnailList::rightClick, this, &Part::slotShowMenu);
+    connect(m_thumbnailList.data(), &ThumbnailList::menuRequested, this, &Part::slotThumbnailMenu);
+    connect(m_thumbnailList.data(), &ThumbnailList::pagesDeleted, this, &Part::slotDeletePages);
+    connect(m_thumbnailList.data(), &ThumbnailList::pageAddedBefore, this, &Part::slotInsertPageBefore);
+    connect(m_thumbnailList.data(), &ThumbnailList::pageAddedAfter, this, &Part::slotInsertPageAfter);
+    connect(m_thumbnailList.data(), &ThumbnailList::pageAdded, this, &Part::slotAddPage);
+    connect(m_thumbnailList.data(), &ThumbnailList::pagesReordered, this, &Part::slotReorderPages);
     m_sidebar->addItem(thumbsBox, QIcon::fromTheme(QStringLiteral("view-preview")), i18n("Thumbnails"));
 
     m_sidebar->setCurrentItem(thumbsBox);
@@ -4659,113 +4665,109 @@ void Part::slotPasteAnnotation()
 // Document Builder Page Management Slots
 void Part::slotDeletePages(const QList<int> &pageNumbers)
 {
-    if (!m_document || pageNumbers.isEmpty())
+    if (!m_document || !m_document->canModifyPages() || pageNumbers.isEmpty()) {
         return;
-
-    // Sort in descending order to avoid index shifting issues when removing pages
-    QList<int> sortedPages = pageNumbers;
-    std::sort(sortedPages.begin(), sortedPages.end(), std::greater<int>());
-
-    // Confirm deletion with user
-    QString message;
-    if (sortedPages.size() == 1) {
-        message = i18np("Are you sure you want to delete this page?",
-                        "Are you sure you want to delete these %1 pages?",
-                        sortedPages.size());
-    } else {
-        message = i18np("Are you sure you want to delete this page?",
-                        "Are you sure you want to delete these %1 pages?",
-                        sortedPages.size());
     }
 
-    if (KMessageBox::questionTwoActions(widget(), message, i18n("Confirm Page Deletion"),
-                                        KStandardGuiItem::ok(), KStandardGuiItem::cancel())
-        != KMessageBox::Ok) {
+    if (pageNumbers.size() >= static_cast<int>(m_document->pages())) {
+        KMessageBox::error(widget(), i18n("A document must keep at least one page."), i18n("Cannot Delete Pages"));
         return;
     }
 
-    // Delete pages from highest index to lowest to prevent index shifting
-    for (int pageNumber : sortedPages) {
-        // Convert to 0-based index for document manipulation
-        int index = pageNumber - 1;
-        if (index >= 0 && index < static_cast<int>(m_document->pages())) {
-            m_document->removePage(index);
-        }
+    const QString message = i18np("Are you sure you want to delete this page?", "Are you sure you want to delete these %1 pages?", pageNumbers.size());
+    if (KMessageBox::warningContinueCancel(widget(), message, i18n("Delete Pages"), KStandardGuiItem::del()) != KMessageBox::Continue) {
+        return;
     }
+
+    m_document->removePages(pageNumbers);
 }
 
-void Part::slotInsertPageBefore(int pageNumber)
+void Part::insertBlankPages(int index, const NewPageDialog::NewPageData &data)
 {
-    if (!m_document || pageNumber < 1)
+    if (!m_document || !m_document->canModifyPages() || index < 0 || index > static_cast<int>(m_document->pages())) {
         return;
+    }
 
-    // Convert to 0-based index
-    int index = pageNumber - 1;
-    if (index > static_cast<int>(m_document->pages()))
-        index = m_document->pages(); // Allow inserting at the end
+    QList<Okular::Page *> newPages;
+    for (int i = 0; i < data.count; ++i) {
+        auto *page = new Okular::Page(0, data.width, data.height, Okular::Rotation::Rotation0);
+        page->setBlank(data.backgroundColor);
+        newPages.append(page);
+    }
+    if (!m_document->insertPages(index, newPages)) {
+        qDeleteAll(newPages);
+    }
+}
 
+void Part::slotInsertPageBefore(int pageNumber)
+{
     NewPageDialog dialog(widget());
     if (dialog.exec() == QDialog::Accepted) {
-        NewPageDialog::NewPageData data = dialog.data();
-        for (int i = 0; i < data.count; ++i) {
-            Okular::Page *page = new Okular::Page(0, data.width, data.height, Okular::Rotation0);
-            m_document->insertPage(index + i, page);
-        }
+        insertBlankPages(pageNumber, dialog.data());
     }
 }
 
 void Part::slotInsertPageAfter(int pageNumber)
 {
-    if (!m_document || pageNumber < 1)
-        return;
-
-    // Convert to 0-based index for insertion after this page
-    int index = pageNumber; // Insert after pageNumber-1, so at index pageNumber
-    if (index > static_cast<int>(m_document->pages()))
-        index = m_document->pages(); // Allow inserting at the end
-
     NewPageDialog dialog(widget());
     if (dialog.exec() == QDialog::Accepted) {
-        NewPageDialog::NewPageData data = dialog.data();
-        for (int i = 0; i < data.count; ++i) {
-            Okular::Page *page = new Okular::Page(0, data.width, data.height, Okular::Rotation0);
-            m_document->insertPage(index + i, page);
-        }
+        insertBlankPages(pageNumber + 1, dialog.data());
     }
 }
 
-void Part::slotAddPage()
+void Part::slotAddPage(const NewPageDialog::NewPageData &data)
 {
-    if (!m_document)
+    if (!m_document) {
         return;
-
-    NewPageDialog dialog(widget());
-    if (dialog.exec() == QDialog::Accepted) {
-        NewPageDialog::NewPageData data = dialog.data();
-        int index = m_document->pages(); // Append at the end
-        for (int i = 0; i < data.count; ++i) {
-            Okular::Page *page = new Okular::Page(0, data.width, data.height, Okular::Rotation0);
-            m_document->insertPage(index + i, page);
-        }
     }
+    // new pages go to the end of the document
+    insertBlankPages(static_cast<int>(m_document->pages()), data);
 }
 
 void Part::slotReorderPages(int fromIndex, int toIndex)
 {
-    if (!m_document || fromIndex < 1 || toIndex < 1)
+    if (!m_document || !m_document->canModifyPages()) {
         return;
+    }
+    m_document->movePage(fromIndex, toIndex);
+}
 
-    // Convert to 0-based indices
-    int from = fromIndex - 1;
-    int to = toIndex - 1;
-
-    // Validate indices
-    int pageCount = m_document->pages();
-    if (from < 0 || from >= pageCount || to < 0 || to >= pageCount)
+void Part::slotThumbnailMenu(QMenu *menu, const Okular::Page *page, const QPoint &globalPos)
+{
+    Q_UNUSED(globalPos)
+    if (!menu || !page || m_embedMode == PrintPreviewMode) {
         return;
+    }
 
-    // Move the page
-    m_document->movePage(from, to);
+    // the menu may outlive the page (the page can be deleted from the menu itself): only keep its number
+    const int pageNumber = page->number();
+    const bool isCurrentPage = pageNumber == m_document->viewport().pageNumber;
+
+    menu->addSeparator();
+    if (m_thumbnailList->isVisible() && !Okular::Settings::syncThumbnailsViewport()) {
+        const QIcon &syncIcon = QIcon::fromTheme(QStringLiteral("emblem-synchronizing"), QIcon::fromTheme(QStringLiteral("view-refresh")));
+        menu->addAction(syncIcon, i18n("Sync Thumbnail with Page"), m_thumbnailList.data(), &ThumbnailList::syncThumbnail);
+    }
+    if ((!isCurrentPage && m_document->bookmarkManager()->isBookmarked(pageNumber)) || (isCurrentPage && m_document->bookmarkManager()->isBookmarked(m_document->viewport()))) {
+        menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-remove"), QIcon::fromTheme(QStringLiteral("edit-delete-bookmark"))), i18n("Remove Bookmark"), this, [this, pageNumber, isCurrentPage]() {
+            if (isCurrentPage) {
+                m_document->bookmarkManager()->removeBookmark(m_document->viewport());
+            } else {
+                m_document->bookmarkManager()->removeBookmark(pageNumber);
+            }
+        });
+    } else {
+        menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add Bookmark"), this, [this, pageNumber, isCurrentPage]() {
+            if (isCurrentPage) {
+                m_document->bookmarkManager()->addBookmark(m_document->viewport());
+            } else {
+                m_document->bookmarkManager()->addBookmark(pageNumber);
+            }
+        });
+    }
+    if (m_pageView->canFitPageWidth()) {
+        menu->addAction(QIcon::fromTheme(QStringLiteral("zoom-fit-best")), i18n("Fit Width"), this, [this, pageNumber]() { m_pageView->fitPageWidth(pageNumber); });
+    }
 }
 
 QAbstractItemModel *Part::annotationsModel() const
