# Charta developer architecture

This document describes how the Charta source tree is put together. It is aimed at contributors
who need to change behaviour without breaking the contracts that the user-facing
[README](../README.md) relies on.

Charta is a fork of KDE Okular. Most of the directory layout, the document core, the generator
plugin model, and large parts of the UI come from Okular. Sections below call out which pieces
are inherited, which are Charta-specific, and which constraints must be preserved.

## 1. Repository map

| Path | Contents |
| --- | --- |
| `CMakeLists.txt` | Top-level project, dependency discovery, QPDF fetch, UI selection, version |
| `core/` | `Okular::Document`, pages, annotations, undo commands, generators interface, observers |
| `generators/` | One shared-library plugin per backend: Poppler/PDF, DjVu, DVI, EPUB, images, and others |
| `part/` | `okularpart` UI part: page view, annotation tools, sidebar, Document Builder, dialogs |
| `shell/` | `charta` executable: `QMainWindow`, tabs, menus, welcome screen, command line |
| `autotests/` | Unit and integration tests linked against the core and part |
| `conf/` | KConfigXT schemas (`okular.kcfg`) and widget declarations |
| `data/` | Annotations tool definitions, icons, UI resources |
| `patches/` | Charta-specific downstream patches, including the Poppler ink appearance patch |
| `.flatpak-patches/` | Flatpak-only patches (mimetype registration) |
| `charta_logo_package/` | Logo masters and exported application icons |
| `LICENSES/` | Reusable license texts referenced by SPDX headers |

The build produces:

- `charta` — the desktop executable.
- `okularpart.so` — the UI part, loaded by the shell and installed into the KDE parts plugin
  directory.
- `okularGenerator_*.so` — document backends installed into the `okular_generators` plugin
  namespace.
- `Okular6Core` — the shared core library used by the part, the generators, and the tests.

## 2. Layering and process model

```text
charta (shell)
  └── okularpart.so (part)
        ├── core (Okular::Document, Page, Annotation, Generator)
        ├── UI (PageView, sidebar, annotation toolbar, Document Builder)
        └── generator plugins (loaded at runtime)
              ├── poppler (+ QPDF for page writing)
              ├── djvu / dvi / epub / comicbook / images / ...
              └── ...
```

Everything runs in one GUI process. Generators are Qt plugins discovered by `KPluginLoader` in
the `okular_generators` namespace. The part and generators communicate through
`Okular::Generator`; the core never links a backend directly.

The shell owns tabs and top-level chrome. The part owns the document, the page view, the
sidebar, and the Document Builder stack page. This is why the builder is a page inside the part
rather than a separate window: it needs the part's document, configuration, and chrome.

## 3. Document core

`core/document.cpp` is the inherited Okular core. The important pieces for Charta are:

- `DocumentPrivate::m_pagesVector` — the authoritative page order.
- `Document::m_undoStack` — one stack for annotation and page-structure commands.
- `Document::canModifyPages()` — returns true only when the active generator name contains
  `poppler` (case-insensitive). Page editing is deliberately PDF-only.
- `DocumentPrivate::beginPageStructureChange()` / `endPageStructureChange()` — bookkeeping
  around a page-order change.
- `Document::saveChanges()` — asks the generator to write the current state, then swaps the
  generator's backing file and reopens.

### Page-structure changes

Insertion, removal, and reordering all end in `DocumentPrivate::applyPageOrder()`:

1. Build `oldToNew` from the old and new `Page *` lists.
2. Call `beginPageStructureChange(oldToNew)`, which:
   - cancels pending pixmap requests,
   - resets active searches (they store page indices),
   - removes bookmarks from the old indices and records their new indices.
3. Install the new order, re-parent inserted pages, and renumber pages.
4. Call `endPageStructureChange()`, which re-adds bookmarks at their new indices and notifies
   observers with `PagesInserted`, `PagesRemoved`, or `PagesMoved`.
5. Delete cached pixmaps for pages that left the visible document; the `Page *` objects stay
   alive so undo can restore them.

The current viewport follows the page it was on; searches are dropped rather than remapped.

`PageStructureCommand` in `core/documentcommands_p.h` wraps a before/after page list. Undo and
redo swap the lists, so page objects (and their annotations) survive a round trip. When the
undo stack is refreshed after a backing-file swap, `refreshInternalPageReferences()` rewrites
page pointers in older commands.

### Page-structure save

`Document::saveChanges()` eventually calls the generator's `applyPageLayout()` (see
`generators/poppler/generator_pdf.cpp`). There are two implementations:

- **QPDF branch (`HAVE_QPDF`)** — opens the file with QPDF, pushes inherited attributes to the
  page level, builds the wanted page list, copies pages with `QPDFPageDocumentHelper`, remaps
  outline destinations that pointed at deleted pages, and writes `<document>.pages.tmp` next to
  the original. The generator then removes the original and renames the temporary file into
  place.
- **Fallback branch (no QPDF)** — splits the PDF with `pdfseparate` and rejoins it with
  `pdfunite` inside a `QTemporaryDir`, then handles inserted blank pages, removes the original,
  and copies the merged result into place.

Both branches remove the original before installing the replacement. **Neither replacement is
atomic**; the original is briefly absent. This is a known limitation, not a guarantee. Do not
describe page-structure saving as atomic.

Annotations on inserted blank pages are handled by a staged conversion: the blank page is
written to a temporary PDF, the annotations are applied, and the result is merged into the page
list.

### What page remapping does *not* fix

QPDF remaps outline destinations for deleted pages. It does not rewrite arbitrary links or
named destinations inside page content, so a link can still point at a page number that no
longer exists. This is documented as a limitation in the user README and is the subject of the
planned page-removal audit in the local planning notes.

## 4. Annotation pipeline

### Tool definitions and UI

- `data/tools.xml` declares the built-in annotation tools: kind, label, icon, default colour,
  width, opacity, and shape-specific defaults.
- `part/annotationtools.{h,cpp}` maps tool definitions to `Okular::Annotation` objects and
  implements the drawing engines.
- `part/annotationactionhandler.{h,cpp}` builds the annotation toolbar, assigns shortcuts,
  tracks the current tool and colour, and exposes actions to the part.
- `part/pageviewannotator.{h,cpp}` routes mouse and tablet events to the active engine, manages
  the temporary tool state, and commits completed annotations.
- `part/pageviewmouseannotation.{h,cpp}` handles selection, moving, resizing, and the selection
  action panel.

### Ink

`SmoothPathEngine` samples tablet events and builds a `SmoothPath`:

- Pressure is decoded in `AnnotatorEngine::decodeEvent()` as
  `qBound(0.0, pressure / 1.60 + 0.375, 1.0)`, matching Drawy's response curve.
- `SmoothPath` uses a seven-point smoothing window (`smoothingWindowSize = 7`).
- Segment widths are derived from pressure and clamped to `[0.5, 12.0]` points.
- The engine repaints only the dirty rectangle enclosing the live stroke.

When the stroke is committed, `PageViewAnnotator` records the annotation colour in
`AnnotationColorHistory` (the eight most recent colours, persisted under
`[Reviews] RecentAnnotationColors`).

### Pressure and the appearance stream

Charta's ink annotation model has two different representations:

- **Editable geometry** — the ink paths (`QList<QList<NormalizedPoint>>`) and the uniform
  annotation width. This is what undo, selection, and the annotation properties dialog operate
  on.
- **Session appearance** — a set of per-point pressures (`m_sessionPressures`), a stroke width,
  and an optional custom appearance stream. `InkAnnotation::setSessionPressures()` stores this
  for the current session only. It is not written into the annotation XML.

When saving a PDF, the Poppler generator calls `InkAnnotation::appearanceStream()`. On the
required patched fork, `generators/poppler/annots.cpp` attaches that stream as the annotation's
`/AP` appearance. The block is guarded by `POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE`; stock Poppler
does not define the macro and, more importantly, lacks the annotation-order APIs used
unconditionally by the same file, so stock Poppler does not compile the current backend.

Consequences:

- During a session, moving or erasing a stroke can use the pressure-shaped outline.
- Saving does not flatten the in-session representation; `swapBackingFile()` preserves the
  session pressures.
- After reopening, Charta has no per-point pressure in the model. The saved PDF still contains
  the appearance stream written by the fork, so other viewers render the pressure-shaped stroke.

### Selection, move, resize, and rotation

Selection is handled by `MouseAnnotation`. A click or lasso produces a list of annotations and
a bounding box; move and resize operations are batched into one undo command per drag.

The working tree contains an in-progress oriented-frame model in `core/annotations.h`
(`AnnotationFrame`, rotated outlines, frame-aware hit testing) and a new
`autotests/annotationrotationtest.cpp`. The frame test is present, but no user-facing rotation
gesture or toolbar action was found during this inspection. Treat annotation rotation as
experimental model groundwork, not a shipped feature.

### Eraser and protected annotations

The eraser is an engine of its own. It removes ink annotations from the page, skipping
annotations that are protected, pending, or not removable by the document's permissions.
Right-button temporary erasing is implemented in `PageViewAnnotator`: pressing the right button
while a `SmoothPathEngine` is active swaps in the eraser, and releasing restores the previous
engine and the locked item.

## 5. Document Builder

The builder is a `QStackedWidget` page in `okularpart` (page index 1 of `Part::m_mainStack`),
not a dialog. The shell shows or hides the sidebar and focus around it through
`requestDocumentBuilderVisibilityChanged(bool)`.

### Files

| File | Responsibility |
| --- | --- |
| `part/documentbuildermodel.{h,cpp}` | Ordered entries, the trailing add tile, reorder operations, MIME data for tests |
| `part/documentbuildersource.{h,cpp}` | One per canonical PDF path: page metadata, password, lazy headless render document, thumbnail cache |
| `part/documentbuilderexport.{h,cpp}` | QPDF page copy, image-page writing, conflict checks, atomic output |
| `part/documentbuilder.{h,cpp}` | Widget, view, delegate, toolbar, drag, context menu, Configure popup |
| `part/documentbuilderconfig.h` | `[DocumentBuilder]` settings: columns, thumbnail size, file names |
| `part/pagerangeinputdialog.{h,cpp}` | Page-range parser kept as public API and tested by the model tests |
| `part/imagepagesettingsdialog.{h,cpp}` | Page size, orientation, margin, background, fit for image pages |

### Model invariants

- The model is a flat list of `BuilderEntry` objects, one per output page.
- `rowCount() == entries + 1`; the last row is the permanent "Add pages" tile.
- Each entry references either a PDF page (`canonicalPath`, source page number) or an image.
- `moveRows` / `moveEntriesBefore` / `moveEntriesByStep` are the only reorder entry points.
  Always reselect by entry id after a model reset, because `beginResetModel` invalidates
  indexes.
- Do not use `moveRows(firstRow, count, ...)` for a scattered multi-selection: it sweeps the
  unselected pages between the first and last selected row.

### Source ownership and passwords

`BuilderSource` is created for a canonical PDF path and lives as long as any entry references
it. When the last entry is removed, the source is deleted after `removeObserver`.

A source owns:

- page count and page sizes read with QPDF at import time,
- the file's password (never copied into an entry and never logged),
- a lazy `Okular::Document` used only to render thumbnails.

Image entries never create a `BuilderSource`; their thumbnails are page previews held in an LRU
cache.

### Import

`DocumentBuilderWidget::addFiles()` is the single dispatcher. For each local file:

- `.pdf` or a `%PDF-` header in the first 1024 bytes → `addPdf()`.
- Otherwise → `addImage()`.

Encrypted PDFs prompt for a password up to three times; the password is stored on the
`BuilderSource` and passed to QPDF for the duration of an export. Remote URLs are rejected with
a message.

### Thumbnails

- The grid requests visible cards at `THUMBNAILS_PRIO` and up to 24 background cards per pass at
  `THUMBNAILS_PRELOAD_PRIO`, chaining until all pages have been requested.
- Per-source requests are batched with `Okular::Document::NoOption`. Never use the one-argument
  `requestPixmaps(list)` overload: it passes `RemoveAllPrevious` and cancels every other pending
  request of the same observer.
- A cancelled request never notifies. `BuilderSource::isThumbnailPending()` times out after
  5 seconds, and the view re-checks visible rows once a second while a request is pending.
  Do not prune pending entries on `notifyContentsCleared(Pixmap)`; that signal also fires for
  a single pre-empted request.

### Grid interaction

Card reordering is a hand-rolled mouse drag, not Qt drag and drop:

- `mousePressEvent` / `mouseMoveEvent` / `mouseReleaseEvent` drive
  `beginEntryDrag()` / `updateEntryDrag()` / `finishEntryDrag()`.
- A ghost card and insertion bar are painted; the view auto-scrolls near the edges.
- Escape or a release outside the viewport cancels the drag.
- The result is applied with `moveRows` for a contiguous block and `moveEntriesBefore()` for a
  scattered selection, then the entries are reselected by id.
- `setDragDropMode(DropOnly)` keeps Qt responsible only for external URL drops.

This avoids Qt's icon-mode drop filter, which accepts a drop only over a drop-enabled index,
the dragged items, or empty space, and which did not deliver drag events back to a view that
started the `QDrag` itself.

### Reorder commands

Four equivalent entry points all end in the model:

- mouse drag,
- `Alt+Up` / `Alt+Down`,
- the context menu's Move Up / Move Down / Move to Start / Move to End,
- the model API used by tests.

`moveEntriesByStep(ids, ±1)` moves each selected entry past its unselected neighbour.
`moveEntriesBefore(ids, row)` gathers the selection for start/end moves.

### Export

`DocumentBuilderExport::exportPdf(pages, outputPath, &error, allowSourceOverwrite)`:

1. Validate that the output directory exists and is writable, and that every source exists.
2. Refuse an output that resolves to a source (canonical path plus device/inode on POSIX).
   `allowSourceOverwrite` is used only by the builder's explicit Save action.
3. Create a unique `QTemporaryFile` in the destination directory, named
   `.<basename>.charta-XXXXXX.pdf`, with `setAutoRemove(false)`.
4. Copy PDF pages with QPDF (`addPage(sourcePage, false)`) or write image pages to temporary
   PDFs and merge them.
5. Reopen the temporary file and verify its page count.
6. Rename it over the destination. On POSIX this is `::rename`, which is atomic on the same
   filesystem. Other platforms remove the destination first, which is not atomic.
7. On any failure, remove the temporary file and leave the destination untouched.

QPDF exceptions are caught around the whole write; the file is compiled with `-fexceptions`.
Export runs synchronously on the GUI thread under a non-cancelable busy state (wait cursor,
`QProgressDialog` with no cancel button, toolbar and grid locked by an RAII guard). Do not add
a Cancel button without first moving export to a worker thread.

### Save versus Export

- **Export** always asks for a destination and refuses to overwrite a source.
- **Save** is available only when the builder was seeded from a real local PDF (never an
  untitled document). It asks for confirmation, calls `exportPdf(..., true)`, emits
  `documentSaved(path)`, and the part resets the builder and reloads the file with
  `promptToSave = false`. The reset is mandatory: the builder's page numbers refer to the old
  file and must not be reused.

## 6. Create-a-PDF / untitled documents

`File → New` and the welcome screen create a temporary PDF with configured blank pages through
`Part::slotNewDocument()` / the blank-page writer. The document is untitled and is not added to
the recent-files list. `Save As` adopts the chosen path and turns it into a normal document.

Autosave must never fire for an untitled document: it would call the same `Ctrl+S` path and
could open a Save dialog by itself. `Part::canAutoSaveNow()` checks `m_untitledDocument` first.

## 7. Autosave

Settings live in `conf/okular.kcfg` under `[General]`:

- `AutoSaveEnabled` — default `true`.
- `AutoSaveIntervalSeconds` — default `60`, range 10–3600.

`Part::canAutoSaveNow()` is the complete gate. It refuses when:

- autosave is already running, a save is in progress, the document is not read-write or not
  modified, the document is not open, or the document is untitled;
- the part is in print-preview embed mode, the document was opened with a password, or the URL
  is not local;
- the document cannot save changes, or an archive cannot swap its backing file;
- the file no longer exists, is not writable, or its modification time differs from
  `m_fileLastModified` (changed on disk);
- the format would lose annotations or forms (`checkNativeSaveDataLoss`).

It also waits while a mouse button is held, while a modal dialog is open, and for two seconds
after the last input. The timer retries shortly afterwards.

The save itself goes through the normal `saveFile()` path, including the generator's
backing-file swap. Autosave has no dedicated automated test.

## 8. Settings and configuration

- `conf/okular.kcfg` is the KConfigXT schema; generated classes are `Okular::Settings` and
  `Okular::SettingsCore`.
- Annotation settings include the toolbar variant (full/quick), compact mode, recent colours,
  width presets, and opacity presets.
- Document Builder settings are a header-only group in `part/documentbuilderconfig.h`:
  `Columns` (auto or 2–6), `ThumbnailSize` (small/medium/large), `ShowFileNames`.
- The shell stores window, tab, recent-file, and sidebar state through its own configuration
  and KRecentFilesAction.

When adding a setting, add it to the KConfigXT schema when it is shared with the core or part,
or to the appropriate header-only group when it is local to a widget. Do not hand-edit generated
config classes.

## 9. QPDF integration

QPDF is optional at configure time:

1. `find_package(qpdf CONFIG QUIET)` for `qpdf::libqpdf`.
2. `pkg_check_modules(QPDF ... libqpdf)` for a pkg-config target.
3. If neither exists, `FetchContent` downloads QPDF v11.9.1 and builds it statically.

Charta forces QPDF's object library to `POSITION_INDEPENDENT_CODE ON` because the generator is a
shared plugin. When QPDF is the fetched static build, the part links `libqpdf.a` with
`LINKER:--exclude-libs,libqpdf.a` on ELF so it does not publish a second public copy of QPDF
next to the generator's. The only expected exported QPDF symbols from `okularpart.so` are the
two weak `QPDFWriter` destructors; anything else is a regression.

`HAVE_QPDF` guards the QPDF page-copy code and the builder exporter. Without QPDF, the builder
cannot export and page editing falls back to `pdfseparate`/`pdfunite`.

## 10. Poppler integration and the required fork

`find_package(Poppler 24.08.0 COMPONENTS Qt6)` locates Poppler for the PDF generator in
`generators/poppler`. The 24.08.0 value is the inherited CMake minimum; the fork Charta uses is
Poppler 26.08.0.

The current tree requires a published Poppler 26.08.0 fork:

<https://github.com/Rupam3238/poppler-patch-for-charta-26.08.0>

It carries two Charta-specific changes:

- **Custom ink appearance** (`5799dc0`, “Baseline Poppler 26.08.0 with custom ink appearance
  support”): defines `POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE` and adds
  `InkAnnotation::setAppearanceStream(const QByteArray &, const QRectF &)` to the Qt6
  annotation API, with the corresponding `AnnotInk` method in the core library.
- **Annotation order** (`0fe4c9d`, “First pass to add reorder functionality”): adds
  `Page::annotationIndex()`, `Page::annotationCount()` and `Page::moveAnnotation()` to the Qt6
  API, plus the internal `Page::moveAnnot()` path that rewrites the page's `/Annots` array
  while preserving the existing annotation objects.

`generators/poppler/annots.cpp` checks `POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE` before attaching
the pressure-shaped stream. The annotation-order calls (`annotationIndex()`,
`annotationCount()`, `moveAnnotation()`) are unconditional, so **stock Poppler does not compile
the current PDF backend**. `patches/poppler-ink-appearance.patch` is the local patch for the
ink portion only; it is not the complete fork.

The fork's `COPYING` is GPL-2.0, and it is based on Poppler 26.08.0. Its git history records
the baseline import and the reorder commit but not the exact upstream commit hash. The
repository also contains a committed `build/` directory, so build it out of tree.

The maintainer's installed copy is under
`/home/workstation/tools/deps/poppler-26.08-staging` (machine-specific). The Flatpak manifest
builds upstream Poppler 26.08.0 and applies only the local ink patch; it has not been updated
to use the full fork and was not verified for this documentation.

## 11. Testing map

Tests are registered from `autotests/CMakeLists.txt` and the generator `CMakeLists.txt` files.
The count depends on optional backends and on whether QPDF is fetched. Important groups:

| Area | Tests |
| --- | --- |
| Document Builder | `documentbuildermodeltest`, `documentbuilderparttest` |
| Blank pages / untitled | `blankpageswritertest`, `untitleddocumenttest` |
| Annotation tools | `annotationtoolbartest`, `annotationcolorhistorytest`, `widthpresetstest`, `addremoveannotationtest`, `modifyannotationpropertiestest` |
| Annotation geometry | `annotationrotationtest` (working tree; oriented-frame model) |
| Shell and part | `mainshelltest`, `shelltest`, `parttest` |
| Generators | `generatorstest`, `imageScalingTest`, `comicbooktest`, `epubgeneratortest`, `kimgiotest` |
| Core documents | `documenttest`, `searchtest`, `formattest`, `keystroketest` |
| Signatures / forms | `signatureformtest`, `signunsignedfieldtest`, `editformstest` |

Not covered by a dedicated test at the time of writing:

- page insert/delete/reorder and the page-save path,
- autosave behaviour,
- the Document Builder exporter's conflict and atomic-replacement rules,
- the patched-Poppler appearance stream.

Run tests through CTest, not by executing binaries from the source tree:

```bash
cd build
xvfb-run --server-args="-screen 0 1024x768x24" ctest --output-on-failure
```

## 12. Engineering caveats

- **Page save is not atomic.** With QPDF the generator writes `<file>.pages.tmp` and then
  removes and renames; without QPDF it removes the original and copies a merged file from a
  temporary directory. A crash between removal and replacement can lose the destination.
- **Outline/link remapping is partial.** QPDF remaps outline destinations for removed pages;
  links and named destinations in page content are not audited.
- **Pressure is session-only.** The model does not serialise per-point pressure. The PDF
  appearance stream is the only persistent representation, and it is written by the required
  patched Poppler fork.
- **The appearance cache is keyed by a raw pointer.** This is one of the page-removal audit
  findings in the local planning notes. Do not assume it is stable across page-structure
  changes.
- **The builder export is synchronous.** Keep it on the GUI thread or move it completely; do
  not add a cancel button to a blocking call.
- **Plugin staleness.** The application loads generators and the part from the build tree during
  development. A stale installed plugin can mask a rebuild; check the loaded path before
  debugging a "no effect" change.
- **No CI.** Build and test results in the README are claims about specific runs, not a
  guarantee. The verification document records what was actually executed.

## 13. Where to make common changes

| Change | Start here |
| --- | --- |
| New annotation tool | `data/tools.xml`, `part/annotationtools.cpp`, `part/annotationactionhandler.cpp` |
| Toolbar shortcut or colour cycle | `part/annotationactionhandler.cpp` |
| Ink pressure or appearance | `part/annotationtools.cpp`, `core/annotations.cpp`, `generators/poppler/annots.cpp` |
| Page structure UI | `part/part.cpp`, `part/thumbnaillist.cpp`, `core/document.cpp` |
| Page save backend | `generators/poppler/generator_pdf.cpp` |
| Document Builder model | `part/documentbuildermodel.cpp` |
| Document Builder export | `part/documentbuilderexport.cpp` |
| Shell chrome or welcome screen | `shell/shell.cpp`, `shell/welcomescreen.cpp` |
| Settings | `conf/okular.kcfg`, `part/documentbuilderconfig.h` |
| Tests | `autotests/CMakeLists.txt` and the matching test source |
