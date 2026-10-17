# Charta — Page Structure Persistence: Fix Plan

Repo: /home/workstation/tools/project/Charta (fork of KDE Okular: C++20, Qt 6, KF6, CMake, Poppler-Qt6)
Branch: main (HEAD e2a4b67 "Undo Redo functionality and colour persistence")
Status: in-session add/delete/reorder/undo/redo and colour memory work; saving the page layout to the PDF is incomplete/lossy.

This plan sketches the fix for "how it handles things internally and saves stuff". It is based on actual repo inspection plus web research (QPDF library/CLI docs at qpdf.readthedocs.io; Poppler API docs at poppler.freedesktop.org/api/qt6). No build/test was possible on this machine (no Qt/KF6/Poppler); every item below needs verification on your machine.

================================================================
1. RESEARCH FINDINGS (how these things should be done)
================================================================

1.1 Poppler cannot write a modified page layout
------------------------------------------------
Poppler Qt6's `Poppler::Document` has no save/write method at all. The only save-related surface is
`additionalAction(SaveDocumentStart)` / `SaveDocumentFinish`, which Okular/Charta uses to drive its own
save. The actual PDF writing in the poppler generator goes through `Poppler::PDFConverter` (cpp backend),
which writes only the original pages of the loaded file. It has no API to insert, delete, or reorder pages.
Conclusion: page-structure changes MUST be written by Charta itself, outside Poppler, on top of the file
Poppler just wrote. That is what Patch 2 (SaveInterface::applyPageLayout) already does.

1.2 QPDF is the right tool for the rewrite
-------------------------------------------
QPDF 3.0+ provides `QPDFPageDocumentHelper::addPage/removePage`, `QPDF::pushInheritedAttributesToPage()`,
and `QPDFWriter` with preserve modes (`qpdf_s_preserve`, `qpdf_o_preserve`). QPDF docs describe these as
"convenience routines that handle two tricky issues: pushing inheritable resources from the /Pages tree
down to individual pages and manipulation of the /Pages tree itself". Patch 2's QPDF path follows exactly
this pattern, so the QPDF route stays the primary save path.

1.3 QPDF page-tree manipulation does NOT preserve document-level features
-------------------------------------------------------------------------
QPDF docs (cli.html, "Page Selection / Limitations", and the `--split-pages` description) state explicitly:
"outlines, threads, and other document-level features of the original PDF file are not preserved".
`addPage`/`removePage` restructure the /Pages tree but do not auto-remap:
- outline / outline item destinations (/Dest page references),
- named destinations (/Names /Dests),
- AcroForm field page references,
- article threads (/P page entries),
- per-page thumbnails (/Thumb).
So after Charta's QPDF rewrite, bookmarks and form/page links can point to wrong pages or vanish.
This is the root cause of "bookmarks and outline entries may break".

1.4 Blank pages have no backing in Poppler
------------------------------------------
A user-added blank page has `sourceNumber() == -1`; Poppler's document knows nothing about it.
Annotations created on a blank page live only in Charta's in-memory AnnotationModel tree. When Poppler
saves the file it cannot write them, and after `applyPageLayout` + `swapBackingFile` reload they are gone.
Workaround today: save right after adding/moving pages, then annotate. This is a real data-loss bug.

1.5 Undo across save
--------------------
Deleted pages belong to the old file (their content only exists there). `swapBackingFile` therefore clears
the undo stack when it contains a `PageStructureCommand`, so undo after a save would resurrect pages that no
longer exist in the file. Keeping undo across save would require retaining the pre-save PDF as a backing
store for deleted pages — possible but complex; explicitly deferred in this plan (see Phase D), with clear
user-visible behavior in the meantime.

1.6 poppler-utils fallback (pdfseparate/pdfunite)
-------------------------------------------------
Used only when QPDF is missing. It works but drops bookmarks/forms/threads (same class of problem as 1.3,
worse because pages are round-tripped through Qt rendering). It also requires poppler-utils installed and
spawns external processes (already made non-blocking via `runTool` with 120s timeout). Keep as last-resort
fallback, with a clear install message when tools are missing.

================================================================
2. PROBLEMS TO FIX
================================================================

P1. Save loses document-level data (bookmarks/outline, named destinations, form page refs, thumbnails)
   after the QPDF page-tree rewrite; worse with the pdfunite fallback.
P2. Annotations on newly added blank pages are dropped on save (no Poppler page to attach them to).
P3. Undo history is cleared on save (breaks "undo until Ctrl+W" for page-structure changes).
P4. Save-failure path: if `applyPageLayout` fails, the document still reloads; user sees
   "Could not open file ...pdf" / "something went wrong". Needs a clean failure path (keep the old file,
   report precisely what failed).
P5. External-tool fallback is fragile (missing poppler-utils, timeouts, hangs).

================================================================
3. PLAN — PHASES
================================================================

Phase A — Verify and test the existing implementation (do first)
----------------------------------------------------------------
Goal: establish a trusted baseline on a machine with Qt/KF6/Poppler.
- Build: `cmake -S . -B build -DOKULAR_UI=desktop -DBUILD_TESTING=ON` (+ `-DCMAKE_BUILD_TYPE=Debug`),
  then `cmake --build build --parallel $(nproc)`.
- Run the full test suite: `cd build && ctest --output-on-failure`; add autotests covering
  insert/remove/move + undo/redo + save + reload if missing.
- Run the user's test checklist (1–7 from the work log) on a copy of a real PDF:
  1. Add black page → dialog defaults to black next time.
  2. Reorder/delete/add → undo/redo each one.
  3. Delete page with annotations → undo → annotations come back.
  4. Reorder → save → reopen → order and annotations right.
  5. Save twice in a row.
  6. Bookmarks, thumbnails, annotation sidebar after save.
  7. Ctrl+W → reopen → undo history gone.
- Record which checklist items pass/fail; failures drive Phases B–D.

Phase B — Robust QPDF path: preserve document-level data (fixes P1)
-------------------------------------------------------------------
In `generators/poppler/generator_pdf.cpp::PDFGenerator::applyPageLayout` (QPDF branch), after
rebuilding the page tree with `helper.removePage`/`helper.addPage`:
1. Remap outlines: walk the /Outlines tree; every /Dest that references an old page object must be
   re-pointed at the corresponding new page object (build an old-page-object → new-page-object map from
   `wanted`/`original`). Do the same for named destinations (/Names /Dests).
2. Remap AcroForm field page references if the generator reads/writes forms.
3. Fix per-page thumbnail references (/Thumb) pointing at removed pages.
4. Warn (errorText) if any destination could not be remapped, rather than silently producing broken
   bookmarks.
5. Linearization: QPDFWriter output is not linearized. Offer `--linearize` via the qpdf CLI if the
   original was linearized and qpdf binary is available, or at least document that linearization is lost.
6. Keep the atomic write-to-`.pages.tmp`-then-rename pattern (already in place).
Why this order: QPDF first (in-process, no external tools); poppler-utils only when QPDF is absent.

Phase C — Annotations on blank pages (fixes P2)
------------------------------------------------
Root cause: blank pages (`sourceNumber == -1`) are unknown to Poppler, so Poppler cannot save annotations
on them and Charta's AnnotationModel has no page to key them on after reload.
Options (in increasing effort):
  C1 (document + guard): Before save, if any blank page carries annotations, fail the save with a clear
  message ("annotations on newly added blank pages cannot be saved yet; save first, then annotate") and
  keep `m_pageStructureModified` so the user can retry after reordering is final. No data loss, no silent drops.
  C2 (materialize + journal): When saving, write blank pages into the PDF first (QPDF already creates them
  in `applyPageLayout`); record blank-page annotations in a small sidecar/journal keyed by blank-page index
  and re-attach them to the matching page after `swapBackingFile` reload. This is the real fix but touches
  AnnotationModel, Document, and the reload path.
Recommendation: ship C1 now (no silent data loss), design C2 as a follow-up with its own tests.

Phase D — Undo across save for page structure (fixes P3)
---------------------------------------------------------
Design: keep a copy of the pre-save file (e.g. `<file>.charta-presave` next to the document, or in the
same temp location) while the undo stack still holds `PageStructureCommand`s. On undo of a page-structure
command after a save, restore pages from that pre-save backing file instead of declaring them lost.
- Delete the pre-save copy on `Ctrl+W`/`closeDocument()` or when the stack is cleared otherwise.
- Until implemented: keep current behavior (stack cleared on save) and show it explicitly in the UI
  ("undo history for page changes is cleared when saving") so users are not surprised.
Scope decision: implement only if Phase A–B testing shows users hit this often; otherwise document as
a known limitation (already listed in the work log).

Phase E — poppler-utils fallback hardening (fixes P5)
-----------------------------------------------------
- Keep `runTool` (non-blocking, `/dev/null` stdin, merged output, 120s timeout, kill on timeout).
- Add an up-front executable check for `pdfseparate`/`pdfunite` with a single clear error message naming
  `poppler-utils` (and pointing to QPDF as the better route) instead of failing mid-save.
- Log `PDFGenerator::applyPageLayout: <cmd> failed: <output>` on failure (already done).

Phase F — Close the loop
------------------------
- Update the Known limits section in this plan and any user-facing docs after each phase.
- Add autotests for: QPDF applyPageLayout reorder (unit-level if generator can be tested standalone), save
  failure handling, and (if C2 is implemented) blank-page annotation round-trip.

================================================================
4. FILES TO TOUCH
================================================================
Already patched (patches 1–6, committed as e2a4b67 plus working-tree changes):
- core/document.cpp          : saveChanges, swapBackingFile, insertPages/removePages/movePages, applyPageOrder
- core/document_p.h          : m_pageStructureModified
- core/documentcommands_p.h / core/documentcommands.cpp : PageStructureCommand
- interfaces/saveinterface.h : PageLayoutEntry + applyPageLayout (default fails)
- generators/poppler/generator_pdf.cpp / .h             : applyPageLayout (QPDF + poppler-utils paths)
- generators/poppler/CMakeLists.txt, CMakeLists.txt, config-okular.h.cmake : QPDF detection/linking
- part/newpagedialog.cpp     : colour memory (okularrc "New Page Dialog"/BackgroundColor)
- part/annotationmodel.cpp   : tree rebuild on PagesInserted/Removed/Moved, UrlChanged refresh

To be added/changed by this plan:
- generators/poppler/generator_pdf.cpp : outline/named-destination/form/thumbnail remapping in QPDF path
- core/document.cpp / core/document_p.h : save-failure rollback, optional pre-save file retention (Phase D)
- part/annotationmodel.cpp               : track blank-page annotations (Phase C1/C2)
- autotests/                             : new tests per Phase A/F

================================================================
5. DEFINITION OF DONE
================================================================
- Build succeeds with and without QPDF; CMake prints a clear warning when QPDF is missing.
- Save with reorder/delete/add produces a valid PDF that reopens with the same page order.
- Bookmarks/outline survive a QPDF-based save (or a clear warning is shown when they cannot be remapped).
- No silent data loss: annotations on blank pages either save (Phase C) or the save fails with a clear
  message and the document is left untouched.
- Save failure leaves the document open on the original file; user gets a precise error.
- Test checklist 1–7 passes on a real PDF copy; autotests added and green.
- Undo behavior after save is documented (cleared now, retained only after Phase D if implemented).

================================================================
6. LOCATION
================================================================
This plan lives at /home/workstation/tools/project/Charta/PAGE_STRUCTURE_SAVE_PLAN.md
