diff --git a/core/page_p.h b/core/page_p.h
index 7d68598..4e30f55 100644
--- a/core/page_p.h
+++ b/core/page_p.h
@@ -12,6 +12,7 @@
 #define _OKULAR_PAGE_PRIVATE_H_
 
 // qt/kde includes
+#include <QColor>
 #include <QMap>
 #include <QString>
 #include <QTransform>
@@ -154,6 +155,10 @@ public:
 
     Page *m_page;
     int m_number;
+    // index of this page in the file loaded by the generator, -1 for pages added by the user
+    int m_sourceNumber;
+    // background color of a page added by the user (only meaningful if m_sourceNumber < 0)
+    QColor m_blankColor;
     Rotation m_orientation;
     double m_width, m_height;
     DocumentPrivate *m_doc;
