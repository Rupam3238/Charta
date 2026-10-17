diff --git a/part/thumbnaillist.h b/part/thumbnaillist.h
index 0b34499..67338f6 100644
--- a/part/thumbnaillist.h
+++ b/part/thumbnaillist.h
@@ -7,11 +7,15 @@
 #ifndef _OKULAR_THUMBNAILLIST_H_
 #define _OKULAR_THUMBNAILLIST_H_
 
+#include <QList>
+#include <QPoint>
 #include <QScrollArea>
 #include <QToolBar>
 
 #include "core/observer.h"
+#include "newpagedialog.h"
 
+class QMenu;
 class ThumbnailListPrivate;
 
 namespace Okular
@@ -50,6 +54,9 @@ public:
     // show current page in Thumbnails view
     void syncThumbnail();
 
+    // the (0-based) numbers of the pages selected in the list, in ascending order
+    QList<int> selectedPageNumbers() const;
+
 public Q_SLOTS:
     // these are connected to ThumbnailController buttons
     void slotFilterBookmarks(bool filterOn);
@@ -64,6 +71,16 @@ protected:
 Q_SIGNALS:
     void rightClick(const Okular::Page *, const QPoint);
 
+    // Document builder: all the page numbers are 0-based
+    void pagesDeleted(const QList<int> &pageNumbers);
+    void pageAddedBefore(int pageNumber);
+    void pageAddedAfter(int pageNumber);
+    void pageAdded(const Okular::NewPageDialog::NewPageData &data);
+    void pagesReordered(int fromIndex, int toIndex);
+
+    // emitted while the page menu is built, so that the owner can append its own actions
+    void menuRequested(QMenu *menu, const Okular::Page *page, const QPoint &globalPos);
+
 private:
     friend class ThumbnailListPrivate;
     ThumbnailListPrivate *d;
