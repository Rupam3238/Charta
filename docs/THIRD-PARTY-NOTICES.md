# Third-party notices

**Project:** Charta — a document reader and annotator derived from KDE Okular.
**Project licence:** GPL-2.0-or-later, per file (SPDX headers).
**Documentation:** GFDL-1.2-or-later.
**Date:** 2026-10-21

This file lists the third-party material in the distribution, the licence that applies, and what
this project does about it. The evidence behind each entry is in `docs/licensing/`.

## 1. KDE Okular — the base application

- **Upstream:** <https://invent.kde.org/graphics/okular>
- **Based on:** Okular `master` (see `docs/licensing/okular-base.md` for the pinned commit and the
  method used to pin it)
- **Licence:** GPL-2.0-or-later (per file, as in upstream)
- **What this project does:** keeps every upstream copyright notice and SPDX header unchanged,
  states that the code has been modified, ships the licence text
  (`LICENSES/GPL-2.0-or-later.txt`), and offers the corresponding source for every binary it
  distributes (see `docs/licensing/RELEASE-CHECKLIST.md`).
- **Modifications:** Charta adds the Document Builder, annotation improvements, colour history,
  rotation support, the `charta_generators` plugin namespace, and the AI-assisted work described in
  `docs/licensing/`.

## 2. KDE Drawy — one pressure-mapping expression

- **Upstream:** <https://invent.kde.org/graphics/drawy>
- **Snapshot:** `35194a7ec850ee54454917597f34d5b4c62bc81c` (2025-06-07), at that time named
  *Drawly*
- **Licence:** **LGPL-2.1-only** — the snapshot's `LICENSE` is LGPL 2.1 and there is no "or later"
  statement anywhere in it. Text: `LICENSES/LGPL-2.1-only.txt`
- **Used:** one arithmetic expression, `pressure() / 1.60 + 0.375`, in
  `part/annotationtools.cpp`, derived from `src/controller/controller.cpp:45` of that snapshot and
  bounded with `qBound`.
- **Not from Drawy:** the seven-point stroke smoothing in this project. That extends Okular's own
  `SmoothPathEngine`; the Drawy snapshot contains no smoothing code.
- **Today:** Drawy is licensed GPL-3.0-or-later and no longer contains this expression, so the
  current upstream file is not the source.
- **Evidence:** `docs/licensing/drawy-provenance.md`,
  `docs/licensing/evidence/drawy-35194a7e-controller.txt`

## 3. Poppler — patched dependency

- **Upstream:** <https://poppler.freedesktop.org/poppler-26.08.0.tar.xz>
- **Archive sha256:** `dc906e68cea698109706ac6aa3d2c9d4512fcfcac42d90b8afcda486d1b9abd0`
- **Licence:** GPL-2.0-or-later
- **What this project does:** ships Poppler's licence and notices, and ships the **complete patch
  series** that turns the pristine archive into the library used here:
  `patches/poppler-ink-appearance.patch`, `patches/poppler-annotation-rotation.patch`,
  `patches/poppler-annotation-followup.patch`. The series is verified to reproduce the built tree
  byte-for-byte (see `docs/licensing/poppler-provenance.md`).
- **Fork-only features:** `Annotator::setUserRotation()`, `setAppearanceStream()`, page reorder,
  ink appearance support.

## 4. QPDF — statically linked

- **Upstream:** <https://github.com/qpdf/qpdf> (v11.9.1, fetched at build time)
- **Licence:** Apache-2.0
- **What this project does:** ships the Apache-2.0 text and the upstream NOTICE with the binary,
  and states that QPDF is included unmodified. Apache-2.0 §4 requires this.
- **Shipped as:** `LICENSES/Apache-2.0.txt` (SPDX canonical text) and `docs/QPDF-NOTICE.md`
  (byte-identical copy of `NOTICE.md` from the fetched qpdf v11.9.1 source, sha256
  `0ea78699c51316d2a5c4f5d45c606f288288704ba3b86ae11b8338963495e1d3`). The NOTICE also records
  third-party material inside QPDF (the Artistic-2.0-licensed qtest, public-domain Rijndael code,
  sphlib). qtest is **not** compiled into the shipped library (verified: no qtest target in the
  QPDF build), so the Artistic-2.0 text itself is not required to be shipped; the NOTICE states
  those terms and where to obtain the text.

## 5. Qt 6 and KDE Frameworks 6 — runtime dependencies

- **Licence:** Qt 6 LGPL-3.0-or-later (also GPL-3.0 and commercial); KF 6 mostly LGPL-2.1-or-later
- **What this project does:** links dynamically against the `org.kde.Platform` runtime, which is a
  separate Flatpak ref and therefore not conveyed by this application. The runtime's own
  corresponding source is the runtime project's, not this project's.
- **If that ever changes** — bundling or statically linking Qt — the LGPL-3.0 text must be added
  and the analysis redone.

## 6. Optional build-time components

| Component | Version | Licence | Notes |
| --- | --- | --- | --- |
| libspectre | 0.2.12 | GPL-2.0-or-later | optional; **not** shipped in the Flatpak (D11) |
| Ghostscript | 10.08.0 | AGPL-3.0 | **not** shipped in the Flatpak (D11); see `docs/licensing/agpl-ghostscript.md` |
| DjVuLibre | 3.5.30 | GPL-2.0-or-later | optional module |
| ebook-tools | (sourceforge) | GPL-2.0-or-later | optional module |
| kdegraphics-mobipocket | 26.08.1 | GPL-2.0-or-later | optional module |
| libkexiv2 / KExiv2 | 26.08.1 | GPL-2.0-or-later | optional module |
| exiv2 | v0.28.9 | GPL-2.0-or-later | optional module |
| discount | v3.0.2.0 | BSD-3-Clause | optional module |
| inih | r62 | BSD-3-Clause | optional module |
| libzip | 1.11.4 | BSD-3-Clause | optional module |
| boost | 1.92.0 | BSL-1.0 | headers only; **text not yet in `LICENSES/`** |
| poppler-data | 0.4.12 | MIT-style + separate Adobe CMap terms | data files; **text not yet in `LICENSES/`** |
| gnustep-make / libs-base | 2_9_3 / 1.31.1 | GPL-2.0-or-later / LGPL-2.1 | optional module; one upstream GNUstep patch (gdomap signal handler) |
| unarchiver (XADMaster) | universal-detector | LGPL-2.1 | optional module |
| Freetype, libtiff, zlib | system | FTL / libtiff / zlib | system libraries; **texts not yet in `LICENSES/`** |

Anything listed as "optional module" is built into the Flatpak and therefore conveyed: the
corresponding source of those modules is their own upstream project's, and the licence texts
listed above must be present in the shipped package.

## 7. Vendored code

- **SyncTeX parser** — `core/synctex/`, X11 licence (per-file headers), carrying its own local
  patch history. Copyright notice retained.

## 8. Translations

- **KDE Okular catalogues** — `po/`, 1 069 files, copied verbatim; **no licence identifier** in any
  of them, and only 265 carry a translator copyright line (upstream is the same). The interim
  classification `LicenseRef-Okular-upstream-terms` is a **placeholder, not a permission**: the
  redistribution terms are still unresolved and are being confirmed with KDE l10n. Interim position:
  `LICENSES/LicenseRef-Okular-upstream-terms.txt`; open question recorded in
  `docs/licensing/translations.md`.
- Installed with the `okular` translation domain, so Charta's new strings are untranslated until
  the domain changes.

## 9. Test fixtures and assets

- **`autotests/data/`** — Okular's test corpus, kept unmodified, GPL-2.0-or-later.
- **`part/data/sources/*.svgz`** — 30 annotation artwork files, byte-identical to Okular,
  GPL-2.0-or-later.
- **`charta_logo_package/`, `icons/charta.svg`** — 31 files, **generated with AI tools**. No human
  authorship claim is made; the project is the licensing party. See
  `docs/licensing/assets.md`.

## 10. AI-generated content

The code in this repository was produced with AI tools and reviewed by the maintainer. The
project does not claim human authorship of generated code. Provenance relative to Okular, Drawy
and Poppler is documented with hashes and diffs in `docs/licensing/`. This section is a statement
of position, not a licence grant, and it does not substitute for the obligations above.

**This notice does not establish legal clearance.** Two questions remain open and are set out in
[`docs/licensing/legal-open-questions.md`](licensing/legal-open-questions.md):

* the copyright position of AI-generated code (and of the AI-generated brand assets), and
* the licence treatment of the Drawy-derived expression (LGPL-2.1-only, and the scope of LGPL-2.1
  section 3 when the licensed material is an application rather than a library).

Both need qualified legal advice before any definitive claim is made. Nothing in this document
should be read as such a claim.

## 11. Corresponding source

For every binary this project distributes, the corresponding source is the source tree at the
release tag plus the patch series in `patches/`. The per-channel offer is recorded in
`docs/licensing/RELEASE-CHECKLIST.md` before any release is published.
