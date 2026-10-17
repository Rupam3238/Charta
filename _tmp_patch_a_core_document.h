diff --git a/core/document.h b/core/document.h
index 54930bd..152fb67 100644
--- a/core/document.h
+++ b/core/document.h
@@ -325,6 +325,55 @@ public:
      */
     uint pages() const;

+    /**
+     * Value returned/used where no valid page number exists.
+     */
+    static constexpr int InvalidPage = -1;
+
+    /**
+     * Returns whether pages can be added, removed and reordered in the loaded document.
+     * This is currently only supported for PDF documents.
+     */
+    bool canModifyPages() const;
+
+    /**
+     * Returns all the pages of the document, in display order.
+     */
+    QList<Page *> allPages() const;
+
+    /**
+     * Inserts @p page at position @p index (0-based, 0 <= index <= pages()).
+     * The document takes ownership of the page. Returns false (and deletes nothing)
+     * if the page could not be inserted.
+     */
+    bool insertPage(int index, Page *page);
+
+    /**
+     * Inserts all the @p pages starting at position @p index with a single update of the observers.
+     * The document takes ownership of the pages.
+     */
+    bool insertPages(int index, const QList<Page *> &pages);
+
+    /**
+     * Appends @p page at the end of the document. See insertPage().
+     */
+    bool appendPage(Page *page);
+
+    /**
+     * Removes the page at @p index (0-based). The last remaining page cannot be removed.
+     */
+    bool removePage(int index);
+
+    /**
+     * Removes all the pages in @p indices (0-based) at once. At least one page has to remain.
+     */
+    bool removePages(const QList<int> &indices);
+
+    /**
+     * Moves the page at index @p from so that it ends up at index @p to (both 0-based).
+     */
+    bool movePage(int from, int to);
+
     /**
      * Returns the url of the currently opened document.
      */
