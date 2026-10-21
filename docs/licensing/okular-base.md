# Okular base commit — pinned

**Phase 3 output.** Evidence class: **E2** (upstream-compared) for every content claim below;
the raw logs are in [`evidence/phase3-scan.txt`](evidence/phase3-scan.txt).

---

## 1. The pinned base

| | |
| --- | --- |
| **`OKULAR_BASE`** | **`e0fdfed749e8b4e83b9e788d559f5f2f8bfa5886`** |
| Date | **2026-09-24 17:32:36 +0200** |
| Subject | `Add Plasma/6.8 union to the development flatpak` |
| Where | `https://invent.kde.org/graphics/okular.git`, branch `master` |
| Charta side | Charta `HEAD` = `73ee06351c30501c9aa990c7d976f8e604ef0c06` (parent `0f13a44`) |

**Fork window: `{a525c38dce07483093fab36398a8d9dacfc1965e, e0fdfed749e8b4e83b9e788d559f5f2f8bfa5886}`** — two consecutive upstream commits,
2026-09-24 01:48:50 +0000 and 2026-09-24 17:32:36 +0200. `e0fdfed749e8` is the **newest** commit that explains
the tree, so it is the pin (plan P3-03: "record the *latest* commit that explains the
majority"). The two candidates are content-equivalent for everything Charta carries: the
only file `e0fdfed749e8` changes is `.flatpak-manifest.json`, and Charta's copy of that file
is modified in any case.

### Confidence: **high for "forked from Okular `master`", medium-high for the exact commit**

* The evidence for the *lineage* is decisive: 1727 of the 1935 tracked Charta files
  are **byte-identical** to the same paths at this commit, and the per-commit match curve
  has a single, clean maximum — a textbook fork signature (§3).
* The evidence for the *exact SHA* is bounded but tight. The fork window is 2 commits wide
  because no upstream source file that Charta carries changed between them. Each bound is
  pinned by specific files (§3.3). We cannot narrow further with content alone, and we do
  not claim to.
* This is **not** a release-tag fork. The base is a `master` commit from 2026-09-24, after
  the `26.08.1` release (2026-09-10). The README may therefore say "based on Okular commit
  `e0fdfed749e8`", and must **not** say "based on Okular 26.08.1".

---

## 2. Method

Exactly the procedure in Appendix C of the plan, executed over **all 1935 tracked
Charta files** rather than a 60-file sample, so the ledger in
[`modified-upstream-files.csv`](modified-upstream-files.csv) is complete.

1. **Clone upstream** (shallow, blob-filtered) and fetch the range that brackets the fork.
2. **Sweep every commit** in the range: for each tracked Charta file, compare the SHA-256 of
   the local file content with the SHA-256 of `git show <commit>:<path>` upstream. Count the
   matches per commit. The commit with the maximum count is the base.
   * `git hash-object` / `git rev-parse` are **not** used: the tree has no `.gitattributes`,
     so the two methods happen to agree today, but filter behaviour is a trap.
3. **Verify** the winning commit with the literal
   `sha256sum <file>` vs `git show <commit>:<path> | sha256sum` pipeline, file by file.
4. **Classify** every tracked path as `identical` / `modified` / `absent-upstream`, and read
   the upstream licence identifier and copyright notice from the upstream file at the base
   commit.

### Match rate at the base commit

| Population | Count |
| --- | --- |
| Tracked files in Charta | 1935 |
| Present upstream at the base (inherited) | 1864 |
| **Byte-identical to upstream** | **1727 = 92.65 % of inherited, 89.25 % of all tracked** |
| Present upstream but modified by Charta | 137 |
| Absent upstream (Charta-original) | 71 |

---

## 3. Why this commit — the per-commit match curve

### 3.1 The curve

Over the 153 commits of the scanned range the number of tracked Charta files whose
content is byte-identical to the upstream content at that commit rises monotonically (with a
single one-file dip at index 85, explained in §3.4), **peaks**, and then falls monotonically.
That is the signature of a single fork point: before the fork Charta holds content that
upstream has since changed; after the fork upstream moves on and Charta no longer matches.

```
commits:  153 scanned (2026-08-02 .. 2026-10-13)
matches:  1297/1935 .................................. 1727/1935 ......... 1313/1935
          ^ shallow boundary                        ^ peak (2 commits)          ^ master HEAD
```

The full per-commit table is in [`evidence/phase3-scan.txt`](evidence/phase3-scan.txt), §2.

### 3.2 The peak

Maximum = **1727 / 1935**, reached at exactly two commits:

```
a525c38dce07483093fab36398a8d9dacfc1965e  2026-09-24 01:48:50 +0000  GIT_SILENT Sync po/docbooks with svn
e0fdfed749e8b4e83b9e788d559f5f2f8bfa5886  2026-09-24 17:32:36 +0200  Add Plasma/6.8 union to the development flatpak
```

### 3.3 The bounds that make the window 2 commits wide

*Lower bound* — Charta carries content that upstream **introduced** at `a525c38dce07483093fab36398a8d9dacfc1965e`, so the fork is
at or after it:

```
po/ar/okular.po
po/ar/okular_poppler.po
po/lt/okular.po
```

(The other 1724 identical files were introduced upstream at
30 different commits, all of them at or before the
base — the three files above are simply the *newest* upstream content that Charta carries,
and they are what fixes the lower bound.)

*Upper bound* — Charta carries content that upstream **replaced** at
`b6f1eccb04b9` (`Adapt to newer clang-format`,
2026-09-25 11:11:08 +0000), so the fork is at or before the commit preceding it:

```
.clang-tidy
core/bookmarkmanager.cpp
generators/djvu/kdjvu.cpp
generators/dvi/TeXFont_PK.cpp
generators/dvi/dvisourcesplitter.cpp
part/DistinguishedNameParser.h
```

Combined: `a525c38dce07 <= F <= e0fdfed749e8`. The pin is the newest member of that window.

### 3.4 No mixed lineage

Every one of the 1727 identical files matches across the **whole** peak window, and no
file matches only in an *older* era of the history. One file, `part/videowidget.h`, has a
non-contiguous match window (matches at commits 84–85 and 88–152, gap 86–87): upstream
changed the file at `f6268b73b5` and reverted it two commits later, so Charta's copy is
consistent with the post-88 content, which is the content at the base. This is the only
non-contiguous case, and it is the single dip visible in the curve of §3.1. It does not
contradict the pin.

---

## 4. Files NOT explained by the base commit

**137 modified** and **71 absent upstream** out of 1935 tracked files. The full
per-file ledger with a one-line description of every change, the upstream blob SHA, the
upstream licence identifier and the upstream copyright notice is in
[`modified-upstream-files.csv`](modified-upstream-files.csv). The 95 non-branding
changed files are:

```
.flatpak-manifest.json                           .gitignore
CMakeLists.txt                                   README.md
aboutdata.h                                      autotests/CMakeLists.txt
autotests/addremoveannotationtest.cpp            autotests/annotationtoolbartest.cpp
autotests/parttest.cpp                           autotests/translateannotationtest.cpp
conf/okular.kcfg                                 config-okular.h.cmake
core/annotations.cpp                             core/annotations.h
core/annotations_p.h                             core/area.cpp
core/audioplayer.cpp                             core/document.cpp
core/document.h                                  core/document_p.h
core/documentcommands.cpp                        core/documentcommands_p.h
core/generator.cpp                               core/generator.h
core/generator_p.h                               core/observer.cpp
core/observer.h                                  core/page.cpp
core/page.h                                      core/page_p.h
generators/dvi/dviFile.cpp                       generators/dvi/dviRenderer.cpp
generators/dvi/dviexport.cpp                     generators/dvi/fontpool.cpp
generators/fictionbook/hi32-app-okular-fb2.svg   generators/poppler/CMakeLists.txt
generators/poppler/annots.cpp                    generators/poppler/annots.h
generators/poppler/generator_pdf.cpp             generators/poppler/generator_pdf.h
generators/poppler/pdfsettingswidget.cpp         gui/guiutils.cpp
gui/pagepainter.cpp                              icons/CMakeLists.txt
interfaces/saveinterface.h                       mobile/android/AndroidManifest.xml
mobile/android/src/OpenFileActivity.java         mobile/app/main.cpp
mobile/app/org.kde.okular.kirigami.appdata.xml   mobile/app/package/metadata.desktop
mobile/app/ui/Main.qml                           okular.categories
okular_part.json                                 part/annotationactionhandler.cpp
part/annotationactionhandler.h                   part/annotationmodel.cpp
part/annotationpopup.cpp                         part/annotationpopup.h
part/annotationpropertiesdialog.cpp              part/annotationtools.cpp
part/annotationtools.h                           part/annotationwidgets.cpp
part/data/tools.xml                              part/dlgannotations.cpp
part/dlgeditor.cpp                               part/dlggeneral.cpp
part/editannottooldialog.cpp                     part/editdrawingtooldialog.cpp
part/magnifierview.cpp                           part/pageview.cpp
part/pageview.h                                  part/pageviewannotator.cpp
part/pageviewannotator.h                         part/pageviewmouseannotation.cpp
part/pageviewmouseannotation.h                   part/part-viewermode.rc
part/part.cpp                                    part/part.h
part/part.rc                                     part/preferencesdialog.cpp
part/presentationwidget.cpp                      part/thumbnaillist.cpp
part/thumbnaillist.h                             part/videowidget.cpp
part/widgetdrawingtools.cpp                      shell/CMakeLists.txt
shell/main.cpp                                   shell/okular_main.cpp
shell/shell.cpp                                  shell/shell.h
shell/shell.qrc                                  shell/shell.rc
shell/welcomescreen.cpp                          shell/welcomescreen.h
shell/welcomescreen.ui                           
```

plus the 42 generator branding files (14 x `generators/*/okularApplication_*.desktop`,
14 x `generators/*/org.kde.mobile.okular_*.desktop`,
14 x `generators/*/org.kde.okular-*.metainfo.xml`).

**71 absent upstream** (the Charta-original set) — all of them are Charta work, not
upstream renames, except the two marked `<-` as derived in the CSV:

```
CMakeFiles/CMakeSystem.cmake                                   autotests/annotationcolorhistorytest.cpp
autotests/annotationpersistencetest.cpp                        autotests/annotationrotationtest.cpp
autotests/blankpageswritertest.cpp                             autotests/documentbuildermodeltest.cpp
autotests/documentbuilderparttest.cpp                          autotests/untitleddocumenttest.cpp
autotests/widthpresetstest.cpp                                 charta_logo_package/README.md
charta_logo_package/app-icon/charta-app-icon-desktop-1024.png  charta_logo_package/app-icon/charta-app-icon-desktop-512.png
charta_logo_package/app-icon/charta-app-icon-padded-transparent-1024.png charta_logo_package/app-icon/charta-app-icon-padded-transparent-512.png
charta_logo_package/app-icon/charta-app-icon-transparent-1024.png charta_logo_package/app-icon/charta-app-icon-transparent-512.png
charta_logo_package/charta-logo-color-white-background.svg     charta_logo_package/favicon/favicon-16.png
charta_logo_package/favicon/favicon-32.png                     charta_logo_package/favicon/favicon-48.png
charta_logo_package/favicon/favicon.svg                        charta_logo_package/png/charta-logo-1024.png
charta_logo_package/png/charta-logo-128.png                    charta_logo_package/png/charta-logo-16.png
charta_logo_package/png/charta-logo-24.png                     charta_logo_package/png/charta-logo-256.png
charta_logo_package/png/charta-logo-32.png                     charta_logo_package/png/charta-logo-48.png
charta_logo_package/png/charta-logo-512.png                    charta_logo_package/png/charta-logo-64.png
charta_logo_package/print/charta-logo-color.eps                charta_logo_package/print/charta-logo-color.pdf
charta_logo_package/print/charta-logo-monochrome.eps           charta_logo_package/print/charta-logo-monochrome.pdf
charta_logo_package/svg/charta-logo-color-padded-background.svg charta_logo_package/svg/charta-logo-color-padded.svg
charta_logo_package/svg/charta-logo-color.svg                  charta_logo_package/svg/charta-logo-monochrome-padded.svg
charta_logo_package/svg/charta-logo-monochrome.svg             core/document.cpp.backup
docs/README-VERIFICATION.md                                    docs/architecture.md
icons/charta.svg                                               mobile/android/res/drawable/charta.xml
part/annotationcolorhistory.cpp                                part/annotationcolorhistory.h
part/blankpageswriter.cpp                                      part/blankpageswriter.h
part/documentbuilder.cpp                                       part/documentbuilder.h
part/documentbuilderconfig.h                                   part/documentbuilderexport.cpp
part/documentbuilderexport.h                                   part/documentbuildermodel.cpp
part/documentbuildermodel.h                                    part/documentbuildersource.cpp
part/documentbuildersource.h                                   part/imagepagesettingsdialog.cpp
part/imagepagesettingsdialog.h                                 part/newpagedialog.cpp
part/newpagedialog.h                                           part/pagerangeinputdialog.cpp
part/pagerangeinputdialog.h                                    part/widthpresetutils.h
patches/poppler-annotation-rotation.patch                      patches/poppler-ink-appearance.patch
shell/org.charta.reader.desktop  <- shell/org.kde.okular.desktop shell/org.charta.reader.metainfo.xml  <- shell/org.kde.okular.appdata.xml
test_savedepth.cpp                                             test_savedepth_simple
test_savedepth_simple.cpp                                      
```

---

## 5. Limits of the method

* **Shallow clone.** The scan covers 2026-08-02 → 2026-10-13 (153 commits), the
  range `git fetch --shallow-since=2026-08-01 origin master` provides. Anything older is not
  in the clone. This is *not* a limitation for the conclusion: the match curve is still
  rising at the shallow boundary and peaks well inside the range, so the fork is inside the
  window. It **would** have been a limitation had the peak sat on the boundary.
* **Blob filtering.** The clone is `--filter=blob:none` (a promisor clone). The blobs for
  the scanned range were materialised locally with one extra
  `git fetch --refetch --filter=blob:limit=20m origin master`; the results are identical
  either way, but without that step every `git show` would be a network round-trip.
  `GIT_NO_LAZY_FETCH=1` was set during the sweep so that a missing blob would *fail loudly*
  rather than be silently fetched.
* **Sample size.** None in the usual sense: the sweep covers **all 1935 tracked
  files**, not a sample of 50–60. The plan's 60-file minimum is exceeded by a factor of
  32. The plan's sample frame alone (`core/`, `part/`, `generators/`, `shell/`,
  `gui/`, `mobile/`, `autotests/`) contributes 783 files.
* **The fork window cannot be closed by content alone.** Two consecutive upstream commits
  are indistinguishable because no source file that Charta carries changed between them.
  A stronger pin would need the Charta import timestamp from the maintainer, or the
  `git log` of the Charta import commit. Recorded honestly rather than guessed.
* **The ledger is pinned to Charta `HEAD` `73ee063`**, not to the working tree. A concurrent
  job is editing the checkout; the working-tree comparison is recorded separately in the
  evidence log (§4a) and differs from the committed-content comparison (§4b) only in those
  files. Everything in this document — the match counts, the CSV and the §7 gap list —
  describes the tree **as committed at `73ee063`**. A later phase has since started adding
  SPDX headers to some of the files listed in §7; that is a *local* change and does not
  change the upstream facts recorded here, but it does mean the §7 list is a statement
  about `73ee063`, not about the current working tree.
* **Absent-upstream is a path statement, not a provenance statement.** "Absent upstream"
  means the *path* does not exist at the base commit. For two files the upstream ancestor
  is at a different path (`shell/org.charta.reader.desktop` <- `shell/org.kde.okular.desktop`,
  `shell/org.charta.reader.metainfo.xml` <- `shell/org.kde.okular.appdata.xml`); the CSV
  records those explicitly. For the other 69 absent paths no upstream counterpart was
  found.
* **`po/` catalogues.** All 1069 tracked `po/` files are byte-identical to upstream but carry
  **no SPDX identifier upstream either** (see §7). Their licence is unresolved upstream and
  is *not* resolved here — that is plan item P6-04, not a Phase 3 result.

---

## 6. How to reproduce this

```bash
# 1. upstream clone (plan Appendix C, P3-01)
git clone --filter=blob:none https://invent.kde.org/graphics/okular.git /tmp/opencode/okular
git -C /tmp/opencode/okular fetch --shallow-since=2026-08-01 origin master

# 1b. materialise the blobs of the scanned range (speed only; results unchanged)
git -C /tmp/opencode/okular fetch --refetch --filter=blob:limit=20m origin master

# 2. the base commit, and the file-by-file check
BASE=e0fdfed749e8b4e83b9e788d559f5f2f8bfa5886
git -C /tmp/opencode/okular show -s --format='%H %ci %s' $BASE

#    identical?
diff -q <(git -C /home/workstation/tools/project/Charta show HEAD:part/actionbar.cpp) \
        <(git -C /tmp/opencode/okular show $BASE:part/actionbar.cpp) && echo IDENTICAL

#    sha256 pair for any path
sha256sum /home/workstation/tools/project/Charta/part/actionbar.cpp
git -C /tmp/opencode/okular show $BASE:part/actionbar.cpp | sha256sum

# 3. the per-commit sweep (plan Appendix C), for every tracked Charta file
cd /home/workstation/tools/project/Charta
git ls-files -z > /tmp/opencode/tracked.bin
git -C /tmp/opencode/okular rev-list --reverse origin/master | while read -r c; do
  m=0; t=0
  while IFS= read -r -d '' f; do
    t=$((t+1))
    l=$(sha256sum "/home/workstation/tools/project/Charta/$f" | cut -d' ' -f1)
    u=$(git -C /tmp/opencode/okular show "$c:$f" 2>/dev/null | sha256sum | cut -d' ' -f1)
    [ "$l" = "$u" ] && m=$((m+1))
  done < /tmp/opencode/tracked.bin
  echo "$c $m/$t"
done | sort -k2 -t' ' -r | head

# 4. the upstream licence identifier of an inherited file
git -C /tmp/opencode/okular show $BASE:part/ktreeviewsearchline.cpp | head -12
```

The raw output of steps 1–4, the full per-commit table, the file-by-file SHA-256 comparison
and the per-file "different commit" list are in
[`evidence/phase3-scan.txt`](evidence/phase3-scan.txt).

---

## 7. Upstream gaps — inherited files that lack an SPDX header **upstream too**

These files are **byte-identical to upstream** and have **no `SPDX-License-Identifier` in the
upstream file either**. They are *upstream* gaps, not Charta defects: Charta did not strip
anything. Phase 2 needs this list so that the header work is not misattributed.

92 inherited source files are in this class (excluding the 1069 `po/` catalogues,
which are listed separately in §5, and excluding binary test data, the `LICENSES/` licence
texts and the `core/synctex/patches/*.diff` patch files):

```
Messages.sh
Okular6Config.cmake.in
OkularConfigureChecks.cmake
autotests/closedialoghelper.cpp
autotests/closedialoghelper.h
cmake/modules/FindDjVuLibre.cmake
conf/okular_core.kcfg
conf/settings.kcfgc
conf/settings_core.kcfgc
conf/settings_mobile.kcfgc
core/chooseenginewidget.ui
core/script/builtin.qrc
core/stamps.svg
core/synctex/synctex_parser_version.txt
core/textdocumentsettings.ui
generators/CMakeLists.txt
generators/comicbook/CMakeLists.txt
generators/comicbook/Messages.sh
generators/comicbook/libokularGenerator_comicbook.json
generators/djvu/CMakeLists.txt
generators/djvu/Messages.sh
generators/djvu/libokularGenerator_djvu.json
generators/dvi/CMakeLists.txt
generators/dvi/Messages.sh
generators/dvi/TeXFont.cpp
generators/dvi/TeXFontDefinition.cpp
generators/dvi/TeXFontDefinition.h
generators/dvi/TeXFont_PK.h
generators/dvi/config.h
generators/dvi/dvi.h
generators/dvi/dviexport.h
generators/dvi/dvisourcesplitter.cpp
generators/dvi/dvisourcesplitter.h
generators/dvi/glyph.cpp
generators/dvi/glyph.h
generators/dvi/libokularGenerator_dvi.json
generators/dvi/psheader.cpp
generators/dvi/special.cpp
generators/dvi/xdvi.h
generators/epub/CMakeLists.txt
generators/epub/Messages.sh
generators/epub/libokularGenerator_epub.json
generators/fax/CMakeLists.txt
generators/fax/Messages.sh
generators/fax/libokularGenerator_fax.json
generators/fictionbook/CMakeLists.txt
generators/fictionbook/Messages.sh
generators/fictionbook/libokularGenerator_fb.json
generators/kimgio/CMakeLists.txt
generators/kimgio/Messages.sh
generators/kimgio/libokularGenerator_kimgio.json
generators/markdown/CMakeLists.txt
generators/markdown/Messages.sh
generators/markdown/libokularGenerator_md.json
generators/mobipocket/CMakeLists.txt
generators/mobipocket/Messages.sh
generators/mobipocket/libokularGenerator_mobi.json
generators/poppler/Messages.sh
generators/poppler/conf/CMakeLists.txt
generators/poppler/conf/pdfsettings.kcfg
generators/poppler/conf/pdfsettings.kcfgc
generators/poppler/conf/pdfsettingswidget.ui
generators/poppler/libokularGenerator_poppler.json
generators/spectre/CMakeLists.txt
generators/spectre/Messages.sh
generators/spectre/conf/CMakeLists.txt
generators/spectre/conf/gssettings.kcfg
generators/spectre/conf/gssettings.kcfgc
generators/spectre/conf/gssettingswidget.ui
generators/spectre/libokularGenerator_ghostview.json
generators/tiff/CMakeLists.txt
generators/tiff/Messages.sh
generators/tiff/libokularGenerator_tiff.json
generators/txt/CMakeLists.txt
generators/txt/Messages.sh
generators/txt/libokularGenerator_txt.json
generators/xps/CMakeLists.txt
generators/xps/Messages.sh
generators/xps/libokularGenerator_xps.json
mobile/CMakeLists.txt
mobile/app/CMakeLists.txt
mobile/app/Messages.sh
mobile/components/Messages.sh
mobile/components/settings.h
part/CMakeLists.txt
part/actionbar.cpp
part/actionbar.h
part/part.qrc
part/selectcertificatedialog.ui
part/signingcertificatelistmodel.cpp
part/signingcertificatelistmodel.h
shell/MacOSXBundleInfo.plist.in
```

The complete, machine-readable form of this list is the set of rows in
[`modified-upstream-files.csv`](modified-upstream-files.csv) with
`status == identical` **and** `licence == none` and a source-file extension.

For completeness, a further 60 files that Charta **modified** also have no
upstream SPDX identifier; they are not "identical to upstream" and therefore not part of the
upstream-gap list, but the same upstream evidence applies to them.

### The six files the plan asked to be checked individually

| File | Status vs upstream | Upstream licence identifier **exactly as upstream has it** |
| --- | --- | --- |
| `part/actionbar.cpp` | identical | **none** — the upstream file has a KDE GPL notice ("either version 2 of the License, or (at your option) any later version", i.e. GPL-2.0-or-later) but **no `SPDX-License-Identifier:` line at all** |
| `part/signingcertificatelistmodel.cpp` | identical | **none** — the upstream file has **no header whatsoever**; it starts at `#include "signingcertificatelistmodel.h"` |
| `part/ktreeviewsearchline.cpp` | identical | `LGPL-2.0-only` |
| `part/okmenutitle.cpp` | identical | `LGPL-2.0-only` |
| `part/magnifierview.cpp` | **modified** (Charta changed it) | `GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL` |
| `generators/comicbook/qnatsort.cpp` | identical | `GPL-2.0-only OR GPL-3.0-only` |

Two of the six (`part/actionbar.cpp`, `part/signingcertificatelistmodel.cpp`) have **no
upstream identifier at all**; the other four carry one upstream. `part/magnifierview.cpp` is
*not* byte-identical — Charta modified it — so it is not an upstream gap, but its upstream
identifier is recorded here because Phase 2 has to reconcile the local header with it.

Upstream Okular has **no `REUSE.toml`** at this commit; it uses per-file `SPDX-` headers
plus a `LICENSES/` directory. 474 of the 1864 inherited files carry an upstream SPDX
identifier; 1390 do not (1069 of those are `po/` catalogues).

---

## 8. What the rest of the plan may now rely on

* **Gate 3 (P3-03/P3-04)**: `OKULAR_BASE` is a SHA, not a branch. OK
* The README may say *"based on Okular commit `e0fdfed749e8` (2026-09-24)"*. It may **not** say
  *"based on Okular 26.08.1"* — the fork is from `master`, after that release. OK
* D2 (per-file licence answers) can be answered from the `licence` and `notice` columns of
  the CSV for every inherited path. OK
* Phase 8 (headers) must treat the 92 files in §7 as upstream gaps and must not
  treat them as "headers Charta stripped". OK
* The two files with a different upstream ancestor path (`shell/org.charta.reader.desktop`,
  `shell/org.charta.reader.metainfo.xml`) are the only cases where "absent upstream" hides
  inherited content; the other 69 absent paths are Charta-original. OK
