diff --git a/core/page.h b/core/page.h
index d4614b1..51fa331 100644
--- a/core/page.h
+++ b/core/page.h
@@ -15,6 +15,8 @@
 #include "okularcore_export.h"
 #include "textpage.h"
 
+#include <QColor>
+
 class QPixmap;
 
 class PagePainter;
@@ -75,6 +77,30 @@ public:
      */
     int number() const;
 
+    /**
+     * Returns the index of this page in the file that was loaded by the
+     * generator. It is equal to number() until pages are inserted, removed or
+     * reordered, and it is -1 for pages that were added by the user (blank pages).
+     * Generators must use this value, not number(), to address their own pages.
+     */
+    int sourceNumber() const;
+
+    /**
+     * Returns whether this page was added by the user and is not backed by the generator.
+     */
+    bool isBlank() const;
+
+    /**
+     * Returns the background color of a blank page.
+     */
+    QColor blankColor() const;
+
+    /**
+     * Turns this page into a blank page of the given background @p color.
+     * Only meant to be used before handing the page to Document::insertPage().
+     */
+    void setBlank(const QColor &color);
+
     /**
      * Returns the orientation of the page as defined by the document.
      */
