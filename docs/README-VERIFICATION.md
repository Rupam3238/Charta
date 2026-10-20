# README verification dossier

This file records what was checked while writing the user-facing [README](../README.md), where the
evidence lives, and what remains unresolved. It is deliberately more technical and more cautious
than the README.

It is **not** a release audit. It describes one inspection of one working tree.

## 1. Snapshot inspected

| Item | Value |
| --- | --- |
| Repository | `https://github.com/Rupam3238/Charta.git` (`origin`, verified with `git remote -v`) |
| HEAD | `66b15b4` — “DocumentBuilder Done.”, 2026-10-19 |
| Working tree (before this documentation pass) | Modified: `.gitignore`, `autotests/CMakeLists.txt`, `core/annotations.cpp`, `core/annotations.h`, `core/annotations_p.h`, `core/area.cpp`, `part/pageviewannotator.cpp`, `part/pageviewmouseannotation.cpp`, `part/widthpresetutils.h`; untracked: `autotests/annotationrotationtest.cpp` |
| Uncommitted work | Oriented annotation-frame model (`AnnotationFrame`), frame-aware geometry/hit testing, a new rotation test, and resize-handle/selection changes. No rotation gesture was found in the part. |
| Platform | Linux, x86-64 |
| Compiler | GCC 15.2.0 |
| CMake | 4.2.3 |
| Qt | 6.10.2 (project minimum 6.6.0) |
| KDE Frameworks | 6.24.0 (project requests 5.240.0; all KF6 releases satisfy that floor) |
| Poppler (installed) | 26.08.0 from `/home/workstation/tools/deps/poppler-26.08-staging`, carrying `POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE` and the annotation-order APIs |
| Poppler fork inspected | <https://github.com/Rupam3238/poppler-patch-for-charta-26.08.0>, `master` at `0fe4c9d`; baseline `5799dc0`; GPL-2.0; 1,523 tracked files under `build/` |
| QPDF | v11.9.1 source reused from the existing build's `_deps` for the configure check |
| Project version | `26.11.70` (`CMakeLists.txt`) |
| Application ID | `org.charta.reader` (`.flatpak-manifest.json`, desktop/metainfo files) |
| Executable | `charta` (`.flatpak-manifest.json`, shell target) |

The working tree is not clean. Statements about “the current tree” below include those
uncommitted changes where relevant; statements about HEAD mean the committed snapshot without
them.

## 2. Commands actually run

### Read-only inspection

- `git remote -v`, `git rev-parse --short HEAD`, `git log -1`, `git status --short`,
  `git log --follow -- patches/poppler-ink-appearance.patch`,
  `git log -- README.md`, `git show --stat 8578911`.
- `git ls-remote` and a shallow clone of
  `https://github.com/Rupam3238/poppler-patch-for-charta-26.08.0.git`, followed by
  `git log --oneline`, `git show --stat`, and `grep` for the Poppler version and CMake options.
  The fork's `CMakeLists.txt` declares `POPPLER_VERSION` 26.08.0 and the options
  `ENABLE_QT6`, `ENABLE_QT5`, and `BUILD_QT6_TESTS` used in the README instructions.
- `find` / `ls` over the repository, `/home/workstation/tools/deps`, and the build tree.
- `grep` / `sed` / `cat` over the CMake files, `part/`, `core/`, `shell/`, `generators/poppler/`,
  `autotests/`, `conf/okular.kcfg`, `.flatpak-manifest.json`, and the Poppler staging headers.
- `nm` was not rerun in this pass. The QPDF symbol-hiding statement in the developer
  documentation is taken from the existing project notes and the CMake `--exclude-libs` rule,
  not from a fresh symbol check.

### Test registration

Existing development build:

```bash
cd /home/workstation/tools/project/Charta/build
ctest -N
```

Result: **44 registered tests** (37 Charta/ECM + 7 from the fetched QPDF build). No test was
executed.

### Fresh configure of the working tree

A separate build directory was configured to avoid relying on the existing cache:

```bash
cd /home/workstation/tools/project/Charta
PKG_CONFIG_PATH=/home/workstation/tools/deps/poppler-26.08-staging/lib/pkgconfig:$PKG_CONFIG_PATH \
cmake -S . -B /tmp/opencode/charta-config-check \
  -DOKULAR_UI=desktop \
  -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/home/workstation/tools/deps/poppler-26.08-staging \
  -DFETCHCONTENT_SOURCE_DIR_QPDF=/home/workstation/tools/project/Charta/build/_deps/qpdf-src
```

Result: **configure succeeded**. `ctest -N` in that directory listed **45 tests**: 38
Charta/ECM tests plus 7 QPDF tests. The difference from the existing build is the uncommitted
`annotationrotationtest`. The configure reused a local QPDF source directory, so it did **not**
verify a fresh network fetch.

The fresh configure found the patched Poppler at
`/home/workstation/tools/deps/poppler-26.08-staging` (cache variables `Poppler_Qt6_INCLUDE_DIR`
and `Poppler_Qt6_LIBRARY`). The staging header defines
`POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE 1` and declares `Page::annotationIndex()`,
`Page::annotationCount()`, and `Page::moveAnnotation()`. The system Poppler header at
`/usr/include/poppler/qt6/poppler-qt6.h` does not declare those three methods.

### What was not run

- No full build (`cmake --build`) was run for this documentation pass. The existing `build/`
  tree contains binaries from earlier work, but they were not rebuilt here.
- The published Poppler fork was cloned and inspected, but not built or installed here.
- No test was executed (`ctest` without `-N`).
- No install (`cmake --install`) was run.
- No Flatpak build was run.
- The application was not launched for manual UI verification.
- No Windows, macOS, Android, or mobile build was attempted.
- No git write operation was performed.

## 3. Test inventory and why the count varies

The suite is registered from:

- `autotests/CMakeLists.txt` (33 tests at HEAD),
- `generators/comicbook/CMakeLists.txt` (`comicbooktest`),
- `generators/epub/CMakeLists.txt` (`epubgeneratortest`),
- `generators/poppler/CMakeLists.txt` (`imageScalingTest`),
- `generators/kimgio/CMakeLists.txt` (`kimgiotest`, conditional; not registered in this
  configuration),
- ECM's `appstreamtest` (from `KDECMakeSettings`),
- QPDF's own tests when QPDF is fetched with `BUILD_TESTING=ON`.

Observed counts:

| Tree | Charta/ECM tests | QPDF tests | Total |
| --- | --- | --- | --- |
| Existing `build/` (HEAD-like, no rotation test) | 37 | 7 | 44 |
| Fresh configure of working tree | 38 | 7 | 45 |

Consequences for the README:

- Do not publish a single fixed test count. It changes with optional backends and with whether
  QPDF is fetched.
- The README lists representative tests rather than claiming the suite is complete or green.
- The README states that no CI exists and that a test run is not guaranteed by the document.

Name checks: the fresh configure listed `appstreamtest`, `check-assert`, `compare-for-test`,
`qpdf`, `libtests`, `examples`, `zlib-flate`, `fuzz`, `imageScalingTest`, `comicbooktest`,
`epubgeneratortest`, and the 33 `autotests` entries plus `annotationrotationtest`.
`kimgiotest` was not registered.

## 4. Claim-to-evidence map

### Identity and build

| README claim | Evidence |
| --- | --- |
| Based on KDE Okular, not an official KDE release | Existing README; inherited `core/`, `generators/`, `part/`, KDE copyright headers |
| Version `26.11.70` | `CMakeLists.txt:4-7` |
| Executable `charta` | `shell/CMakeLists.txt`; `.flatpak-manifest.json` `"command": "charta"` |
| Application ID `org.charta.reader` | `.flatpak-manifest.json` `"id"`; `shell/org.charta.reader.desktop`, `shell/org.charta.reader.metainfo.xml` |
| CMake 3.22+, C++20 | `CMakeLists.txt:1`, `CMakeLists.txt:94-95` |
| Qt 6.6+ | `CMakeLists.txt:11`, `CMakeLists.txt:103` |
| KF6 floor 5.240.0 | `CMakeLists.txt:12`, `CMakeLists.txt:105-119`; installed KF6 is 6.24.0 |
| Poppler fork 26.08.0 required | `CMakeLists.txt:318` sets the 24.08 minimum; `generators/poppler/annots.cpp` calls `annotationIndex()`, `annotationCount()`, and `moveAnnotation()` unconditionally. The fork at <https://github.com/Rupam3238/poppler-patch-for-charta-26.08.0> adds them; the system Poppler 26.01 header lacks them |
| Fork is GPL-2.0 and based on Poppler 26.08.0 | Fork `COPYING`; `CMakeLists.txt:43-49` (`26.08.0`) |
| Fork commit history | `5799dc0` baseline with custom ink appearance; `0fe4c9d` annotation reorder APIs |
| Fork contains a committed `build/` directory | `git ls-files build` in the clone → 1,523 files; use an out-of-tree build directory |
| QPDF fetched as v11.9.1 if absent | `CMakeLists.txt:218-272` |
| QPDF built static/PIC, symbol-hiding on ELF | `CMakeLists.txt:232-293`; project notes. Not rechecked with `nm` in this pass |
| `OKULAR_UI` values | `CMakeLists.txt:14`, `CMakeLists.txt:67-80` |
| `USE_DBUS`, `FORCE_NOT_REQUIRED_DEPENDENCIES`, `BUILD_COVERAGE` options | `CMakeLists.txt:16`, `CMakeLists.txt:143-147`, `CMakeLists.txt:389-391` |
| Flatpak runtime `org.kde.Platform` 6.11 | `.flatpak-manifest.json:4-5` |
| Flatpak builds upstream Poppler 26.08.0 and applies only the local ink patch | `.flatpak-manifest.json:283-312`; it does not use the full fork |
| Flatpak applies the mimetype patch | `.flatpak-manifest.json:421`, `.flatpak-patches/okular-mimetypes.patch` |

### Annotation behaviour

| README claim | Evidence |
| --- | --- |
| Pressure mapping | `part/annotationtools.cpp:84-87` (`pressure / 1.60 + 0.375`, bounded) |
| Seven-point smoothing | `part/annotationtools.cpp:457` (`smoothingWindowSize = 7`) |
| Ink width clamp `[0.5, 12.0]` | `part/annotationtools.cpp:497` |
| Dirty-rectangle live repaint | `part/pageviewannotator.cpp:1996-2205`, `2422-2514`; no tile rendering was found |
| Right-button temporary eraser | `part/pageviewannotator.cpp:2070-2175` |
| Hold threshold 180 ms | `part/annotationactionhandler.cpp:78` |
| Recent colours max 8 | `part/annotationcolorhistory.cpp`; key `[Reviews] RecentAnnotationColors` in `conf/okular.kcfg` |
| Colour cycle key `C` | `part/annotationactionhandler.cpp:2205` |
| Width presets 0.5–64, step 0.5 | `part/widthpresetutils.h:13-15` |
| Opacity presets 10–100 % | `part/annotationactionhandler.cpp:728` |
| Polygon fill / inner colour | `part/data/tools.xml`; `GeomPolygon` inner colour handling in `part/annotationactionhandler.cpp` |
| Clear current page `Ctrl+Shift+Delete` | `part/pageview.cpp:1032-1049` |
| `F6`, `Alt+1..4`, `Alt+6/7`, `1`, `B`, `Alt+Shift+*` shortcuts | `part/annotationactionhandler.cpp:2185-2205` |
| Pressure is session-only | `core/annotations.h` (`setSessionPressures`, “Not serialized”); `core/annotations.cpp` session-pressure members |
| Patched Poppler writes `/AP` | `patches/poppler-ink-appearance.patch`; `POPPLER_QT6_HAS_CUSTOM_INK_APPEARANCE` in the staging header and fork; `generators/poppler/annots.cpp` |
| Stock Poppler is not a supported substitute | The custom-stream block is guarded by the macro; the annotation-order calls are not guarded and do not exist in the system Poppler header |
| Annotation rotation not exposed | No rotation action/gesture found in `part/`; the frame work is in the working tree only |

### Page editing

| README claim | Evidence |
| --- | --- |
| PDF/Poppler only | `core/document.cpp` `Document::canModifyPages()` requires `poppler` in the generator name |
| Page insert/delete/reorder with undo | `core/document.cpp` `applyPageOrder()`, `insertPages()`, `removePages()`, `movePages()`; `PageStructureCommand` in `core/documentcommands_p.h` and `.cpp` |
| Bookmarks and viewport remapped, searches reset | `DocumentPrivate::beginPageStructureChange()` / `endPageStructureChange()` |
| Last page cannot be removed | `core/document.cpp` `removePages()` guard |
| QPDF page writing | `generators/poppler/generator_pdf.cpp` `applyPageLayout()` QPDF branch |
| `pdfseparate`/`pdfunite` fallback | `generators/poppler/generator_pdf.cpp` non-QPDF branch |
| Page save is not atomic | QPDF branch: `applyPageLayout()` writes `<file>.pages.tmp`, then removes and renames. Fallback: `QTemporaryDir`, then remove and copy. Both remove the original first. |
| Link/named-destination limitation | QPDF branch remaps outlines; no equivalent audit of page-content links was found |

### Autosave

| README claim | Evidence |
| --- | --- |
| Enabled by default, 60 s | `conf/okular.kcfg` `AutoSaveEnabled`, `AutoSaveIntervalSeconds` |
| No autosave for untitled documents | `Part::canAutoSaveNow()` checks `m_untitledDocument` |
| Refuses password-opened, non-local, read-only, unmodified, unopened documents | `Part::canAutoSaveNow()` |
| Refuses if mtime changed on disk | `m_fileLastModified != fi.lastModified()` |
| Refuses if saving would lose forms/annotations | `checkNativeSaveDataLoss()` |
| Waits for mouse release / modal / input quiet period | `Part::slotAutoSave()` guards and timer logic |
| No dedicated test | No `autosave`/`AutoSave` test found in `autotests/` |

### Document Builder

| README claim | Evidence |
| --- | --- |
| Stack page inside the part, not a dialog | `part/part.cpp` creates `m_documentBuilder` in `m_mainStack`; shell signals for visibility |
| Add Files accepts PDFs and images | `DocumentBuilderWidget::addFiles()`, `addFilesViaDialog()`, `supportedImagePatterns()` |
| PDF detected by suffix or `%PDF-` in first 1024 bytes | `looksLikePdf()` in `part/documentbuilder.cpp` |
| Passwords: three attempts, stored on the source, not logged | `part/documentbuilder.cpp:1166-1188`; `BuilderSource::password()` |
| Remote URLs rejected | `remoteFilesDropped` handler shows “Only local files can be added” |
| Grid columns auto or 2–6, size small/medium/large, file names toggle | `part/documentbuilderconfig.h` |
| Mouse-drag reorder, ghost card, insertion bar | `part/documentbuilder.cpp` drag methods |
| Alt+Up/Down and context-menu moves | `DocumentBuilderWidget::moveSelectionBy()` and context-menu actions |
| Add tile is permanent | `DocumentBuilderModel::rowCount() == entries + 1` |
| Export temp file in destination directory | `part/documentbuilderexport.cpp` `QTemporaryFile` with `.charta-XXXXXX.pdf` |
| Reopen and page-count verification | `documentbuilderexport.cpp` verification block |
| POSIX `::rename`, non-POSIX remove+rename | `documentbuilderexport.cpp` platform branch |
| Source conflict check (canonical path, device/inode) | `outputConflictsWithSource()` / `outputConflictsWithAnySource()` |
| Save only for a real local seeded PDF | `Part::slotOpenDocumentBuilder()`, `DocumentBuilderWidget::saveToTarget()` |
| Save resets builder and reloads | `Part::slotDocumentBuilderSaved()` |
| Export copies page content only | `QPDFPageDocumentHelper::addPage(sourcePage, false)`; no outline/form/metadata copying |
| Export synchronous and non-cancelable | `setExportBusy()`, no cancel button, GUI-thread call |
| Model and part tests exist | `autotests/documentbuildermodeltest.cpp`, `autotests/documentbuilderparttest.cpp` |

## 5. Corrections made to the earlier draft

| Earlier statement | Current finding |
| --- | --- |
| Colour cycle is `Ctrl+.` | The default shortcut is plain `C` (`annotationactionhandler.cpp:2205`). `CLAUDE.md` was stale on this point. |
| Page-structure save is atomic | It writes a staged result (QPDF: `<file>.pages.tmp`; fallback: a temporary directory), then removes the original before replacing it. Not atomic. |
| Document Builder replacement is always atomic | Atomic only on POSIX (`::rename`); non-POSIX uses remove+rename. |
| Live ink is rendered in tiles | No tile mechanism was found; the code uses a dirty rectangle. |
| Page reorder works from the page view too | Reorder is wired to thumbnail drag (`ThumbnailList::pagesReordered` → `Part::slotReorderPages`). |
| Lasso is “instant” / broken | Commit `8578911` changed `part/pageview.cpp` to fix it. The TODO entry is stale. |
| Pressure is never preserved | Nuance: the model is session-only, but the patched Poppler fork writes a pressure-shaped `/AP` stream into the PDF. |
| Stock Poppler works with only uniform ink | The current PDF backend does not compile against stock Poppler: it calls `annotationIndex()`, `annotationCount()`, and `moveAnnotation()`, which the fork adds. The patched fork is a build requirement, not only an appearance improvement. |
| The Poppler patch had no published source | The fork is now public at <https://github.com/Rupam3238/poppler-patch-for-charta-26.08.0> (GPL-2.0). The local patch file still covers only the ink-appearance part. |
| 44 tests is the project's test count | 44 was the existing build's registration. A fresh configure of the working tree registered 45; counts vary with optional backends and QPDF. |
| Annotation rotation is entirely absent | The working tree has an oriented-frame model and a test, but no UI gesture. The README calls it “not exposed in the UI.” |
| Text tool and other limitations are current bugs | They are maintained as project status/TODO items; not all were reproduced. The README labels them as limitations, not verified failures. |

Additional discrepancy: `Ctrl+Shift+B` is the shell's Document Builder shortcut and also the
part's “Add to quick tools” default (`annotationactionhandler.cpp:2204`). The README lists it
only for the Document Builder. Whether the part action shadows the shell action depends on
focus/context; this was not tested manually.

## 6. Supported formats

The format list comes from the generator `CMakeLists.txt` files and the MIME declarations in
`generators/`. It is an inherited capability list, not a test matrix.

The fresh configure found:

- Poppler 26.08.0 (staging), Freetype, libtiff, LibSpectre, KExiv2Qt6, DjVuLibre, EPub,
  Discount;
- Qt6 DBus, Qt6 Qml, Qt6 TextToSpeech, KF6 Wallet, KF6 Purpose;
- QMobipocket6 and CUPS were **not** found.

Therefore:

- PDF, image, DjVu, DVI, EPUB, FictionBook, Comic Book, Fax, Markdown, PostScript, TIFF, text,
  and XPS backends are present in the tree.
- Mobipocket and printing/CUPS support were not available in this configure.
- The README marks non-PDF formats as “inherited; optional; not independently verified”.
- Viewing a format does not imply annotation editing or page editing. Page editing is restricted
  to Poppler, and annotation saving depends on the backend.

The Document Builder's image filter is built from `QImageReader::supportedImageFormats()` at
runtime, so its exact extension list is environment-dependent.

## 7. Licensing and provenance

### What can be established

- Source files overwhelmingly declare `SPDX-License-Identifier: GPL-2.0-or-later`. In
  `core/`, `part/`, `shell/`, `generators/`, and `gui/`: 395 files.
- A few files declare other licenses: 4 `GPL-2.0-only`, 4 `LGPL-2.0-or-later`, 5
  `BSD-2-Clause`.
- `LICENSES/` contains the corresponding texts (GPL-2.0-only, GPL-2.0-or-later,
  LGPL-2.0-or-later, BSD-2-Clause, BSD-3-Clause, GFDL-1.2-or-later, and others).
- Most files retain KDE/Okular copyright headers.
- Four inspected files in `core/`/`part/` have no SPDX line:
  `part/actionbar.h`, `part/actionbar.cpp`, `part/signingcertificatelistmodel.h`,
  `part/signingcertificatelistmodel.cpp`. They may still carry copyright blocks; this was not
  audited file by file.
- There is **no top-level `COPYING` file**. Licensing is expressed through per-file headers and
  `LICENSES/`.
- The published Poppler fork is GPL-2.0 and based on Poppler 26.08.0. Its two commits
  (`5799dc0` baseline with custom ink appearance, `0fe4c9d` annotation reorder APIs) are
  authored by `Workspace Experiment <workspace@example.com>`.

### What cannot be established from the repository

- The exact KDE Okular source snapshot and the full upstream copyright history were not
  reconstructed. The repository history inspected here is a short, project-specific history
  with generic commit metadata, not a full upstream Okular history.
- The Drawy adaptation has no recorded source snapshot, commit, or license. The only concrete
  references are a comment at `part/annotationtools.cpp:85` (“Match Drawy's pressure response
  curve”) and the existing README attribution.
- The exact upstream Poppler commit used for the fork's baseline is not recorded in the fork's
  history, and the commits use a generic workspace identity rather than a human author.
  Poppler's own license and notices still apply to the modified code.
- The local Charta patch was added in commit `029b4ed` and modified in `088ce39`, both authored
  by the same generic identity. It covers only the ink-appearance portion.
- No signed contributor license agreement or file-by-file provenance manifest was found in the
  tree.

### AI disclosure

The README keeps the statement that AI tools were used to generate or modify essentially all of
the implementation under the maintainer's direction. This is a factual development-process
disclosure. It is **not** a copyright attribution, a license grant, or a substitute for
resolving the Drawy and Poppler provenance questions.

The README does not claim that disclosing these issues satisfies the licensing obligations. It
states that the Drawy question must be resolved before a stable release.

## 8. Checks on the documentation itself

Performed after writing the README and this file:

- relative-link targets were checked against the filesystem (see section 9 for the result),
- fenced code blocks were checked for balanced opening/closing fences,
- the logo path was checked against the tracked tree,
- no link points at `CLAUDE.md`, the local plan files, or other gitignored material,
- no link points at a screenshot (the repository has no screenshots; the README uses the SVG
  logo only).

## 9. Documentation link and code-fence check

Run from the repository root after writing the files:

```bash
python3 - <<'PY'
import pathlib, re
for doc in [pathlib.Path('README.md'), pathlib.Path('docs/architecture.md'), pathlib.Path('docs/README-VERIFICATION.md')]:
    text = doc.read_text()
    fences = text.count(chr(96) * 3)
    links = re.findall(r'\]\(([^)]+)\)', text)
    missing = []
    for link in links:
        if link.startswith(('http://', 'https://', '#', 'mailto:')):
            continue
        target = (doc.parent / link.split('#')[0]).resolve()
        if not target.exists():
            missing.append(link)
    print(f'{doc}: fences={fences} ({"balanced" if fences % 2 == 0 else "UNBALANCED"}), links={len(links)}, missing={missing}')
PY
```

Result of the run used for this snapshot: all three files had balanced fences; every relative
link resolved; no missing targets. The external links are the KDE Okular and KDE Drawy sites,
the Charta GitHub repository and issue tracker, and the Poppler fork repository. The two GitHub
repositories were checked with `git remote -v` / `git ls-remote`; the KDE sites were not
fetched.

## 10. Outstanding verification

The following are explicitly **not** verified and should be done before treating the README as a
release statement:

1. Clean clone + clean configure **with a network QPDF fetch** (the check reused a local source).
2. Full build of the clean tree, including the part, shell, Poppler generator, and tests.
3. Full test execution under Xvfb, with results recorded.
4. `cmake --install` into a staging prefix and a launch of the installed application.
5. Flatpak build and launch of `org.charta.reader`.
6. Manual UI pass: annotation toolbar, colour cycle, temporary pan/erase, page insert/delete/
   reorder/save, autosave, Document Builder drag/export/save.
7. Build the published Poppler fork, then build Charta against it; verify `/AP` save/reopen in
   an independent PDF viewer and verify PDF-backed annotation reordering.
8. Confirm that stock Poppler fails to compile the current PDF backend, as expected, and record
   the first missing symbol/API.
9. Page-removal audit: links, named destinations, outlines, bookmarks, and the annotation
   appearance cache.
10. Mobile/Kirigami build and non-Linux platforms.
11. File-by-file license and copyright audit; resolution of the Drawy snapshot/license and the
    Poppler fork provenance.
12. Update the Flatpak manifest to use the full published Poppler fork (or add the
    annotation-order patch) before attempting the build in item 5.
13. Resolution of the duplicate `Ctrl+Shift+B` default shortcut.
14. Tests for page structure, autosave, and the exporter's conflict/atomicity rules.
