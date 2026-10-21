# 01 — File census and origin classification (Phase 1)

**Phase:** 1 of the licensing plan (`/home/workstation/.opencode/plan/Licensing.md`, §4 "Phase 1").
**Audit base:** `HEAD` = `73ee06351c30501c9aa990c7d976f8e604ef0c06` ("Documentation Updatation").
**Upstream reference:** Okular `master` @ `60fd9fbcf5315c7d931d60f7d5270e7d1c0cc7b9` (2026-10-13) —
the same SHA the plan already pins in §2.4. Cloned read-only to `/tmp/opencode/okular`
(tree objects only; no blobs were needed, see §5).
**Machine-readable output:** `docs/licensing/file-classification.csv` — 1 936 lines
(header + **1 935 rows**, one per tracked file).
**Raw scan output:** `docs/licensing/evidence/phase1-scan.txt` (every count, every list).
**Date:** 2026-10-21.

---

> **Note added after Phase 3 (2026-10-21).** This document compares against upstream Okular
> `master` @ `60fd9fbc` — the upstream *head* at the time of the phase — not against the fork
> point. Phase 3 pinned the actual base to **`e0fdfed749e8b4e83b9e788d559f5f2f8bfa5886`**
> (2026-09-24, two-commit window) with 1 727 of 1 935 files byte-identical. See
> [`okular-base.md`](okular-base.md). The counts in this document are unaffected; only the
> "upstream reference" label should be read as "upstream head", not "fork base".

## 0. What this phase did and did not do

Did: counted and classified every tracked file, measured the SPDX/copyright state of every
tracked file, and compared every path (and every content hash) against upstream Okular.

Did **not**: modify a single product file, delete anything, run a build, or install anything.
The only files written are this document, the CSV, and the scan log under
`docs/licensing/evidence/`. No Git write command was run in this repository.

## 1. The unit of measurement, and why the working tree is not the unit

The census is over **tracked content**, i.e. the blob recorded in the Git index at `HEAD`
(`git ls-files -z` → 1 935 paths; `git cat-file --batch` → the exact bytes of each blob).
That choice is deliberate: the working tree is **not quiescent** and was moving during this
phase, so hashing the working tree would have produced a result that no longer existed minutes
later.

Observed working-tree drift *while this phase ran* (`E0`, `git status --porcelain` at the moment
this phase ended): **32 tracked paths modified or deleted, 12 untracked paths added.** The list grew
continuously during the phase — it was 2 modified files at the start, 5 midway, 31 at the end —
because other agents are working in the same checkout. The **index** itself is unchanged
(`git diff --cached` is empty, `git ls-files | wc -l` = 1 935, `HEAD` = `73ee063`), so the census
below is still a coherent description of one tree.

```
 M .flatpak-manifest.json          M aboutdata.h                  M core/document.cpp
 M CMakeLists.txt                  M generators/CMakeLists.txt     M mobile/components/okularsingleton.cpp
 M README.md                       M generators/*/org.kde.okular-*.metainfo.xml  (14 files)
 M docs/README-VERIFICATION.md     M okular_part.json              M part/actionbar.{cpp,h}
 M part/annotationtools.cpp        M patches/poppler-*.patch       M shell/CMakeLists.txt
 M shell/org.charta.reader.{desktop,metainfo.xml}
 D .flatpak-patches/okular-mimetypes.patch
?? .githooks/   ?? .github/    ?? CONTRIBUTING.md   ?? LICENSE      ?? REUSE.toml
?? LICENSES/CC0-1.0.txt   ?? LICENSES/LGPL-2.1-only.txt   ?? LICENSES/LicenseRef-Okular-upstream-terms.txt
?? docs/THIRD-PARTY-NOTICES.md   ?? docs/images/   ?? patches/poppler-annotation-followup.patch
```

Consequences:

1. The CSV describes the **committed** tree at `73ee063`, not the live checkout. A reviewer
   re-running the scan on the working tree will get up to 32 differing blobs and 12 extra
   untracked paths.
2. `LICENSES/LGPL-2.1-only.txt`, `LICENSES/CC0-1.0.txt` and
   `LICENSES/LicenseRef-Okular-upstream-terms.txt` now exist **untracked** in the working tree
   (added by another agent during this phase). They are *not* among the 1 935 tracked files and are
   therefore **not** in the census. Phase 8 must confirm they are byte-identical to the canonical
   SPDX texts and then have them committed by the maintainer.
3. `patches/`, the `org.kde.okular-*.metainfo.xml` set and `shell/` are being edited by other
   agents right now. The 14 generator metadata files, the two patch files and the two Charta
   identity files in the census are their `HEAD` versions; the working-tree versions differ.
   Phases 5, 7 and 8 own those file sets.
4. `.flatpak-patches/okular-mimetypes.patch` is **deleted in the working tree** (not staged). It
   is still tracked at `HEAD` and therefore still has a census row — the deletion is another
   agent's in-flight work, not a fact about the committed tree.
5. A top-level `LICENSE`, a `REUSE.toml`, `docs/THIRD-PARTY-NOTICES.md`, `CONTRIBUTING.md`,
   `.githooks/` and `.github/` appeared untracked. They are later-phase deliverables under
   construction by another agent; none of them is in the census.

## 2. Evidence classes used in the CSV

The `evidence_class` column uses the three marks of the Phase-1 brief:

| Mark | Meaning here |
| --- | --- |
| `E0` | The row's facts were **measured by an executed command** at the recorded SHA: `git ls-files -z`, `git cat-file --batch`, `git ls-tree -r` against the pinned upstream tree, and the byte-level SPDX scan. **1 928 rows.** |
| `E1` | The row's facts come from **reading a file**. No row's *verdict* rests on E1 alone; the two reads that exist (`shell/org.charta.reader.desktop:1-12`, `shell/org.charta.reader.metainfo.xml:3-5`, `charta_logo_package/README.md:1-12`) are cited in the `notes` column of rows whose origin verdict is `E3`. |
| `E3` | **Inferred / not established** — **7 rows**, listed in §8. |

The plan's own fourth mark, `E2` ("upstream-compared"), is used inside the `notes` column
because the origin verdict for 1 928 rows rests on an upstream comparison at a pinned SHA:
`path present in upstream Okular master @60fd9fbc (E2 path compare)` /
`blob SHA identical to upstream (E2)`. `E2` is a refinement of `E0` (it is also a command that
was run, with output saved), never a weaker claim, so the two marks never contradict.

## 3. Reconciliation

| Quantity | Measured here | Expected by the plan | Agrees |
| --- | --- | --- | --- |
| Tracked files | **1 935** | 1 935 (§2.2) | yes |
| CSV rows (excl. header) | 1 935 | 1 935 | yes |
| Σ category counts | 1 935 | 1 935 | yes |
| Σ `inherited_or_new` counts | 1 935 | 1 935 | yes |
| Files with an `SPDX-License-Identifier` | **503** | 503 (§2.3) | yes |
| Files with an `SPDX-FileCopyrightText` | **738** | "1 069 `po/` files carry it" (§2.3) | **no — corrected below** |

`git ls-files | wc -l` → `1935`; `git ls-files -z | tr -d '\0' | wc -l` → `1935` (the `-z` form
is what makes `autotests/data/kartöffelchen.jpg` survive intact).

## 4. Counts

### 4.1 Per category

| Category | Files | Notes |
| --- | ---: | --- |
| translations | 1 069 | `po/**`, 83 language directories, all `okular*.po` |
| source-part | 156 | `part/**` code |
| source-generators | 142 | `generators/**` code, 14 back-ends |
| source-core | 113 | `core/**` + `interfaces/**` + `aboutdata.h` + `kdocumentviewer.h` |
| assets-images | 78 | 45 `.png` + 30 `.svgz` + 3 `.svg`: `part/data/**` artwork (67), generator app icons (8), `core/stamps.svg`, `mobile/android/**` (2) |
| packaging | 66 | 30 `.desktop`, 16 `.json`, 16 `.metainfo.xml`/`.appdata.xml`, 3 `.patch` (`patches/**` 2 + `.flatpak-patches/**` 1), `okular.categories`, `.flatpak-manifest.json` |
| build-config | 56 | 25 `CMakeLists.txt`, 6 `*.cmake`, 2 `*.in`, 9 `*.kcfg`/`*.kcfgc`, 6 `*.ui`, 3 `*.qrc`, 3 `*.rc`, `.clang-tidy`, `_clang-format` |
| test-data | 49 | `autotests/data/**` (37) + generator test fixtures (12): 20 PDF, 12 JPG, 3 `.md`, 3 `.tex`, 2 PNG, 2 EPUB, 2 SQLite, 2 `.gz`, 1 `.txt`, 1 `.xml`, 1 CBZ |
| tests | 45 | 41 `*.cpp` + 3 `*.h` + 1 `CMakeLists.txt` |
| source-mobile | 36 | 17 `.qml`, 8 `*.cpp`, 8 `*.h`, 2 `.xml`, 1 `.java` |
| assets-logo | 30 | `charta_logo_package/**` |
| vendored | 23 | `core/synctex/**` (SyncTeX, X11) + its own `patches/` tree |
| scripts | 18 | 17 `Messages.sh` (16 in back-end directories + 1 at top level) + `hooks/pre-commit` |
| source-gui | 17 | `gui/**` |
| licence-texts | 12 | `LICENSES/*.txt` |
| source-shell | 9 | `shell/**` code |
| other | 7 | `.gitignore`, `.git-blame-ignore-revs`, `CMakeFiles/CMakeSystem.cmake`, `core/document.cpp.backup`, `test_savedepth.cpp`, `test_savedepth_simple.cpp`, `test_savedepth_simple` |
| documentation | 4 | `README.md`, `docs/architecture.md`, `docs/README-VERIFICATION.md`, `part/data/README.Icons` |
| assets-other | 3 | `part/data/drawingtools.xml`, `part/data/tools.xml`, `part/data/toolsQuick.xml` (runtime XML data) |
| assets-icons | 2 | `icons/charta.svg`, `icons/CMakeLists.txt` |
| **Total** | **1 935** | |

### 4.2 Per `inherited_or_new`

| Value | Files | Basis |
| --- | ---: | --- |
| `okular-inherited` | **1 864** | the path exists in upstream Okular `master` @ `60fd9fbc` |
| `charta-original` | **64** | the path does **not** exist upstream and is a Charta-specific path |
| `unknown-pending-phase-3` | **7** | the path does not exist upstream and its origin is *not* established — see §8 |
| **Total** | **1 935** | |

### 4.3 SPDX and copyright state

| Measure | Files |
| --- | ---: |
| **With** an `SPDX-License-Identifier` | **503** |
| **Without** an `SPDX-License-Identifier` | **1 432** |
| **With** an `SPDX-FileCopyrightText` | **738** |
| **Without** an `SPDX-FileCopyrightText` | **1 197** |

Identifiers actually in use (identical to the plan's §2.3 table — the counts match exactly):

| Identifier | Files |
| --- | ---: |
| `GPL-2.0-or-later` | 473 |
| `X11` | 6 |
| `BSD-2-Clause` | 6 |
| `LGPL-2.0-or-later` | 6 |
| `LGPL-2.0-only` | 4 |
| `BSD-3-Clause` | 3 |
| `GPL-2.0-only OR GPL-3.0-only` | 2 |
| `GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL` | 2 |
| `MIT` | 1 |

**Correction to the plan (§2.3).** The plan states that *all* 1 069 `po/` catalogues carry
`SPDX-FileCopyrightText`. Measured: **265 of 1 069 do, 804 do not** (`po/de/okular.po` has 13
copyright lines, `po/fr/okular.po` has 1, `po/ja/okular.po` has none). The accurate statement is
therefore "265 of 1 069 translation catalogues carry at least one copyright line; **none** carries
a licence identifier". The licence gap is unchanged; the copyright coverage is smaller than
recorded. This is `E0` and supersedes the plan's figure.

No file carries more than one distinct `SPDX-License-Identifier`, and no identifier was found
beyond byte 4 000 of any file — so the 503 count is free of false positives from body text.

**Genuine code gaps** (C/C++/Java sources with no identifier): 21 files — 15 under
`generators/dvi/`, `part/actionbar.{cpp,h}`, `part/signingcertificatelistmodel.{cpp,h}`,
`mobile/components/settings.h`, `mobile/android/src/OpenFileActivity.java`. The plan's figure of
"41 source files" counts `CMakeLists.txt`, `Messages.sh`, `.ui`/`.qrc`/`.rc` files as source
files; this census classifies those as `build-config`/`scripts` instead, which is why the number
here is 21. The underlying gap is the same one the plan describes (an upstream Okular gap).

## 5. Upstream comparison (what the origin column is built on)

Path existence **and** content identity were both established against the pinned upstream tree.
Content identity was obtained without downloading a single upstream blob: a Git blob SHA is
content-addressed, so `git ls-tree -r HEAD` in the upstream clone (trees only) gives the upstream
blob SHA per path, and equality with Charta's index SHA proves byte equality.

| Result | Files |
| --- | ---: |
| Inherited and **byte-identical** to upstream | **1 313** |
| Inherited, path exists but **content modified** in Charta | **551** |
| Charta-original (no upstream path) | 64 |
| Unknown | 7 |

The 551 modified files are listed one per line in `evidence/phase1-scan.txt` (Phase 3 should
start there). By area: `po/` 374 (consistent with the plan's "older translation sync"),
`generators/` 64, `part/` 38, `core/` 36, `shell/` 10, `mobile/` 8, `autotests/` 5, `gui/` 3,
`interfaces/` 2, `conf/` 1, `icons/` 1, and 9 top-level files (`.clang-tidy`,
`.flatpak-manifest.json`, `.gitignore`, `CMakeLists.txt`, `README.md`, `aboutdata.h`,
`config-okular.h.cmake`, `okular.categories`, `okular_part.json`).

This phase says **"the content differs"**, never **"the content is Okular's"**. Attribution of
the differences is Phase 3's job, not Phase 1's.

## 6. Largest and oddest groups

**Largest single file:** `core/synctex/synctex_parser.c` — 388 938 bytes (vendored SyncTeX).
Next: `generators/epub/autotests/data/test.epub` (357 713), `autotests/data/1500x300.png`
(303 461), `core/document.cpp` (254 319), `part/pageview.cpp` (239 478),
`core/document.cpp.backup` (238 352 — a stray copy of the previous file), `po/ta/okular.po`
(221 884), `core/stamps.svg` (212 832).

**No tracked file exceeds 1 MiB.** The 25 largest are listed in the scan log.

**Oddest groups:**

1. **`po/` — 1 069 files, 55 % of the tree.** No licence identifier anywhere; 804 have no
   copyright line either. 83 language directories, all named `okular*.po`. All inherited, 374
   modified. Largest single compliance hole in the tree and the reason Phase 6 exists.
2. **`core/synctex/` — 23 files.** Vendored third-party code (X11) with its own patch history
   (`002dcd3e….diff` plus a 14-file `patches/` series). Only 6 of the 23 carry an identifier;
   the patches do not. A separate provenance line from the rest of the tree.
3. **`charta_logo_package/` — 30 files.** Brand assets (SVG/PNG/EPS/PDF) plus a README. **No
   licence statement anywhere in the package**, and the package README itself says nothing about
   licensing. Per the plan §2.13 these are AI-generated; Phase 8 must state a position rather
   than assert authorship.
4. **`part/data/` — 72 files.** Runtime artwork (31 `.svgz`/`.svg` + 36 `.png` + 3 XML tool-definition
   files + `README.Icons` + `CMakeLists.txt`). **No identifier on any of them**; 3 of the XML
   files differ from upstream. Provenance per file is explicitly unanswered in the plan (§2.13).
5. **`LICENSES/` — 12 files.** All 12 are byte-identical to upstream Okular. They carry no
   SPDX tag of their own (correct for licence texts). The tree references four licences that
   are **absent from the tracked tree**: `CC0-1.0`, `GFDL-1.3`, `LGPL-2.1-only`, `Apache-2.0`.
   (`CC0-1.0.txt` and `LGPL-2.1-only.txt` appeared **untracked** in the working tree during this
   phase; they are not part of the 1 935 and are not in the census.)
6. **`CMakeFiles/CMakeSystem.cmake`** — a tracked CMake cache fragment from a local build
   (path, compiler, and system identification of the machine that ran the build).
7. **`test_savedepth.cpp`, `test_savedepth_simple.cpp`, `test_savedepth_simple`** — local
   scratch code plus a **compiled ELF binary** committed to the repository.
8. **`core/document.cpp.backup`** — 238 KB backup copy of a source file, tracked, not compiled.
9. **Three patch locations in one tree.** `patches/poppler-*.patch` (Charta-authored, against Poppler
   26.08.0) and `core/synctex/patches/*.diff` (vendored, against SyncTeX) plus
   `.flatpak-patches/okular-mimetypes.patch` (inherited from upstream). The last one patches
   `shell/org.kde.okular.desktop`, which does not exist in this tree.

## 7. Anomalies

| Anomaly | Count | Detail |
| --- | ---: | --- |
| **Broken symlink** | 1 | `mobile/android/icon-playstore.png → ../../icons/512-apps-okular.png`; the target does not exist (`icons/` holds only `CMakeLists.txt` and `charta.svg`). The symlink blob is 31 bytes (the target string). Already reported in Phase 0; still broken. |
| **ELF binaries** | 1 | `test_savedepth_simple` (16 944 bytes) — a compiled executable tracked in the repository. |
| **Binary files** | 137 | PNG/JPG/SVGZ/PDF/EPUB/CBZ/SQLite/`.gz` — not SPDX-scannable; the CSV records them as such. |
| **CRLF line endings** | 0 | No tracked text file contains a `\r\n` sequence. |
| **Non-UTF-8 encodings** | 0 | All 1 798 text files decode as UTF-8; no BOM, no UTF-16, no Latin-1. |
| **Files > 1 MiB** | 0 | Largest is 388 938 bytes. |
| **Tracked paths missing from the working tree** | 1 | `.flatpak-patches/okular-mimetypes.patch` — deleted in the working tree by another agent during this phase, still tracked at `HEAD` (the index is unchanged), so it keeps its census row. Two dozen-odd further tracked paths have working-tree content that differs in size from their `HEAD` blob; the exact list is in the scan log (it changes as the other agents work — re-run the scan for the current state). |
| **Unusual extensions** | 7 kinds | `.backup` (1, `core/document.cpp.backup`), `.Icons` (1, `part/data/README.Icons`), `.cbz` (1), `.epub` (2), `.db` (2), `.gz` (2), `.eps` (2), `.diff` (14), `.patch` (3), `.tex` (3), `.java` (1), `.js` (1), `.categories` (1), plus 9 extension-less files (`.clang-tidy`, `.git-blame-ignore-revs`, `.gitignore`, `_clang-format`, `core/synctex/patches/series`, `generators/spectre/DESIGN`, `generators/xps/.emacs-dirvars`, `hooks/pre-commit`, `test_savedepth_simple`). |
| **Non-ASCII paths** | 1 | `autotests/data/kartöffelchen.jpg` — present and hashed correctly; it is *not* missing. Shell loops without `-z` mis-report it. |
| **Upstream paths absent from this tree** | 222 | 145 of them are `po/<lang>/docs/okular/*` hand-book images and translations; the rest are the upstream hand-book (`doc/**`), upstream CI (`.gitlab-ci.yml`, `.kde-ci.yml`), the upstream icon set (`icons/*-apps-okular.png`, `icons/small/hi*-apps-okular.svgz`) and `metainfo.yaml`. Their absence is not a defect, but it is the reason `mobile/android/icon-playstore.png` is broken: the file it points at is one of the removed upstream icons. |

The 222 upstream paths that Charta does not have are listed in the scan log's upstream-diff
section; they are the `doc/` hand-book, upstream CI (`.gitlab-ci.yml`, `.kde-ci.yml`), the upstream
icon set (`icons/*-apps-okular.png`, `icons/small/hi*-apps-okular.svgz`), `metainfo.yaml` and the
hand-book translations.

## 8. What could not be classified, and why

Seven files are marked `unknown-pending-phase-3` (`E3`): the path is absent from upstream Okular
@ `60fd9fbc`, and it is neither demonstrably inherited nor a Charta-authored source file, so the
column has no better honest answer. Each one is listed with the reason below.

| Path | Category | Why unresolved |
| --- | --- | --- |
| `shell/org.charta.reader.desktop` | packaging | A **renamed and rewritten** Okular file: upstream has `shell/org.kde.okular.desktop`; this one has `Name=Charta` and the Charta MIME types (read: `shell/org.charta.reader.desktop:1-12`). The content *is* Okular-derived, but the path is new and the byte-level lineage is not established. Phase 3 must diff it against the upstream desktop file. |
| `shell/org.charta.reader.metainfo.xml` | packaging | Same situation: upstream has `shell/org.kde.okular.appdata.xml`; this file has `<id>org.charta.reader</id>` (read: `shell/org.charta.reader.metainfo.xml:3-5`) and still carries `metadata_license = CC0-1.0` / `project_license = GPL-2.0+ and GFDL-1.3`. |
| `core/document.cpp.backup` | other | A backup copy of a source file that *is* upstream. Origin is "a copy of an upstream file", not authorship, and the copy may predate or postdate Charta's edits. |
| `CMakeFiles/CMakeSystem.cmake` | other | A tracked CMake artefact from a local build; it is nobody's "source". |
| `test_savedepth.cpp` | other | Local scratch source; not product code (plan §2.11). |
| `test_savedepth_simple.cpp` | other | Local scratch source; not product code. |
| `test_savedepth_simple` | other | Local scratch **compiled binary**. |

Five of the seven are removal candidates under decision D6 (`CMakeFiles/CMakeSystem.cmake`,
`core/document.cpp.backup`, `test_savedepth.cpp`, `test_savedepth_simple.cpp`,
`test_savedepth_simple`): they should be deleted by the maintainer, not given a licence header.
The two `shell/org.charta.reader.*` files are the opposite case — they are product files whose
lineage Phase 3 must establish and whose identity Phase 7 must finish.

### Guesses, stated plainly

1. **`charta-original` for 14 files beyond the paths the Phase-1 brief enumerated.** The brief
   named `charta_logo_package/`, `icons/charta.svg`, `part/documentbuilder*`,
   `part/pagerangeinputdialog*`, `part/imagepagesettingsdialog*`, `part/annotationcolorhistory*`,
   `docs/` and `patches/` — 50 files. The same rule was applied to 14 further files that are
   absent from upstream and are named after Charta-only features:
   `part/blankpageswriter.{cpp,h}`, `part/newpagedialog.{cpp,h}`, `part/widthpresetutils.h`,
   `mobile/android/res/drawable/charta.xml`, and 8 new `autotests/*test.cpp` files (annotation
   colour history, annotation persistence, annotation rotation, blank-page writer, document
   builder model, document builder part, untitled document, width presets). Each of these 14 is
   listed in the scan log; a reviewer who disagrees with any of them can flip one column cell
   and the counts in §4.2 still sum to 1 935.
2. **Category boundaries.** `.ui`, `.qrc`, `.rc`, `.kcfg`, `CMakeLists.txt` and `Messages.sh` are
   classified as `build-config`/`scripts` even when they live inside `core/`, `part/` or
   `generators/`. The plan's "41 source files without a header" counts them as source files; this
   census does not. Both views are in §4.3.
3. **"Inherited" means "the path exists upstream"**, not "the content is upstream's". 551 of the
   1 864 inherited files are modified in Charta. No file was attributed to a specific upstream
   *commit*; the upstream reference is a single pinned SHA.

## 9. Handover to the next phases

| Phase | What this census hands over |
| --- | --- |
| 2 (SPDX baseline) | The 503/1 432 and 738/1 197 split, the 21 code files with no identifier, and the corrected `po/` copyright figure. The per-row `spdx_identifier` column is the starting matrix. |
| 3 (Okular lineage pin) | The 1 313 byte-identical / 551 modified split and the 551-file diff list. The upstream clone is at `/tmp/opencode/okular` (and a full clone at `/tmp/opencode/okular-full`), both @ `60fd9fbc`. |
| 4 (Drawy) | Not touched by this phase. `part/annotationtools.cpp` is one of the 551 modified files. |
| 5 (Poppler) | `patches/**` is Charta-original (2 files); `core/synctex/patches/**` is vendored (14 files). Both patch files are being edited in the working tree right now. |
| 6 (dependencies/assets) | 30 `charta_logo_package/` files with no licence statement; 72 `part/data/` artwork files with no identifier; 49 test-data files; 23 vendored SyncTeX files. |
| 7 (identity) | `shell/org.charta.reader.desktop` and `.metainfo.xml` are the two renamed-and-rewritten identity files; besides them, 14 inherited `org.kde.okular-*.metainfo.xml` files, `mobile/app/org.kde.okular.kirigami.appdata.xml` and 29 inherited `.desktop` files still carry the Okular identity. |
| 8 (headers/notices) | The 1 432 files without an identifier, of which 1 069 are `po/` and 137 are binary. |

## 10. Commands

```sh
git -C /home/workstation/tools/project/Charta rev-parse HEAD
git -C /home/workstation/tools/project/Charta ls-files -z            # 1935 paths
git -C /home/workstation/tools/project/Charta ls-files -s -z          # mode + blob SHA per path
git -C /home/workstation/tools/project/Charta cat-file --batch-check # blob sizes
git -C /home/workstation/tools/project/Charta cat-file --batch       # blob contents (SPDX scan)
git -C /tmp/opencode/okular ls-tree -r -z HEAD                      # upstream paths + blob SHAs
git -C /tmp/opencode/okular ls-tree -r --name-only HEAD              # 2086 upstream paths
git -C /tmp/opencode/okular-full rev-parse HEAD                      # 60fd9fbc… (full clone, content available)
git -C /home/workstation/tools/project/Charta status --porcelain=v1  # tree not quiescent
python3 /tmp/opencode/census.py                                      # the scan that wrote both artefacts
```

The scan script itself is not committed (it lives outside the repository, in `/tmp/opencode/`);
the outputs it produced are committed-grade artefacts. Re-running it requires the upstream clone
to exist; without it, the origin column degrades to `E3`/`unknown-pending-phase-3` for every
row, as the Phase-1 brief anticipated.
