# 02 — SPDX / REUSE gaps (Phase 2)

**Phase:** 2 — SPDX / REUSE baseline. **Measure only. No fix was applied.**
**Baseline SHA:** `73ee06351c30501c9aa990c7d976f8e604ef0c06` ("Docuementation Updatation").
**Evidence classes:** every number below is `E0` (command executed at the recorded SHA, output saved) unless
explicitly marked `E2` (upstream-compared) or `E3`.
**Artifacts:** `docs/licensing/spdx-matrix.csv`, `docs/licensing/evidence/phase2-scan.txt`,
`docs/licensing/evidence/phase2-reuse-lint.txt`.

> **Pinning note.** Other agents were working in the same checkout **and in the same
> `docs/licensing/` directory** during this phase. From 20:00 IST onwards they started applying fixes
> in the product tree (`REUSE.toml`, `LICENSE`, `CONTRIBUTING.md`, new `LICENSES/*.txt`, new SPDX
> headers on the 30 identifier-without-copyright files), and they have added their own Phase 1/3/4/6/7
> artifacts next to mine. Every measurement here is therefore taken with an explicit tree-ish —
> `git grep … 73ee063 --` and `git archive 73ee063` — **not** against the working tree, so the numbers
> below describe the **baseline commit** and cannot drift. Section 7 of `phase2-scan.txt` records the
> observed drift.
>
> This phase wrote only: `docs/licensing/spdx-matrix.csv`, `docs/licensing/spdx-gaps.md`,
> `docs/licensing/evidence/phase2-scan.txt`, `docs/licensing/evidence/phase2-reuse-lint.txt` and
> `docs/licensing/evidence/phase2-gap-classification.txt`. It created **no** `REUSE.toml`, added **no**
> SPDX header, downloaded **no** licence text, and modified **no** file outside `docs/licensing/`.

---

> **Note added after Phase 3 (2026-10-21).** This document compares against upstream Okular
> `master` @ `60fd9fbc` — the upstream *head* at the time of the phase — not against the fork
> point. Phase 3 pinned the actual base to **`e0fdfed749e8b4e83b9e788d559f5f2f8bfa5886`**
> (2026-09-24, two-commit window) with 1 727 of 1 935 files byte-identical. See
> [`okular-base.md`](okular-base.md). The counts in this document are unaffected; only the
> "upstream reference" label should be read as "upstream head", not "fork base".

## 1. The three headline numbers (`E0`)

| Measure | Count | Command |
| --- | --- | --- |
| Tracked files at the baseline SHA | **1 935** | `git ls-tree -r --name-only 73ee063 \| wc -l` |
| Files carrying an `SPDX-License-Identifier` | **503** | `git grep -l 'SPDX-License-Identifier' 73ee063 -- \| wc -l` |
| Files carrying an `SPDX-FileCopyrightText` | **738** | `git grep -l 'SPDX-FileCopyrightText' 73ee063 -- \| wc -l` |
| Files carrying **both** | **473** | set intersection |
| Files carrying an identifier but **no** copyright line | **30** | §5 |
| Files carrying a copyright line but **no** identifier | **265** | §4.1 (all of them are `po/`) |
| Files carrying **neither** | **1 167** | §4 |

Reconciliation: 473 + 30 + 265 + 1 167 = 1 935. ✔

`reuse` counts a slightly different denominator because it treats `LICENSES/*.txt` as licence *texts*
(not as files needing a tag) and skips symlinks: **1 922** files at the baseline SHA
(1 935 − 12 licence texts − 1 broken symlink `mobile/android/icon-playstore.png`).

## 2. Identifier histogram and text presence (`E0`)

| Identifier | Files | Text in `LICENSES/`? |
| --- | ---: | --- |
| `GPL-2.0-or-later` | 473 | yes |
| `X11` | 6 | yes |
| `LGPL-2.0-or-later` | 6 | yes |
| `BSD-2-Clause` | 6 | yes |
| `LGPL-2.0-only` | 4 | yes |
| `BSD-3-Clause` | 3 | yes |
| `GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL` | 2 | yes (all three) |
| `GPL-2.0-only OR GPL-3.0-only` | 2 | yes (both) |
| `MIT` | 1 | yes |
| **total** | **503** | |

**Identifiers in use with NO text in `LICENSES/`: _the set is empty_.** Every one of the nine
expressions above is fully covered by the twelve texts in `LICENSES/` at the baseline SHA.

### 2.1 Texts that are referenced in-tree but absent from `LICENSES/` (`E0`)

These are *not* "identifiers in use" — no file carries them as an `SPDX-License-Identifier` — but they
are named in-tree, so REUSE cannot resolve them and Phase 8 will need the text.

| Needed text | Motivating reference | Text at baseline SHA? |
| --- | --- | --- |
| `CC0-1.0` | **16 ×** `<metadata_license>CC0-1.0</metadata_license>` — 14 × `generators/*/org.kde.okular-*.metainfo.xml`, `shell/org.charta.reader.metainfo.xml:5`, `mobile/app/org.kde.okular.kirigami.appdata.xml:4` | **no** |
| `GFDL-1.3` / `GFDL-1.3-or-later` | **16 ×** `<project_license>GPL-2.0+ and GFDL-1.3</project_license>` — the same 16 files, line 6 (line 5 in the two non-generator files) | **no** |
| `Apache-2.0` | QPDF **v11.9.1** is `FetchContent`-built **static** and linked into `okularGenerator_poppler` — `CMakeLists.txt:232-300` (`set(BUILD_STATIC_LIBS ON)`, `FetchContent_Declare(qpdf … GIT_TAG v11.9.1)`, `QPDF_LINK_TARGET libqpdf`) | **no** |
| `LGPL-2.1-only` | `part/annotationtools.cpp:87` — a *comment* naming the Drawy snapshot's licence, not an SPDX tag | **no** |

The mirror-image case also exists: **`GFDL-1.2-or-later` is present in `LICENSES/` but is referenced by
no file at the baseline SHA** — all 16 AppStream files say `GFDL-1.3`. That is exactly why `reuse lint`
flags it as an unused licence.

Route for all four: `reuse download <SPDX-ID>` from the canonical SPDX licence list. Never retyped.
`CC0-1.0` and `GFDL-1.3` are also the two SPDX ids `appstreamcli check-license` confirms as
"Suitable for AppStream metadata: yes" (`phase2-scan.txt` §6).

### 2.2 The `LICENSES/` directory is inherited wholesale from Okular (`E2`)

The 12 texts tracked at the baseline SHA carry **exactly the same file names as upstream Okular**, in
the same order, and are **byte-identical** to them — verified file-by-file with `sha256sum`
(`phase2-scan.txt` §5). Upstream probe: Okular `60fd9fbcf5315c7d931d60f7d5270e7d1c0cc7b9`
(2026-10-13), which is also the SHA the plan's Phase 3 names as its candidate base.

Consequence: `LICENSES/` is **not** an independent statement about Charta's licensing. It happens to
cover every identifier in use only because the fork copied Okular's directory. `reuse lint` reports
`GFDL-1.2-or-later` and `GPL-3.0-or-later` as **unused** — they are present but referenced by no
`SPDX-License-Identifier` anywhere in the tree (`GFDL-1.2-or-later` is referenced by nothing at all at
the baseline SHA; `GPL-3.0-or-later` only appears as one alternative inside the compound OR expression
of `part/magnifierview.{cpp,h}`).

## 3. `reuse lint` baseline (`E0`)

`reuse` **was available**: `python3 -m venv /tmp/opencode/reuse-venv && pip install reuse` succeeded
(network reachable). **reuse 6.2.0**, REUSE Specification 3.3, lint_version 1.0. Nothing was installed
system-wide. Two runs are recorded in `phase2-reuse-lint.txt`:

| Category | Run A — repo root, phase start | Run B — pristine export of `73ee063` |
| --- | --- | --- |
| Files scanned | 1 932 (incl. 10 untracked concurrent files) | **1 922** |
| Files with copyright information | 1 427 | **1 426** |
| Files with licence information | 503 | **503** |
| Bad / deprecated / invalid expressions | 0 / 0 / 0 | 0 / 0 / 0 |
| Missing licences | 0 | 0 |
| Unused licences | `GFDL-1.2-or-later`, `GPL-3.0-or-later`, `LGPL-2.1-only` | `GFDL-1.2-or-later`, `GPL-3.0-or-later` |
| Read errors | 0 | 0 |
| Compliant | **no** | **no** |

Run B per-file breakdown (`reuse lint --json`):

| | licence present | no licence |
| --- | ---: | ---: |
| **copyright present** | 473 | 953 |
| **no copyright** | 30 | 466 |

`reuse`'s copyright detection is broader than the SPDX tag (it also accepts legacy `Copyright …` prose
headers), which is why it finds copyright on 1 426 files while `git grep SPDX-FileCopyrightText` finds
only 738.

**REUSE-relevant headline:** at the baseline SHA, **1 419 files have no `SPDX-License-Identifier`**
(953 with copyright but no licence + 466 with neither) and **496 files have no
`SPDX-FileCopyrightText`** (466 with neither + 30 with a licence but no copyright).

## 4. The 1 167 files with neither tag

### 4.1 By cause (`E0` + `E2`)

| Cause | Count | Share |
| --- | ---: | ---: |
| `translation-catalogue` | **804** | 68.8 % |
| `upstream-okular-gap` | **188** | 16.1 % |
| `binary/asset` | **116** | 9.9 % |
| `charta-original` | **38** | 3.3 % |
| `vendored` | **17** | 1.5 % |
| `scratch-file` | **4** | 0.3 % |
| **total** | **1 167** | 100 % |

Caveat on the denominator: of the 1 167, **12 are `LICENSES/*.txt`** (the licence texts themselves —
REUSE handles them separately, they are not a gap) and **1 is the broken symlink**
`mobile/android/icon-playstore.png` (REUSE skips symlinks). The real REUSE work list is **1 154 files**.

### 4.2 By directory (`E0`)

| Directory | Count | | Directory | Count |
| --- | ---: | --- | --- | ---: |
| `po/` | 804 | | `core/` | 4 |
| `generators/` | 131 | | `autotests/` | 3 |
| `part/data/` | 72 | | `docs/` | 2 |
| `autotests/data/` | 37 | | `icons/` | 2 |
| `charta_logo_package/` | 30 | | `patches/` | 2 |
| repository root | 29 | | `shell/` | 6 |
| `core/synctex/` | 17 | | `conf/` | 5 |
| `mobile/` | 12 | | `cmake/` | 1 |
| `part/` | 9 | | `.flatpak-patches/` | 1 |

### 4.3 Top 10 groups, with the specific files

Full per-file classification (all 1 167 rows, with the upstream-comparison columns) is in
`docs/licensing/evidence/phase2-scan.txt` §3 and in the machine-readable form referenced there.

**G1 — `translation-catalogue` / `po/` — 804 files.**
All of `po/<lang>/okular*.po` for 83 locales. Named (pattern-complete): `po/ar/okular_djvu.po`,
`po/ar/okular_epub.po`, `po/ar/okular_fax.po`, `po/ar/okular_fictionbook.po`, `po/ar/okular_ghostview.po`,
`po/ar/okular_kimgio.po`, `po/ar/okular_markdown.po`, `po/ar/okular_mobi.po`, `po/ar/okular_tiff.po`,
`po/ar/okular_txt.po`, `po/ar/okular_xps.po`, `po/az/okular_comicbook.po`, … `po/zh_TW/okular_xps.po`.
(The 265 that do carry `SPDX-FileCopyrightText` are **not** in this group — see §6.)

**G2 — `upstream-okular-gap` / `generators/` — 112 files.**
`generators/CMakeLists.txt`; then, per backend, `CMakeLists.txt`, `Messages.sh`,
`libokularGenerator_<x>.json`, `okularApplication_<x>.desktop`, `org.kde.mobile.okular_<x>.desktop`,
`org.kde.okular-<x>.metainfo.xml` for `comicbook`, `djvu`, `dvi`, `epub`, `fax`, `fictionbook`, `kimgio`,
`markdown`, `mobipocket`, `poppler`, `spectre`, `tiff`, `txt`, `xps`; plus
`generators/dvi/{TeXFont.cpp,TeXFontDefinition.cpp,TeXFontDefinition.h,TeXFont_PK.h,config.h,dvi.h,dviexport.cpp,dviexport.h,dvisourcesplitter.cpp,dvisourcesplitter.h,glyph.cpp,glyph.h,psheader.cpp,special.cpp,xdvi.h}`,
`generators/poppler/conf/{pdfsettings.kcfg,pdfsettings.kcfgc,pdfsettingswidget.ui}`,
`generators/spectre/{DESIGN,conf/gssettings.kcfg,conf/gssettings.kcfgc,conf/gssettingswidget.ui}`,
`generators/fictionbook/hi32-app-okular-fb2.svg`, `generators/epub/data/CMakeLists.txt`,
`generators/xps/.emacs-dirvars`.

**G3 — `binary/asset` / `part/data/` — 66 files.**
`part/data/{checkmark,circle,comment,cross,help,insert,key,newparagraph,note,paperclip,paragraph,pushpin,rightarrow,rightpointer,star}.png`;
`part/data/tool-base-okular.png`, `tool-base-okular@2x.png`, `tool-highlighter-okular-colorizable.png`,
`tool-highlighter-okular-colorizable@2x.png`, `tool-ink-okular-colorizable.png`,
`tool-ink-okular-colorizable@2x.png`, `tool-note-inline-okular-colorizable.png`,
`tool-note-inline-okular-colorizable@2x.png`, `tool-note-inline.png`, `tool-note-okular-colorizable.png`,
`tool-note-okular-colorizable@2x.png`, `tool-note.png`, `tool-typewriter-okular-colorizable.png`,
`tool-typewriter-okular-colorizable@2x.png`, `tool_hl_orange.png`, `tool_hl_pink.png`,
`tool_hl_yellow.png`, `tool_ink_green.png`, `tool_note.png`, `uparrow.png`, `upleftarrow.png`; and 26
`part/data/sources/*.svgz` (`checkmark.svgz` … `upleftarrow.svgz`, `tool-*-okular.svgz`).

**G4 — `charta-original` / `charta_logo_package/` — 30 files.**
`charta_logo_package/README.md`; `app-icon/charta-app-icon-{desktop,desktop-padded-transparent,transparent}-{512,1024}.png`;
`favicon/favicon-{16,32,48}.png`, `favicon/favicon.svg`;
`png/charta-logo-{16,24,32,48,64,128,256,512,1024}.png`;
`print/charta-logo-color.{eps,pdf}`, `print/charta-logo-monochrome.{eps,pdf}`;
`svg/charta-logo-color.svg`, `svg/charta-logo-color-padded.svg`, `svg/charta-logo-color-padded-background.svg`,
`svg/charta-logo-monochrome.svg`, `svg/charta-logo-monochrome-padded.svg`;
`charta-logo-color-white-background.svg`.

**G5 — `binary/asset` / `autotests/data/` — 29 files.**
`1500x300.png`, `300x1500.png`, `RequestFullScreen.pdf`, `additionalFormActions.pdf`, `checkbox_ro.pdf`,
`contents.epub`, `fake_okular_certstore/cert9.db`, `fake_okular_certstore/key4.db`, `fieldFormat.pdf`,
`file1.pdf`, `file1.pdf.gz`, `file2.pdf`, `formSamples.pdf`, `formattest.pdf`,
`hello_with_dummy_signature.pdf`, `kartöffelchen.jpg`, `keystroketest.pdf`, `kjsfunctionstest.pdf`,
`pdf_with_internal_links.pdf`, `pdf_with_links.pdf`, `pdf_with_signature.pdf`, `potato.jpg`,
`protected.pdf`, `simple-multipage.pdf`, `simpleCalculate.pdf`, `synctextest.pdf`,
`synctextest.synctex.gz`, `tocreload.pdf`, `visibilitytest.pdf`.

**G6 — `binary/asset` / `generators/` — 19 files.**
`generators/comicbook/autotests/data/rotated_cb.cbz`, `generators/epub/autotests/data/test.epub`,
`generators/epub/data/okular-epub-movie.png`, `generators/epub/data/okular-epub-sound-icon.png`,
`generators/fictionbook/{16,32,48}-apps-okular-fb2.png`,
`generators/kimgio/tests/data/testExifOrientation-{0,0mirror,180,180mirror,270,270mirror,90,90mirror,noexif,unspecified}.jpg`,
`generators/spectre/{16,32}-apps-okular-gv.png`.

**G7 — `vendored` / `core/synctex/` — 17 files.**
`core/synctex/002dcd3eac30db5c352f53d4181737961cc7ee9a.diff`,
`core/synctex/patches/{00-disable-SYNCTEX_INLINE,04-gcc-specify-printf-format,06-mingw-_synctex_error,08-fix_cpp_comments,10-fix-typo,11-fix-unused-parameters-warnings,12-omit-no-file-warning,13-fix-Wundef-warnings,14-fix-misc-compiler-warnings,15-prevent-leaks-and-segfault,16-use-windows-ansi-api-path-find,17-call-va_end-before-return,18-Include-stdarg.h-for-va_start-and-va_end}.diff`,
`core/synctex/patches/series`, `core/synctex/synctex_parser_readme.md`,
`core/synctex/synctex_parser_version.txt`.
(The five `core/synctex/synctex_parser*.[ch]` files **do** carry `SPDX-License-Identifier: X11` and are
not in this group; only the patch series, the readme and the version stamp are untagged.)

**G8 — `upstream-okular-gap` / `mobile/` — 9 files.**
`mobile/CMakeLists.txt`, `mobile/android/AndroidManifest.xml`, `mobile/android/src/OpenFileActivity.java`,
`mobile/app/CMakeLists.txt`, `mobile/app/Messages.sh`, `mobile/app/org.kde.okular.kirigami.appdata.xml`,
`mobile/app/package/metadata.desktop`, `mobile/components/Messages.sh`, `mobile/components/settings.h`.

**G9 — `upstream-okular-gap` / `part/` — 9 files.**
`part/CMakeLists.txt`, `part/actionbar.cpp`, `part/actionbar.h`, `part/part-viewermode.rc`,
`part/part.qrc`, `part/part.rc`, `part/selectcertificatedialog.ui`,
`part/signingcertificatelistmodel.cpp`, `part/signingcertificatelistmodel.h`.

**G10 — `upstream-okular-gap` / `autotests/data/` — 8 files.**
`autotests/data/fake_okular_certstore/pkcs11.txt`, `autotests/data/file1-docdata.xml`,
`autotests/data/imageSizes.md`, `autotests/data/imageUrlsWithSpecialChars.md`,
`autotests/data/pdf_with_internal_links.tex`, `autotests/data/simple-multipage.tex`,
`autotests/data/strikethrough.md`, `autotests/data/synctextest.tex`.

**Groups outside the top 10** (for completeness):
`upstream-okular-gap` at repository root — 25 (`.clang-tidy`, `.flatpak-manifest.json`,
`.git-blame-ignore-revs`, `.gitignore`, `CMakeLists.txt`, the 12 `LICENSES/*.txt`,
`Messages.sh`, `Okular6Config.cmake.in`, `OkularConfigureChecks.cmake`, `README.md`,
`config-okular.h.cmake`, `hooks/pre-commit`, `okular.categories`, `okular_part.json`);
`charta-original` — 38 total (30 logo files + `docs/architecture.md`,
`docs/README-VERIFICATION.md`, `icons/charta.svg`, `mobile/android/res/drawable/charta.xml`,
`patches/poppler-annotation-rotation.patch`, `patches/poppler-ink-appearance.patch`,
`shell/org.charta.reader.desktop`, `shell/org.charta.reader.metainfo.xml`);
`binary/asset` at `mobile/` — 2; `upstream-okular-gap` at `part/data/` — 6, `conf/` — 5, `core/` — 4,
`shell/` — 4, `autotests/` — 3, `cmake/` — 1, `icons/` — 1, `.flatpak-patches/` — 1;
`scratch-file` at repository root — 4 (`CMakeFiles/CMakeSystem.cmake`, `test_savedepth.cpp`,
`test_savedepth_simple`, `test_savedepth_simple.cpp`).

## 5. Gap group — files with an identifier but no copyright line (30)

Not part of the 1 167, but a REUSE gap in the other direction.

**26 of the 30 are Charta-new** (absent from upstream Okular at `60fd9fbc`): the annotation and
Document Builder work —
`autotests/annotationcolorhistorytest.cpp`, `autotests/annotationpersistencetest.cpp`,
`autotests/annotationrotationtest.cpp`, `autotests/blankpageswritertest.cpp`,
`autotests/documentbuildermodeltest.cpp`, `autotests/documentbuilderparttest.cpp`,
`autotests/untitleddocumenttest.cpp`, `autotests/widthpresetstest.cpp`,
`part/annotationcolorhistory.cpp`, `part/annotationcolorhistory.h`, `part/blankpageswriter.cpp`,
`part/blankpageswriter.h`, `part/documentbuilder.cpp`, `part/documentbuilder.h`,
`part/documentbuilderconfig.h`, `part/documentbuilderexport.cpp`, `part/documentbuilderexport.h`,
`part/documentbuildermodel.cpp`, `part/documentbuildermodel.h`, `part/documentbuildersource.cpp`,
`part/documentbuildersource.h`, `part/imagepagesettingsdialog.cpp`, `part/imagepagesettingsdialog.h`,
`part/pagerangeinputdialog.cpp`, `part/pagerangeinputdialog.h`, `part/widthpresetutils.h`.

**4 are inherited from upstream**, and upstream does not have a copyright line in them either:
`part/magnifierview.cpp`, `part/magnifierview.h` (`GPL-2.0-only OR GPL-3.0-only OR
LicenseRef-KDE-Accepted-GPL`) and `part/okmenutitle.cpp`, `part/okmenutitle.h` (`LGPL-2.0-only`).
These four are also the only Charta files not on `GPL-2.0-or-later`.

## 6. The `po/` translation catalogues — explicit statement

**The `po/` directory holds 1 069 tracked translation catalogues at the baseline SHA. Their licence
status is:**

- **0 of 1 069** carry an `SPDX-License-Identifier`.
- **265 of 1 069** carry an `SPDX-FileCopyrightText` line (translator credits, e.g.
  `# SPDX-FileCopyrightText: 2024 Enol P. <enolp@softastur.org>`).
- **804 of 1 069** carry neither tag.

**Upstream Okular also has no identifier there.** This is measured, not assumed (`E2`): at Okular
`60fd9fbcf5315c7d931d60f7d5270e7d1c0cc7b9`, `git grep -l 'SPDX-License-Identifier' -- po/` returns
**0** files out of **1 214** `po/` files. Upstream also has exactly **265** `po/` files with an
`SPDX-FileCopyrightText` line, and that set of 265 paths is **identical** to Charta's 265.

Charta's `po/` tree is a **strict subset** of upstream's: all 1 069 Charta `po/` paths exist upstream
(0 new paths), and the 145 upstream `po/` paths the fork does not have are translated *documentation*
assets (`po/<lang>/docs/okular/*.png`, `po/<lang>/docs/okular/index.docbook`), not catalogues. Both
trees have the same 83 locale directories.

**Correction to the plan (§2.3).** The plan states that "the 1 069 `po/` files carry
`SPDX-FileCopyrightText` lines (translator credits) but no `SPDX-License-Identifier`". The
identifier half is exactly right; the copyright half is true for only **265** of the 1 069, not for all
of them. The 265 figure is upstream's own number — Charta inherited it unchanged. The plan's derived
conclusion still holds (the `po/` licence cannot be inferred from a REUSE file, because Okular has
no `REUSE.toml` either, only `LICENSES/` and per-file headers), but the count is 265, not 1 069.

**Also note:** Okular upstream *does* tag two files `CC0-1.0` (`.craft.ini`, `.kde-ci.yml`). Charta
carries neither of those files and uses `CC0-1.0` only inside AppStream `<metadata_license>` elements,
which is why the `CC0-1.0` text is missing from `LICENSES/`.

## 7. Intended fix routes — **recorded only, nothing applied**

Nothing in this section has been executed. No `REUSE.toml` was created, no header was added, no
licence text was downloaded, and no file outside `docs/licensing/` was touched.

| Group | Cause | Route | Size |
| --- | --- | --- | ---: |
| G1 `po/` | translation catalogue | **one `REUSE.toml` annotation** for `po/**` — never per-file headers: these files are regenerated by `lupdate`/`lrelease` and any header would be overwritten. The identifier still has to be *decided* (Phase 6) and the `po/` grant is not inherited from Okular. | 804 |
| G2 `generators/` (text) | upstream Okular gap | **per-file headers** for the 15 C++ sources in `generators/dvi/` (they are Okular sources; `GPL-2.0-or-later` + the legacy prose copyright they already carry); **`REUSE.toml` group** for the 97 packaging/build files (`.desktop`, `.metainfo.xml`, `.json`, `CMakeLists.txt`, `Messages.sh`, `.kcfg(c)`, `.ui`, `.rc`, `.qrc`, `DESIGN`, `.emacs-dirvars`). | 112 |
| G3 `part/data/` | binary/asset | **`REUSE.toml` group**; artwork is upstream KDE artwork → the notice belongs in `docs/THIRD-PARTY-NOTICES.md`, not in a per-file header. | 66 |
| G4 `charta_logo_package/` | Charta-original | **`REUSE.toml` group**, but only **after** the maintainer picks the logo licence (D-decision): `CC0-1.0` if `CC-BY-SA-4.0` is unwanted, otherwise the share-alike terms need a `CC-BY-SA-4.0` text too. No header per PNG. | 30 |
| G5 `autotests/data/` (binary) | binary/asset | **`REUSE.toml` group**. | 29 |
| G6 `generators/` (binary) | binary/asset | **`REUSE.toml` group**. | 19 |
| G7 `core/synctex/` | vendored | **`REUSE.toml` group**, annotated with the SyncTeX licence (`X11`, matching the five `synctex_parser*` files that already say so) and a notice that the `.diff` files are a patch series against SyncTeX 1.19. | 17 |
| G8 `mobile/` | upstream Okular gap | **`REUSE.toml` group** (QML/Java/CMake/metadata). | 9 |
| G9 `part/` | upstream Okular gap | **per-file headers** for the 4 C++ files (`actionbar.*`, `signingcertificatelistmodel.*`); **`REUSE.toml` group** for `CMakeLists.txt`, `*.rc`, `*.qrc`, `*.ui`. | 9 |
| G10 `autotests/data/` (text) | upstream Okular gap | **`REUSE.toml` group** (`.tex`, `.md`, `.xml`, `pkcs11.txt`). | 8 |
| root / `conf` / `core` / `shell` / `icons` / `cmake` | upstream Okular gap | **`REUSE.toml` groups**; the 12 `LICENSES/*.txt` are excluded (REUSE handles them separately). | 47 |
| Charta-original non-logo (8 files) | Charta-original | **per-file headers** for `docs/architecture.md`, `docs/README-VERIFICATION.md`, `icons/charta.svg`, `mobile/android/res/drawable/charta.xml`, `shell/org.charta.reader.desktop`, `shell/org.charta.reader.metainfo.xml`; **`REUSE.toml` group** for `patches/poppler-*.patch` (they are patches, not code). | 8 |
| `scratch-file` | scratch | **remove, do not license**: `CMakeFiles/CMakeSystem.cmake` (a committed CMake artefact), `test_savedepth.cpp`, `test_savedepth_simple.cpp`, `test_savedepth_simple` (an ELF x86-64 executable committed to the tree). Removal is a maintainer action in Phase 8, not a licensing action. | 4 |
| 30 identifier-without-copyright files | 26 Charta-new + 4 inherited | **per-file header**: add `SPDX-FileCopyrightText` next to the existing identifier. | 30 |

**Estimated REUSE job size:** roughly **55 per-file header edits** (15 `generators/dvi/` sources +
4 `part/` sources + 6 Charta-original docs/icons/desktop/mobile files + the 30 files that need a
copyright line added) + **≈20 `REUSE.toml` annotation blocks** (one per group in the table above,
plus a few for the smaller directories) + **4 removals** + **4 licence texts to download**
(`CC0-1.0`, `GFDL-1.3-or-later`, `Apache-2.0`, `LGPL-2.1-only`) + **1 licence decision** (the logo).
The per-file header count is small; the `REUSE.toml` group count is what actually drives the work, and
it is dominated by the `po/` block (804) and the packaging-metadata blocks.

## 8. Limits, guesses and open items

1. **The upstream probe is a probe, not the Phase 3 pin.** Okular `master` HEAD was
   `60fd9fbcf5315c7d931d60f7d5270e7d1c0cc7b9` (2026-10-13) at clone time, which happens to be the SHA
   the plan's Phase 3 names as its candidate base. Phase 3 must still do the content-hash walk; this
   phase used a single upstream commit and only for "is this file tagged upstream?", never for
   provenance.
2. **"Content identical to upstream" was not verified for every file individually.** Of the 1 167,
   **1 125 exist upstream** at the same path and **42 do not** (38 classified `charta-original` + the
   4 `scratch-file` ones). Of the 1 125, **791 are byte-identical** to upstream (sha256), **333 differ**
   and **1 is the broken symlink** `mobile/android/icon-playstore.png`. The classification rests on
   "present upstream and untagged upstream", which *was* checked for every one of the 1 125. Phase 3
   owns the per-file identical/modified ledger.
3. **The 38 `charta-original` files are "absent upstream at the same path".** That is a path-level
   statement. Two of them (`mobile/android/res/drawable/charta.xml`, `icons/charta.svg`) may be
   Charta-local rework of upstream material — Phase 3/4 decides, not this phase.
4. **The `binary/asset` cause is extension- and NUL-byte-based**, not a content audit. A PNG that is
   really a script would be misfiled; none was observed.
5. **The `scratch-file` group is a judgement**, backed by `file(1)` output:
   `test_savedepth_simple` is an *ELF 64-bit LSB pie executable* committed to the tree, and
   `CMakeFiles/CMakeSystem.cmake` is a generated CMake artefact. `test_savedepth.cpp` and
   `test_savedepth_simple.cpp` are scratch C/C++ probes.
6. **`LGPL-2.1-only` appears only as a prose comment** (`part/annotationtools.cpp:87`), not as an SPDX
   tag, and the Drawy snapshot it refers to is not in this repository. Whether Charta's own code
   inherits LGPL-2.1-only is a **Phase 4 (Drawy disposition)** question. Phase 2 only records that the
   text would be needed.
7. **`GFDL-1.3` is written as a free-text licence string** in
   `mobile/app/org.kde.okular.kirigami.appdata.xml:5` (`GFDL-1.3`, not the SPDX id
   `GFDL-1.3-or-later`). `appstreamcli check-license GFDL-1.3` resolves it to the canonical
   `GFDL-1.3` id, so both spellings should be considered when the text is fetched.
8. **`reuse` was not run against a build of the product**, and no build was performed (out of scope
   for this phase). `reuse lint` is a static, header-only analysis.
9. **`reuse` scans the filesystem, not the Git index**, when no `.git` is present. Run B therefore
   needed the `git archive` export to be a true baseline. Run A is what the maintainer sees in the
   live checkout; both are recorded.
10. **Untracked concurrent changes were excluded from every number.** `docs/images/` (2 files),
    `docs/licensing/` (this phase's own output), `REUSE.toml`, `LICENSE`, `CONTRIBUTING.md` and three
    new `LICENSES/*.txt` appeared in the working tree during the phase and are **not** in the baseline.

## 9. How to re-run this phase

```sh
cd /home/workstation/tools/project/Charta
SHA=73ee06351c30501c9aa990c7d976f8e604ef0c06

# identifier histogram
git grep -h 'SPDX-License-Identifier' "$SHA" -- \
  | sed 's/.*SPDX-License-Identifier:[[:space:]]*//' | sed 's/[[:space:]]*$//' \
  | sort | uniq -c | sort -rn

# the three headline counts
git grep -l 'SPDX-License-Identifier'    "$SHA" -- | wc -l   # 503
git grep -l 'SPDX-FileCopyrightText'    "$SHA" -- | wc -l   # 738
comm -23 <(git ls-tree -r --name-only "$SHA" | sort) \
         <(git grep -l 'SPDX-License-Identifier\|SPDX-FileCopyrightText' "$SHA" -- \
            | sed "s/^$SHA://" | sort -u) | wc -l             # 1167

# reuse lint, deterministic baseline
python3 -m venv /tmp/opencode/reuse-venv
/tmp/opencode/reuse-venv/bin/pip install reuse
/tmp/opencode/reuse-venv/bin/reuse --version          # reuse, version 6.2.0
rm -rf /tmp/opencode/charta-head && mkdir -p /tmp/opencode/charta-head
git archive --format=tar "$SHA" | tar -x -C /tmp/opencode/charta-head
( cd /tmp/opencode/charta-head && /tmp/opencode/reuse-venv/bin/reuse lint )

# upstream probe
git clone --depth 1 --filter=blob:none https://invent.kde.org/graphics/okular.git /tmp/opencode/okular-probe
git -C /tmp/opencode/okular-probe grep -l 'SPDX-License-Identifier' 60fd9fbcf5315c7d931d60f7d5270e7d1c0cc7b9 -- po/ | wc -l   # 0
```

Full raw output: `docs/licensing/evidence/phase2-scan.txt` and
`docs/licensing/evidence/phase2-reuse-lint.txt`.
