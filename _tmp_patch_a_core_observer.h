diff --git a/core/observer.h b/core/observer.h
index 019e3bf..2c8108b 100644
--- a/core/observer.h
+++ b/core/observer.h
@@ -11,6 +11,7 @@
 #ifndef _OKULAR_DOCUMENTOBSERVER_H_
 #define _OKULAR_DOCUMENTOBSERVER_H_
 
+#include <QFlags>
 #include <QList>
 
 #include "okularcore_export.h"
@@ -56,8 +57,12 @@ public:
     enum SetupFlags {
         DocumentChanged = 1,   ///< The document is a new document.
         NewLayoutForPages = 2, ///< All the pages have
-        UrlChanged = 4         ///< The URL has changed @since 1.3
+        UrlChanged = 4,        ///< The URL has changed @since 1.3
+        PagesInserted = 8,     ///< One or more pages have been inserted into the document
+        PagesRemoved = 16,     ///< One or more pages have been removed from the document
+        PagesMoved = 32        ///< Pages have been reordered inside the document
     };
+    Q_DECLARE_FLAGS(SetupFlagsValue, SetupFlags)
 
     /**
      * This method is called whenever the document is initialized or reconstructed.
