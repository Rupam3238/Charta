diff --git a/core/document.cpp b/core/document.cpp
index cc28061..a83613e 100644
--- a/core/document.cpp
+++ b/core/document.cpp
@@ -1039,7 +1039,10 @@ void DocumentPrivate::performAddPageAnnotation(int page, Annotation *annotation,
 
     // tell the annotation proxy
     if (proxy && proxy->supports(AnnotationProxy::Addition)) {
-        proxy->notifyAddition(annotation, page);
+        const int sourcePage = proxyPageNumber(page);
+        if (sourcePage >= 0) {
+            proxy->notifyAddition(annotation, sourcePage);
+        }
     }
 
     if (orderProxy && (annotation->flags() & Annotation::ExternallyDrawn) && index >= 0) {
@@ -1096,7 +1099,10 @@ void DocumentPrivate::performRemovePageAnnotation(int page, Annotation *annotati
 
         // tell the annotation proxy
         if (proxy && proxy->supports(AnnotationProxy::Removal)) {
-            proxy->notifyRemoval(annotation, page);
+            const int sourcePage = proxyPageNumber(page);
+            if (sourcePage >= 0) {
+                proxy->notifyRemoval(annotation, sourcePage);
+            }
         }
 
         kp->removeAnnotation(annotation); // Also destroys the object
@@ -1179,7 +1185,10 @@ void DocumentPrivate::performModifyPageAnnotation(int page, Annotation *annotati
     // tell the annotation proxy
     if (proxy && proxy->supports(AnnotationProxy::Modification)) {
-        proxy->notifyModification(annotation, page, appearanceChanged);
+        const int sourcePage = proxyPageNumber(page);
+        if (sourcePage >= 0) {
+            proxy->notifyModification(annotation, sourcePage, appearanceChanged);
+        }
     }
 
     // notify observers about the change
@@ -1608,7 +1617,12 @@ void DocumentPrivate::sendGeneratorPixmapRequest()
         // we can not really know if the generator can do async requests
         m_executingPixmapRequests.push_back(request);
         m_pixmapRequestsMutex.unlock();
-        m_generator->generatePixmap(request);
+        if (request->page()->isBlank()) {
+            // pages added by the user are not backed by the generator
+            generateBlankPagePixmap(request);
+        } else {
+            m_generator->generatePixmap(request);
+        }
     } else {
         m_pixmapRequestsMutex.unlock();
         // pino (7/4/2006): set the polling interval from 10 to 30
@@ -2292,6 +2306,247 @@ void DocumentPrivate::clearAndWaitForRequests()
     } while (startEventLoop);
 }
 
+void DocumentPrivate::generateBlankPagePixmap(PixmapRequest *request)
+{
+    QSize size(request->width(), request->height());
+    if (request->isTile()) {
+        size = request->normalizedRect().geometry(request->width(), request->height()).size();
+    }
+    size = size.expandedTo(QSize(1, 1));
+
+    QImage img(size, QImage::Format_RGB32);
+    img.fill(request->page()->blankColor());
+
+    request->page()->setPixmap(request->observer(), new QPixmap(QPixmap::fromImage(img)), request->normalizedRect());
+    requestDone(request);
+}
+
+int DocumentPrivate::proxyPageNumber(int page) const
+{
+    const Page *p = m_pagesVector.value(page, nullptr);
+    return p ? p->d->m_sourceNumber : -1;
+}
+
+void DocumentPrivate::renumberPages()
+{
+    for (int i = 0; i < m_pagesVector.size(); ++i) {
+        m_pagesVector.at(i)->d->m_number = i;
+    }
+}
+
+QList<int> DocumentPrivate::beginPageStructureChange(const QList<int> &oldToNew)
+{
+    // nothing may be running or queued for the old page indices
+    clearAndWaitForRequests();
+
+    // drop searches (they keep page indices) and the undo history (it refers to pages and annotations)
+    const QList<int> searchIds = m_searches.keys();
+    for (const int id : searchIds) {
+        m_parent->resetSearch(id);
+    }
+    if (m_undoStack) {
+        m_undoStack->clear();
+    }
+
+    // bookmarks are stored by page index: take them off the old indices, they are put back on the new ones later
+    QList<int> bookmarkedPages;
+    if (m_bookmarkManager) {
+        for (int old = 0; old < oldToNew.size(); ++old) {
+            if (m_bookmarkManager->isBookmarked(old)) {
+                m_bookmarkManager->removeBookmark(old);
+                if (oldToNew.at(old) >= 0) {
+                    bookmarkedPages.append(oldToNew.at(old));
+                }
+            }
+        }
+    }
+    return bookmarkedPages;
+}
+
+void DocumentPrivate::endPageStructureChange(const QList<int> &oldToNew, const QList<int> &bookmarkedPages, int setupFlag)
+{
+    const int pageCount = m_pagesVector.size();
+    auto mapped = [&oldToNew](int old) { return (old >= 0 && old < oldToNew.size()) ? oldToNew.at(old) : -1; };
+
+    // pixmap memory accounting
+    for (auto it = m_allocatedPixmaps.begin(); it != m_allocatedPixmaps.end();) {
+        AllocatedPixmap *p = *it;
+        const int newPage = mapped(p->page);
+        if (newPage < 0) {
+            m_allocatedPixmapsTotalMemory -= p->memory;
+            delete p;
+            it = m_allocatedPixmaps.erase(it);
+        } else {
+            p->page = newPage;
+            ++it;
+        }
+    }
+
+    // text page memory accounting
+    QList<int> textFifo;
+    for (const int old : std::as_const(m_allocatedTextPagesFifo)) {
+        const int newPage = mapped(old);
+        if (newPage >= 0) {
+            textFifo.append(newPage);
+        }
+    }
+    m_allocatedTextPagesFifo = textFifo;
+
+    // visible rects are refreshed by the views as soon as they relayout
+    qDeleteAll(m_pageRects);
+    m_pageRects.clear();
+
+    // keep the current viewport on the same page (or the nearest one if it has been removed)
+    DocumentViewport viewport = *m_viewportIterator;
+    int newCurrent = mapped(viewport.pageNumber);
+    if (newCurrent < 0) {
+        newCurrent = qBound(0, viewport.pageNumber, pageCount - 1);
+    }
+    viewport.pageNumber = newCurrent;
+    m_viewportHistory.clear();
+    m_viewportHistory.push_back(viewport);
+    m_viewportIterator = m_viewportHistory.begin();
+
+    foreachObserverD(notifySetup(m_pagesVector, DocumentObserver::NewLayoutForPages | setupFlag));
+    foreachObserverD(notifyViewportChanged(false));
+
+    if (m_bookmarkManager) {
+        for (const int page : bookmarkedPages) {
+            m_bookmarkManager->addBookmark(page);
+        }
+    }
+}
+
+bool Document::canModifyPages() const
+{
+    return d->m_generator && !d->m_generatorName.isEmpty() && d->m_generatorName.contains(QLatin1String("poppler"), Qt::CaseInsensitive);
+}
+
+QList<Page *> Document::allPages() const
+{
+    return d->m_pagesVector;
+}
+
+bool Document::insertPage(int index, Page *page)
+{
+    return insertPages(index, QList<Page *>() << page);
+}
+
+bool Document::insertPages(int index, const QList<Page *> &pages)
+{
+    if (pages.isEmpty() || !canModifyPages() || index < 0 || index > d->m_pagesVector.size()) {
+        return false;
+    }
+    for (const Page *page : pages) {
+        if (!page) {
+            return false;
+        }
+    }
+
+    const int oldCount = d->m_pagesVector.size();
+    const int added = pages.size();
+    QList<int> oldToNew(oldCount);
+    for (int i = 0; i < oldCount; ++i) {
+        oldToNew[i] = (i < index) ? i : i + added;
+    }
+
+    const QList<int> bookmarked = d->beginPageStructureChange(oldToNew);
+
+    int position = index;
+    for (Page *page : pages) {
+        page->d->m_doc = d;
+        page->d->rotateAt(d->m_rotation);
+        d->m_pagesVector.insert(position++, page);
+    }
+    d->renumberPages();
+
+    d->endPageStructureChange(oldToNew, bookmarked, DocumentObserver::PagesInserted);
+    return true;
+}
+
+bool Document::appendPage(Page *page)
+{
+    return insertPage(d->m_pagesVector.size(), page);
+}
+
+bool Document::removePage(int index)
+{
+    return removePages(QList<int>() << index);
+}
+
+bool Document::removePages(const QList<int> &indices)
+{
+    if (!canModifyPages()) {
+        return false;
+    }
+
+    const int oldCount = d->m_pagesVector.size();
+    QSet<int> toRemove;
+    for (const int index : indices) {
+        if (index >= 0 && index < oldCount) {
+            toRemove.insert(index);
+        }
+    }
+    if (toRemove.isEmpty() || toRemove.size() >= oldCount) {
+        // nothing to do, or it would remove every page
+        return false;
+    }
+
+    QList<int> oldToNew(oldCount);
+    int next = 0;
+    for (int i = 0; i < oldCount; ++i) {
+        oldToNew[i] = toRemove.contains(i) ? -1 : next++;
+    }
+
+    const QList<int> bookmarked = d->beginPageStructureChange(oldToNew);
+
+    QList<Page *> removedPages;
+    QList<Page *> kept;
+    kept.reserve(oldCount - toRemove.size());
+    for (int i = 0; i < oldCount; ++i) {
+        if (toRemove.contains(i)) {
+            removedPages.append(d->m_pagesVector.at(i));
+        } else {
+            kept.append(d->m_pagesVector.at(i));
+        }
+    }
+    d->m_pagesVector = kept;
+    d->renumberPages();
+
+    d->endPageStructureChange(oldToNew, bookmarked, DocumentObserver::PagesRemoved);
+
+    // the observers do not know the removed pages any more: it is safe to delete them now
+    qDeleteAll(removedPages);
+    return true;
+}
+
+bool Document::movePage(int from, int to)
+{
+    const int count = d->m_pagesVector.size();
+    if (!canModifyPages() || from == to || from < 0 || from >= count || to < 0 || to >= count) {
+        return false;
+    }
+
+    QList<int> oldToNew(count);
+    for (int i = 0; i < count; ++i) {
+        oldToNew[i] = i;
+    }
+    oldToNew.move(from, to);
+    // oldToNew now lists the old index found at each new position: invert it
+    QList<int> inverse(count);
+    for (int newIndex = 0; newIndex < count; ++newIndex) {
+        inverse[oldToNew.at(newIndex)] = newIndex;
+    }
+
+    const QList<int> bookmarked = d->beginPageStructureChange(inverse);
+
+    d->m_pagesVector.move(from, to);
+    d->renumberPages();
+
+    d->endPageStructureChange(inverse, bookmarked, DocumentObserver::PagesMoved);
+    return true;
+}
+
 int DocumentPrivate::findFieldPageNumber(Okular::FormField *field)
 {
     // Lookup the page of the FormField
@@ -3510,7 +3765,7 @@ void Document::requestPixmaps(const QList<PixmapRequest *> &requests, PixmapRequ
 void Document::requestTextPage(uint pageNumber)
 {
     Page *kp = d->m_pagesVector[pageNumber];
-    if (!d->m_generator || !kp) {
+    if (!d->m_generator || !kp || kp->isBlank()) {
         return;
     }
 
@@ -3540,7 +3795,10 @@ void DocumentPrivate::endAnnotationChangeBatch()
             Annotation *annotation = it.key();
             const int page = it.value().first;
             if (m_pagesVector.value(page, nullptr) && annotation->d_ptr->m_page) { // still attached to its page
-                proxy->notifyModification(annotation, page, it.value().second);
+                const int sourcePage = proxyPageNumber(page);
+                if (sourcePage >= 0) {
+                    proxy->notifyModification(annotation, sourcePage, it.value().second);
+                }
             }
         }
     }
@@ -3764,7 +4022,10 @@ void Document::beginAnnotationLiveMove(const QList<AnnotationMoveItem> &items)
             continue;
         }
         if (proxy && proxy->supports(AnnotationProxy::Modification)) {
-            proxy->notifyModification(item.annotation, item.page, true); // BeingMoved is set: only hides
+            const int sourcePage = d->proxyPageNumber(item.page);
+            if (sourcePage >= 0) {
+                proxy->notifyModification(item.annotation, sourcePage, true); // BeingMoved is set: only hides
+            }
         }
         if (item.annotation->flags() & Annotation::ExternallyDrawn) {
             d->refreshAnnotationPixmaps(item.page);
