diff --git a/part/thumbnaillist.cpp b/part/thumbnaillist.cpp
index 6c66124..4d26947 100644
--- a/part/thumbnaillist.cpp
+++ b/part/thumbnaillist.cpp
@@ -9,6 +9,7 @@
 // qt/kde includes
 #include <QAction>
 #include <QApplication>
+#include <QDialog>
 #include <QIcon>
 #include <QMenu>
 #include <QPainter>
@@ -19,6 +20,8 @@
 #include <QTimer>
 #include <QVBoxLayout>
 
+#include <algorithm>
 
 #include <KActionCollection>
 #include <KLocalizedString>
 #include <KTitleWidget>
@@ -84,6 +87,14 @@ public:
     // Drag reorder variables
     int m_dragFromIndex;
     QPoint m_dragStartPos;
+    bool m_reordering;
+    // the current mouse press only changed the selection (Ctrl / Shift click)
+    bool m_selectionClick;
+
+    // selection helpers
+    void updateSelectionHighlights();
+    void selectOnly(int page);
+    void selectRange(int first, int last);
+
     // resize thumbnails to fit the width
     void viewportResizeEvent(QResizeEvent *);
@@ -129,6 +140,8 @@ public:
     void resizeFitWidth(int width);
     // set thumbnail's selected state
     void setSelected(bool selected);
+    // set whether the thumbnail is part of the multi page selection
+    void setPageSelected(bool selected);
     // set the visible rect of the current page
     void setVisibleRect(const Okular::NormalizedRect &rect);
 
@@ -217,8 +217,10 @@ private:
     ThumbnailListPrivate *m_parent;
     const Okular::Page *m_page;
     bool m_selected;
+    bool m_pageSelected;
     int m_pixmapWidth, m_pixmapHeight;
     int m_labelHeight, m_labelNumber;
     Okular::NormalizedRect m_visibleRect;
@@ -217,8 +231,10 @@ ThumbnailListPrivate::ThumbnailListPrivate(ThumbnailList *qq, Okular::Document *
     , m_vectorIndex(0)
     , m_pageCurrentlyGrabbed(0)
     , m_lastSelectedPage(-1)
-    , m_multiSelect(false)
+    , m_multiSelect(true)
     , m_dragFromIndex(-1)
+    , m_reordering(false)
+    , m_selectionClick(false)
 {
     setMouseTracking(true);
     m_mouseGrabItem = nullptr;
@@ -299,16 +315,18 @@ ThumbnailList::~ThumbnailList()
 // BEGIN DocumentObserver inherited methods
 void ThumbnailList::notifySetup(const QList<Okular::Page *> &pages, int setupFlags)
 {
-    // Preserve selection across rebuilds
-    QSet<int> preservedSelection;
-    if (!(setupFlags & Okular::DocumentObserver::DocumentChanged)) {
-        preservedSelection = d->m_selectedPageNumbers;
+    // page numbers change when pages are added, removed or moved: the selection is not valid any more
+    const bool pagesChanged = setupFlags & (Okular::DocumentObserver::PagesInserted | Okular::DocumentObserver::PagesRemoved | Okular::DocumentObserver::PagesMoved);
+    if ((setupFlags & Okular::DocumentObserver::DocumentChanged) || pagesChanged) {
+        d->m_selectedPageNumbers.clear();
+        d->m_lastSelectedPage = -1;
     }
+    const int previousScrollValue = verticalScrollBar()->value();
 
     // if there was a widget selected, save its pagenumber to restore
     // its selection (if available in the new set of pages)
     int prevPage = -1;
-    if (!(setupFlags & Okular::DocumentObserver::DocumentChanged) && d->m_selected) {
+    if (!(setupFlags & Okular::DocumentObserver::DocumentChanged) && !pagesChanged && d->m_selected) {
         prevPage = d->m_selected->page()->number();
     } else {
         prevPage = d->m_document->viewport().pageNumber;
@@ -367,21 +385,8 @@ void ThumbnailList::notifySetup(const QList<Okular::Page *> &pages, int setupFla
         }
     }
 
-    // Restore selection after thumbnail regeneration
-    if (!preservedSelection.isEmpty()) {
-        d->m_selectedPageNumbers = preservedSelection;
-        // Update selection states of all thumbnails
-        for (ThumbnailWidget *thumb : d->m_thumbnails) {
-            thumb->setSelected(d->m_selectedPageNumbers.contains(thumb->pageNumber()));
-        }
-        // Update m_selected to the first selected item if any
-        for (ThumbnailWidget *thumb : d->m_thumbnails) {
-            if (d->m_selectedPageNumbers.contains(thumb->pageNumber())) {
-                d->m_selected = thumb;
-                break;
-            }
-        }
-    }
+    // Restore the multi page selection after the thumbnails have been regenerated
+    d->updateSelectionHighlights();
 
     // update scrollview's contents size (sets scrollbars limits)
     height -= this->style()->layoutSpacing(QSizePolicy::Frame, QSizePolicy::Frame, Qt::Vertical);
@@ -389,7 +394,8 @@ void ThumbnailList::notifySetup(const QList<Okular::Page *> &pages, int setupFla
 
     // enable scrollbar when there's something to scroll
     verticalScrollBar()->setEnabled(viewport()->height() < height);
-    verticalScrollBar()->setValue(centerHeight - viewport()->height() / 2);
+    // when pages are added, removed or moved keep the list where it is instead of centering the current page
+    verticalScrollBar()->setValue(pagesChanged ? previousScrollValue : centerHeight - viewport()->height() / 2);
 
     // request for thumbnail generation
     d->delayedRequestVisiblePixmaps(200);
@@ -492,14 +498,37 @@ bool ThumbnailList::canUnloadPixmap(int pageNumber) const
 }
 // END DocumentObserver inherited methods
 
-QList<int> ThumbnailList::selectedPageNumbers() const {
-    QList<int> result;
-    for (int page : d->m_selectedPageNumbers) {
-        result.append(page);
-    }
+QList<int> ThumbnailList::selectedPageNumbers() const
+{
+    QList<int> result = d->m_selectedPageNumbers.values();
+    std::sort(result.begin(), result.end());
     return result;
 }
 
+void ThumbnailListPrivate::updateSelectionHighlights()
+{
+    for (ThumbnailWidget *thumb : std::as_const(m_thumbnails)) {
+        thumb->setPageSelected(m_selectedPageNumbers.contains(thumb->pageNumber()));
+    }
+}
+
+void ThumbnailListPrivate::selectOnly(int page)
+{
+    m_selectedPageNumbers.clear();
+    m_selectedPageNumbers.insert(page);
+    m_lastSelectedPage = page;
+    updateSelectionHighlights();
+}
+
+void ThumbnailListPrivate::selectRange(int first, int last)
+{
+    m_selectedPageNumbers.clear();
+    for (int i = qMin(first, last); i <= qMax(first, last); ++i) {
+        m_selectedPageNumbers.insert(i);
+    }
+    updateSelectionHighlights();
+}
+
 void ThumbnailList::updateWidgets()
 {
     // Update all visible widgets
@@ -604,6 +633,11 @@ void ThumbnailList::keyPressEvent(QKeyEvent *keyEvent)
         return;
     }
 
+    if (keyEvent->key() == Qt::Key_Delete && d->m_document->canModifyPages() && !d->m_selectedPageNumbers.isEmpty()) {
+        Q_EMIT pagesDeleted(selectedPageNumbers());
+        return;
+    }
+
     int nextPage = -1;
     if (keyEvent->key() == Qt::Key_Up) {
         if (!d->m_selected) {
@@ -763,6 +797,7 @@ ThumbnailWidget::ThumbnailWidget(ThumbnailListPrivate *parent, const Okular::Pag
     : m_parent(parent)
     , m_page(page)
     , m_selected(false)
+    , m_pageSelected(false)
     , m_pixmapWidth(10)
     , m_pixmapHeight(10)
 {
@@ -786,6 +821,14 @@ void ThumbnailWidget::setSelected(bool selected)
     }
 }
 
+void ThumbnailWidget::setPageSelected(bool selected)
+{
+    if (m_pageSelected != selected) {
+        m_pageSelected = selected;
+        update();
+    }
+}
+
 void ThumbnailWidget::setVisibleRect(const Okular::NormalizedRect &rect)
 {
     if (rect == m_visibleRect) {
@@ -804,89 +847,82 @@ void ThumbnailListPrivate::mousePressEvent(QMouseEvent *e)
         return;
     }
 
-    const QRect r = item->visibleRect();
-    const int margin = ThumbnailWidget::margin();
-    const QPoint p = e->pos() - item->pos();
+    m_mouseGrabPos.setX(0);
+    m_mouseGrabPos.setY(0);
+    m_mouseGrabItem = nullptr;
+    m_reordering = false;
+    m_selectionClick = false;
+    m_dragFromIndex = -1;
+    m_dragStartPos = QPoint();
+
     const int clickedPageNumber = item->pageNumber();
 
-    // Handle multi-select modifiers
-    if (m_multiSelect) {
+    if (e->button() == Qt::RightButton) {
+        // the context menu works on the selection: a click outside of it selects only the clicked page
+        if (m_multiSelect && !m_selectedPageNumbers.contains(clickedPageNumber)) {
+            selectOnly(clickedPageNumber);
+        }
+        return;
+    }
+
+    if (m_multiSelect && e->button() == Qt::LeftButton) {
         if (e->modifiers() & Qt::ControlModifier) {
-            // Ctrl/Cmd+click: toggle selection of clicked item
+            // Ctrl+click toggles the clicked page
             if (m_selectedPageNumbers.contains(clickedPageNumber)) {
                 m_selectedPageNumbers.remove(clickedPageNumber);
-                item->setSelected(false);
             } else {
                 m_selectedPageNumbers.insert(clickedPageNumber);
-                item->setSelected(true);
-            }
-            m_lastSelectedPage = clickedPageNumber;
-        } else if (e->modifiers() & Qt::ShiftModifier && m_lastSelectedPage != -1) {
-            // Shift+click: select range from last selected to clicked
-            int start = qMin(m_lastSelectedPage, clickedPageNumber);
-            int end = qMax(m_lastSelectedPage, clickedPageNumber);
-
-            // Clear current selection and select range
-            m_selectedPageNumbers.clear();
-            for (int i = start; i <= end; ++i) {
-                m_selectedPageNumbers.insert(i);
-            }
-
-            // Update selection states of all thumbnails
-            for (ThumbnailWidget *thumb : m_thumbnails) {
-                thumb->setSelected(m_selectedPageNumbers.contains(thumb->pageNumber()));
             }
-
             m_lastSelectedPage = clickedPageNumber;
-        } else {
-            // Regular click: select only this item
-            if (!m_selectedPageNumbers.contains(clickedPageNumber)) {
-                m_selectedPageNumbers.clear();
-                m_selectedPageNumbers.insert(clickedPageNumber);
-                m_lastSelectedPage = clickedPageNumber;
-
-                // Update selection states
-                for (ThumbnailWidget *thumb : m_thumbnails) {
-                    thumb->setSelected(m_selectedPageNumbers.contains(thumb->pageNumber()));
-                }
-            }
+            updateSelectionHighlights();
+            m_selectionClick = true;
+            return;
         }
-    } else {
-        // Single select mode (original behavior)
-        if (e->button() != Qt::RightButton && r.contains(p - QPoint(margin / 2, margin / 2))) {
-            m_mouseGrabPos.setX(0);
-            m_mouseGrabPos.setY(0);
-            m_mouseGrabItem = item;
-            m_pageCurrentlyGrabbed = item->pageNumber();
-            m_mouseGrabItem = item;
-        } else {
-            m_mouseGrabPos.setX(0);
-            m_mouseGrabPos.setY(0);
-            m_mouseGrabItem = nullptr;
+        if ((e->modifiers() & Qt::ShiftModifier) && m_lastSelectedPage != -1) {
+            // Shift+click selects the range from the last clicked page
+            selectRange(m_lastSelectedPage, clickedPageNumber);
+            m_selectionClick = true;
+            return;
         }
     }
 
-    // Start drag operation if not a right-click and we're in single select mode
-    // or if we clicked on the drag area (not just selecting)
-    if (!m_multiSelect || !(e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
-        if (e->button() != Qt::RightButton && r.contains(p - QPoint(margin / 2, margin / 2))) {
-            m_mouseGrabPos.setX(0);
-            m_mouseGrabPos.setY(0);
-            m_mouseGrabItem = item;
-            m_pageCurrentlyGrabbed = item->pageNumber();
-            m_mouseGrabItem = item;
+    const QRect r = item->visibleRect();
+    const int margin = ThumbnailWidget::margin();
+    const QPoint p = e->pos() - item->pos();
 
-            // Initialize drag start position
+    if (e->button() != Qt::RightButton && r.contains(p - QPoint(margin / 2, margin / 2))) {
+        // the part of the thumbnail that is shown in the page view can be dragged to scroll it
+        m_mouseGrabItem = item;
+        m_pageCurrentlyGrabbed = item->pageNumber();
+    } else if (m_multiSelect && e->button() == Qt::LeftButton) {
+        selectOnly(clickedPageNumber);
+        if (m_document->canModifyPages()) {
+            // the rest of the thumbnail can be dragged to move the page
+            m_dragStartPos = e->pos();
+            m_dragFromIndex = clickedPageNumber;
         }
     }
+
+    CursorWrapHelper::startDrag();
 }
 
 void ThumbnailListPrivate::mouseReleaseEvent(QMouseEvent *e)
 {
+    const bool wasReordering = m_reordering;
+    const bool onlySelected = m_selectionClick;
+    m_reordering = false;
+    m_selectionClick = false;
+    m_dragFromIndex = -1;
+    m_dragStartPos = QPoint();
+
+    if (wasReordering || onlySelected || e->button() == Qt::RightButton) {
+        m_mouseGrabItem = nullptr;
+        m_mouseGrabPos.setX(0);
+        m_mouseGrabPos.setY(0);
+        setCursor(Qt::ArrowCursor);
+        return;
+    }
+
     ThumbnailWidget *item = itemFor(e->pos());
     m_mouseGrabItem = item;
     if (!item) { // mouse on the spacing between items
@@ -908,10 +944,6 @@ void ThumbnailListPrivate::mouseReleaseEvent(QMouseEvent *e)
     setCursor(Qt::OpenHandCursor);
     m_mouseGrabPos.setX(0);
     m_mouseGrabPos.setY(0);
-
-    // Reset drag state
-    m_dragStartPos = QPoint();
-    m_dragFromIndex = -1;
 }
 
 void ThumbnailListPrivate::mouseMoveEvent(QMouseEvent *e)
@@ -935,29 +967,32 @@ void ThumbnailListPrivate::mouseMoveEvent(QMouseEvent *e)
         e->ignore();
         return;
     }
+    // dragging a thumbnail moves its page
+    if (m_dragFromIndex >= 0 && (e->buttons() & Qt::LeftButton)) {
+        if (!m_reordering && (e->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
+            return;
+        }
+        m_reordering = true;
+        setCursor(Qt::ClosedHandCursor);
+
+        const ThumbnailWidget *target = itemFor(e->pos());
+        if (target && target->pageNumber() != m_dragFromIndex) {
+            const int from = m_dragFromIndex;
+            const int to = target->pageNumber();
+            // the thumbnails are rebuilt while the page is moved: do not touch them afterwards
+            m_dragFromIndex = to;
+            Q_EMIT q->pagesReordered(from, to);
+            selectOnly(to);
+        }
+        return;
+    }
+
     // no item under the mouse or previously selected
     if (!m_mouseGrabItem) {
         e->ignore();
         return;
     }
 
-    // Drag reorder functionality
-    if (m_mouseGrabItem && !m_dragStartPos.isNull()) {
-        // Check if we've moved far enough to start a drag operation
-        int dragDistance = (e->pos() - m_dragStartPos).manhattanLength();
-        if (dragDistance >= QApplication::startDragDistance()) {
-            // We're dragging - reorder the thumbnail
-            const ThumbnailWidget *item = itemFor(e->pos());
-            if (item && item != m_mouseGrabItem) {
-                int toIndex = item->pageNumber();
-                // Emit the reorder signal
-                Q_EMIT q->pagesReordered(m_dragFromIndex, toIndex);
-                // Update our drag source index for continued dragging
-                m_dragFromIndex = toIndex;
-            }
-        }
-    }
-
     const QRect r = m_mouseGrabItem->rect();
     if (!m_mouseGrabPos.isNull()) {
         const QPoint mousePos = e->pos();
@@ -1066,57 +1101,44 @@ void ThumbnailListPrivate::contextMenuEvent(QContextMenuEvent *e)
         return;
     }
 
+    if (!m_document->canModifyPages()) {
+        // pages cannot be edited in this document: only the generic page menu
+        Q_EMIT q->rightClick(item->page(), e->globalPos());
+        return;
+    }
+
+    const int clickedPage = item->pageNumber();
+    const Okular::Page *clickedPagePtr = item->page();
+    const int pageCount = static_cast<int>(m_document->pages());
+
+    // the menu acts on the selection; a page outside of it selects only itself
+    if (!m_selectedPageNumbers.contains(clickedPage)) {
+        selectOnly(clickedPage);
+    }
+    const QList<int> selectedPages = q->selectedPageNumbers();
+
+    QMenu menu;
-    QList<int> selectedPages = q->selectedPageNumbers();
-    bool hasSelection = !selectedPages.isEmpty();
-    bool isItemSelected = selectedPages.contains(item->pageNumber());
-
-    // Add actions based on selection state
-    if (hasSelection) {
-        if (selectedPages.size() == 1) {
-            // Single page selected
-            menu.addAction(i18n("Delete this page"), this, [this, item]() {
-                Q_EMIT q->pagesDeleted(QList<int>() << item->pageNumber());
-            });
-            menu.addAction(i18n("Add page before"), this, [this, item]() {
-                Q_EMIT q->pageAddedBefore(item->pageNumber());
-            });
-            menu.addAction(i18n("Add page after"), this, [this, item]() {
-                Q_EMIT q->pageAddedAfter(item->pageNumber());
-            });
-        } else {
-            // Multiple pages selected
-            menu.addAction(i18n("Delete %1 pages").arg(selectedPages.size()), this, [this, selectedPages]() {
-                Q_EMIT q->pagesDeleted(selectedPages);
-            });
-        }
-        menu.addSeparator();
-    }
-
-    // Always available actions
-    menu.addAction(i18n("Add page…"), this, [this]() {
-        // In a real implementation, this would open a NewPageDialog
-        // For now, we'll create a default page
-        NewPageData data;
-        data.width = 595.0;  // A4 width in points
-        data.height = 842.0; // A4 height in points
-        data.backgroundColor = Qt::white;
-        data.count = 1;
-        Q_EMIT q->pageAdded(data);
-    });
-    menu.addAction(i18n("Select all"), this, [this]() {
-        // Select all pages
-        QList<int> allPages;
-        for (int i = 0; i < m_document->pages(); ++i) {
-            allPages.append(i);
-        }
-        m_selectedPageNumbers = QSet<int>(allPages.begin(), allPages.end());
-        // Update thumbnail selection states
-        for (ThumbnailWidget *thumb : m_thumbnails) {
-            thumb->setSelected(m_selectedPageNumbers.contains(thumb->pageNumber()));
+    if (selectedPages.size() > 1) {
+        QAction *deleteAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18np("Delete %1 page", "Delete %1 pages", selectedPages.size()), this, [this, selectedPages]() { Q_EMIT q->pagesDeleted(selectedPages); });
+        deleteAction->setEnabled(selectedPages.size() < pageCount);
+    } else {
+        QAction *deleteAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete this page"), this, [this, clickedPage]() { Q_EMIT q->pagesDeleted(QList<int>() << clickedPage); });
+        deleteAction->setEnabled(pageCount > 1);
+        menu.addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Add page before"), this, [this, clickedPage]() { Q_EMIT q->pageAddedBefore(clickedPage); });
+        menu.addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Add page after"), this, [this, clickedPage]() { Q_EMIT q->pageAddedAfter(clickedPage); });
+    }
+    menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Add page…"), this, [this]() {
+        Okular::NewPageDialog dialog(q);
+        if (dialog.exec() == QDialog::Accepted) {
+            Q_EMIT q->pageAdded(dialog.data());
         }
     });
+    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-select-all")), i18n("Select all"), this, [this, pageCount]() { selectRange(0, pageCount - 1); });
+
+    // let the owner add the generic page actions (bookmarks, fit width, ...)
+    Q_EMIT q->menuRequested(&menu, clickedPagePtr, e->globalPos());
 
+    // the thumbnails may be rebuilt by the chosen action: do not use them after this point
     menu.exec(e->globalPos());
 }
 
@@ -1127,7 +1149,13 @@ void ThumbnailWidget::paint(QPainter &p, const QRect _clipRect)
     const QPalette pal = m_parent->palette();
 
     // draw the bottom label + highlight mark
-    const QColor fillColor = m_selected ? pal.color(QPalette::Active, QPalette::Highlight) : pal.color(QPalette::Active, QPalette::Base);
+    const QColor baseColor = pal.color(QPalette::Active, QPalette::Base);
+    const QColor highlightColor = pal.color(QPalette::Active, QPalette::Highlight);
+    QColor fillColor = m_selected ? highlightColor : baseColor;
+    if (m_pageSelected && !m_selected) {
+        // pages of the multi selection get a softer version of the highlight color
+        fillColor = QColor((baseColor.red() + highlightColor.red()) / 2, (baseColor.green() + highlightColor.green()) / 2, (baseColor.blue() + highlightColor.blue()) / 2);
+    }
     p.fillRect(clipRect, fillColor);
     p.setPen(m_selected ? pal.color(QPalette::Active, QPalette::HighlightedText) : pal.color(QPalette::Active, QPalette::Text));
     p.drawText(0, m_pixmapHeight + (m_margin - 3), itemWidth, m_labelHeight, Qt::AlignCenter, QString::number(m_labelNumber));
