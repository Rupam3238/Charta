# Asset provenance (Phase 6)

**Evidence class:** `E0` for the comparisons, `E3` for the maintainer-supplied origins.
**Date:** 2026-10-21

## 1. `part/data/sources/*.svgz` — 30 files: **inherited from Okular**

All 30 `.svgz` annotation artwork files are **byte-identical to Okular `master`**
(`git show origin/master:part/data/sources/<file>` compared by SHA-256; 30/30 identical, 0 different,
0 absent upstream). This closes the open question from the plan (section 2.13): they are **not** AI
artefacts and not Charta-original.

- Licence: **GPL-2.0-or-later** (Okular's), unchanged.
- Obligation: keep the upstream attribution; they travel with the source and need no separate
  notice entry beyond Okular's.

## 2. `charta_logo_package/` — 30 files: **AI-generated**

| Group | Count | Format |
| --- | --- | --- |
| `print/` | 4 | 2 x PDF, 2 x EPS (print masters) |
| `svg/` | 7 | SVG |
| `png/` | 18 | PNG (raster sizes) |
| `favicon/`, `app-icon/`, `README.md` | 1 | assorted |

Provenance (maintainer, 2026-10-21): **produced by AI tools**. The maintainer has no commissioned
artwork and no third-party asset licence to point at.

What this means, stated without embellishment:

- The files carry **no human authorship claim** and no "commissioned artwork" claim.
- Copyrightability of AI output is jurisdiction-dependent and unresolved; the plan's D9 position
  applies to the assets exactly as it applies to the code.
- Practical obligations that remain: state a licence for the assets (D10: keep them, with an
  explicit notice), and do **not** assert a copyright holder that does not exist.
  `SPDX-FileCopyrightText` for these files should name the project as the licensing party, not an
  individual artist.
- The print masters (PDF/EPS) are the D10 item: keep, with an explicit licence line. They also need
  the "no artwork commissioned" caveat, because a print shop assumes a human designer.

## 3. `icons/charta.svg` — AI-generated

Same position as the logo package. `icons/` contains only `charta.svg` and a `CMakeLists.txt`; the
file `mobile/android/icon-playstore.png` is a **symlink to `../../icons/512-apps-okular.png`, which
does not exist** — a broken artefact, recorded in `00-baseline.md` section 0.3.

## 4. `autotests/data/` — 37 fixtures: **Okular's test corpus**

Provenance confirmed by the maintainer (2026-10-21) and consistent with the tree. The corpus stays:
it is GPL-2.0-or-later, it is the input to the test suite, and deleting it would remove test
coverage rather than fix a licence problem. The remaining question is content, not licensing: the
private-key check in P6-06 (`fake_okular_certstore/`, `*_with_signature.pdf`) is still open and
blocks publishing the corpus.

## 5. Fixture secret check — NOT COMPLETED

The plan requires inspecting `autotests/data/fake_okular_certstore/` (cert9.db, key4.db,
pkcs11.txt) and the signed PDFs for private-key material. A quick string scan of the key database
found the substring "private" 11 times, which is not evidence either way. **This check is still
open** and is on the human task list; it must be finished before any public release.
