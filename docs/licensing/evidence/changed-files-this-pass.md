# Files changed during the first pass

From `git status --porcelain` at the end of the first pass. The tree was being edited by
another agent at the same time; their rows are listed so that nothing is ambiguous about
who changed what. None of their files was modified, reverted or reformatted here.

| Changed by | Git | Path | Why |
|---|---|---|---|
| this pass | `M` | `.flatpak-manifest.json` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `D` | `.flatpak-patches/okular-mimetypes.patch` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `CMakeLists.txt` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `D` | `LICENSES/GPL-3.0-or-later.txt` | licence text added from a canonical source |
| this pass | `M` | `README.md` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `aboutdata.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/annotationcolorhistorytest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/annotationpersistencetest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/annotationrotationtest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/blankpageswritertest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/documentbuildermodeltest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/documentbuilderparttest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/untitleddocumenttest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `autotests/widthpresetstest.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `core/document.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| other agent | `M` | `docs/README-VERIFICATION.md` | not touched by this pass (the other agent owns these) |
| this pass | `M` | `generators/CMakeLists.txt` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `generators/comicbook/org.kde.okular-comicbook.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/djvu/org.kde.okular-djvu.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/dvi/org.kde.okular-dvi.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/epub/org.kde.okular-epub.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/fax/org.kde.okular-fax.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/fictionbook/org.kde.okular-fb.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/kimgio/org.kde.okular-kimgio.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/markdown/org.kde.okular-md.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/mobipocket/org.kde.okular-mobipocket.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/poppler/org.kde.okular-poppler.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/spectre/org.kde.okular-spectre.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/tiff/org.kde.okular-tiff.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/txt/org.kde.okular-txt.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `generators/xps/org.kde.okular-xps.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `mobile/app/org.kde.okular.kirigami.appdata.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `M` | `mobile/components/okularsingleton.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `okular_part.json` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/actionbar.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/actionbar.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/annotationcolorhistory.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/annotationcolorhistory.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/annotationtools.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/blankpageswriter.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/blankpageswriter.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuilder.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuilder.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuilderconfig.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuilderexport.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuilderexport.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuildermodel.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuildermodel.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuildersource.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/documentbuildersource.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/imagepagesettingsdialog.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/imagepagesettingsdialog.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/magnifierview.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/pagerangeinputdialog.cpp` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/pagerangeinputdialog.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `part/widthpresetutils.h` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `patches/poppler-annotation-rotation.patch` | regenerated Poppler patch (series verified byte-identical) |
| this pass | `M` | `patches/poppler-ink-appearance.patch` | regenerated Poppler patch (series verified byte-identical) |
| this pass | `M` | `shell/CMakeLists.txt` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `shell/org.charta.reader.desktop` | Phase 7/8 — namespace rename, SPDX headers, or packaging fix |
| this pass | `M` | `shell/org.charta.reader.metainfo.xml` | Phase 7 — AppStream project_license corrected |
| this pass | `??` | `.githooks/` | Phase 11 enforcement |
| this pass | `??` | `.github/` | Phase 11 enforcement |
| this pass | `??` | `CONTRIBUTING.md` | licence pointer / annotations / notices / policy |
| this pass | `??` | `LICENSE` | licence pointer / annotations / notices / policy |
| this pass | `??` | `LICENSES/Apache-2.0.txt` | licence text added from a canonical source |
| this pass | `??` | `LICENSES/CC0-1.0.txt` | licence text added from a canonical source |
| this pass | `??` | `LICENSES/LGPL-2.1-only.txt` | licence text added from a canonical source |
| this pass | `??` | `LICENSES/LicenseRef-Okular-upstream-terms.txt` | licence text added from a canonical source |
| this pass | `??` | `REUSE.toml` | licence pointer / annotations / notices / policy |
| this pass | `??` | `docs/QPDF-NOTICE.md` | licence pointer / annotations / notices / policy |
| this pass | `??` | `docs/THIRD-PARTY-NOTICES.md` | licence pointer / annotations / notices / policy |
| other agent | `??` | `docs/images/` | not touched by this pass (the other agent owns these) |
| this pass | `??` | `docs/licensing/` | evidence / analysis document |
| this pass | `??` | `patches/poppler-annotation-followup.patch` | regenerated Poppler patch (series verified byte-identical) |

75 entries: 73 from this pass, 2 from the other agent.
