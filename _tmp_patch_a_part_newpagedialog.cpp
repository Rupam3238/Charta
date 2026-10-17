diff --git a/part/newpagedialog.cpp b/part/newpagedialog.cpp
index da9388c..7f16d8b 100644
--- a/part/newpagedialog.cpp
+++ b/part/newpagedialog.cpp
@@ -7,6 +7,7 @@
 #include "newpagedialog.h"
 
 // qt/kde includes
+#include <QGridLayout>
 #include <QLabel>
 #include <QSpinBox>
 #include <QDoubleSpinBox>
@@ -18,6 +19,8 @@
 #include <QColorDialog>
 #include <KLocalizedString>
 
+#include <utility>
 
 // local includes
 #include "settings.h"
@@ -175,4 +178,4 @@ NewPageDialog::NewPageData NewPageDialog::data() const
     return data;
 }
 
-} // namespace Okular
\ No newline at end of file
+} // namespace Okular
