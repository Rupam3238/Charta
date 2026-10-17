diff --git a/core/document_p.h b/core/document_p.h
index 7e76bb4..44d8c3f 100644
--- a/core/document_p.h
+++ b/core/document_p.h
@@ -226,6 +226,18 @@ public:
 
     void clearAndWaitForRequests();
 
+    // Page structure editing (insert / remove / move pages)
+    void renumberPages();
+    // Returns the page index used by the generator (and its annotation proxy) for @p page, or -1 for blank pages
+    int proxyPageNumber(int page) const;
+    // Stops pending work that refers to page indices and removes the bookmarks of the old pages. Returns the
+    // new indices of the pages that were bookmarked.
+    QList<int> beginPageStructureChange(const QList<int> &oldToNew);
+    // Remaps all the state that is keyed by page index and tells the observers about the new layout
+    void endPageStructureChange(const QList<int> &oldToNew, const QList<int> &bookmarkedPages, int setupFlag);
+    // Fills a pixmap request for a blank page without involving the generator
+    void generateBlankPagePixmap(PixmapRequest *request);
+
     OKULARCORE_EXPORT static QString evaluateKeystrokeEventChange(const QString &oldVal, const QString &newVal, int selStart, int selEnd);
 
     /*
