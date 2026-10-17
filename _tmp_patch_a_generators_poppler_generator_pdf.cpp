diff --git a/generators/poppler/generator_pdf.cpp b/generators/poppler/generator_pdf.cpp
index f5c02bb..b19cb9a 100644
--- a/generators/poppler/generator_pdf.cpp
+++ b/generators/poppler/generator_pdf.cpp
@@ -1302,7 +1302,7 @@ QImage PDFGenerator::image(Okular::PixmapRequest *request)
     qreal fakeDpiY = request->height() / pageHeight * dpi().height();
 
     // generate links rects only the first time
-    bool genObjectRects = !rectsGenerated.at(page->number());
+    bool genObjectRects = !rectsGenerated.at(page->sourceNumber());
 
     // 0. LOCK [waits for the thread end]
     userMutex()->lock();
@@ -1314,7 +1314,7 @@ QImage PDFGenerator::image(Okular::PixmapRequest *request)
     // 1. Set OutputDev parameters and Generate contents
     // note: thread safety is set on 'false' for the GUI (this) thread
-    std::unique_ptr<Poppler::Page> p = pdfdoc->page(page->number());
+    std::unique_ptr<Poppler::Page> p = pdfdoc->page(page->sourceNumber());
 
     // 2. Take data from outputdev and attach it to the Page
     QImage img;
@@ -1348,7 +1348,7 @@ QImage PDFGenerator::image(Okular::PixmapRequest *request)
         // and as we are not doing anything with Image type rects i did not port it, have a look at
         // dead gp_outputdev.cpp on image extraction
         page->setObjectRects(generateLinks(p->links()));
-        rectsGenerated[request->page()->number()] = true;
+        rectsGenerated[request->page()->sourceNumber()] = true;
 
         resolveMediaLinkReferences(page);
     }
@@ -1452,7 +1452,7 @@ Okular::TextPage *PDFGenerator::textPage(Okular::TextRequest *request)
         userMutex()->unlock();
         return nullptr;
     }
-    std::unique_ptr<Poppler::Page> pp = pdfdoc->page(page->number());
+    std::unique_ptr<Poppler::Page> pp = pdfdoc->page(page->sourceNumber());
     if (pp) {
         TextExtractionPayload payload(request);
         textList = pp->textList(Poppler::Page::Rotate0, shouldAbortTextExtractionCallback, QVariant::fromValue(&payload));
