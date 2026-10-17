diff --git a/core/page.cpp b/core/page.cpp
index 7f4398a..e3db195 100644
--- a/core/page.cpp
+++ b/core/page.cpp
@@ -68,6 +68,7 @@ static void deleteObjectRects(QList<ObjectRect *> &rects, const QSet<ObjectRect:
 PagePrivate::PagePrivate(Page *page, uint n, double w, double h, Rotation o)
     : m_page(page)
     , m_number(n)
+    , m_sourceNumber(static_cast<int>(n))
     , m_orientation(o)
     , m_width(w)
     , m_height(h)
@@ -234,6 +235,27 @@ int Page::number() const
     return d->m_number;
 }
 
+int Page::sourceNumber() const
+{
+    return d->m_sourceNumber;
+}
+
+bool Page::isBlank() const
+{
+    return d->m_sourceNumber < 0;
+}
+
+QColor Page::blankColor() const
+{
+    return d->m_blankColor;
+}
+
+void Page::setBlank(const QColor &color)
+{
+    d->m_sourceNumber = -1;
+    d->m_blankColor = color.isValid() ? color : QColor(Qt::white);
+}
+
 Rotation Page::orientation() const
 {
     return d->m_orientation;
