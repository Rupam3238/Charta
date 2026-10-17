diff --git a/part/part.h b/part/part.h
index 6d8f8ac..43db7a0 100644
--- a/part/part.h
+++ b/part/part.h
@@ -39,6 +39,7 @@
 #include "../core/observer.h"
 #include "../interfaces/viewerinterface.h"
 #include "../kdocumentviewer.h"
+#include "newpagedialog.h"
 
 #include "okularpart_export.h"
 
@@ -188,13 +189,6 @@ public Q_SLOTS: // dbus
     Q_SCRIPTABLE void slotCopyTextSelectionOrAnnotation();
     Q_SCRIPTABLE void slotPasteAnnotation();
 
-// Document Builder Page Management Slots
-    void slotDeletePages(const QList<int> &pageNumbers);
-    void slotInsertPageBefore(int pageNumber);
-    void slotInsertPageAfter(int pageNumber);
-    void slotAddPage();
-    void slotReorderPages(int fromIndex, int toIndex);
-
 Q_SIGNALS:
     void enablePrintAction(bool enable);
     void openSourceReference(const QString &absFileName, int line, int column); /* NOLINT(bugprone-derived-method-shadowing-base-method) cppcheck-suppress duprInheritedMember; see viewerinterface*/
@@ -498,7 +492,17 @@ private:
     // Current value of maxRecentItems to detect value change, and inform shell
     int m_maxRecentItems;
 
+    void insertBlankPages(int index, const NewPageDialog::NewPageData &data);
+
 private Q_SLOTS:
+    // Document builder: page management, all the page numbers are 0-based
+    void slotDeletePages(const QList<int> &pageNumbers);
+    void slotInsertPageBefore(int pageNumber);
+    void slotInsertPageAfter(int pageNumber);
+    void slotAddPage(const NewPageDialog::NewPageData &data);
+    void slotReorderPages(int fromIndex, int toIndex);
+    void slotThumbnailMenu(QMenu *menu, const Okular::Page *page, const QPoint &globalPos);
+
     void slotAccessibilityPreferences();
     void slotAnnotationPreferences();
     void slotHandleActivatedSourceReference(const QString &absFileName, int line, int col, bool *handled);
