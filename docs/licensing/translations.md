# Translations (Phase 6)

**Evidence class:** `E0` (counts and headers read from the tree), `E1` (upstream comparison).
**Date:** 2026-10-21

## 1. What is in the tree

| Fact | Value |
| --- | --- |
| Catalogue files | **1069** (one per language under `po/<lang>/okular.po`) |
| Languages | 83 |
| Still mentioning "Okular" in content | 947 files |
| Carrying `SPDX-FileCopyrightText` | 265 files |
| Carrying `SPDX-License-Identifier` | **0 files** |
| `Project-Id-Version` | `"Project-Id-Version: okular\n"` |
| `POT-Creation-Date` (de) | `"POT-Creation-Date: 2026-09-22 15:58+0000\n"` |

## 2. The licence question — still open, and it is upstream's gap

The catalogues carry **no licence identifier at all**, and only **265 of the 1 069** carry a
translator copyright line — 804 carry neither an identifier nor a copyright line. (This corrects an
earlier statement in the plan that said all 1 069 carried a copyright line; the measurement is
`git grep -l 'SPDX-FileCopyrightText' -- po | wc -l` = 265.) This is not a Charta
defect: upstream Okular has the same state.

- `https://raw.githubusercontent.com/KDE/okular/master/po/de/okular.po` contains **0**
  `SPDX-License-Identifier` lines.
- Okular has **no `REUSE.toml` and no `.reuse/dep5`** (both 404 on the GitHub mirror, and the
  GitLab API tree listing of the repository root contains no `reuse` entry). Its root has only
  `LICENSES/` (the same 12 texts Charta inherits) and per-file headers.
- Therefore there is **no machine-readable grant** for the translation files, in either project.

What the plan will **not** do: assert `LGPL-2.0-or-later` (the first revision of this plan did
exactly that, and it was wrong). What it will do:

1. Ask the Okular maintainers / KDE l10n whether the repository's default terms
   (`GPL-2.0-or-later`) cover `po/`, and record the answer with its source.
2. If no answer is available, record the grant as **unresolved** and ship the translator copyright
   lines unchanged, with the reuse documented.

Until (1) or (2) resolves, `po/**` gets **no licence identifier** beyond the placeholder
`LicenseRef-Okular-upstream-terms` — an annotation plus a note, rather than a fabricated grant.
This is deliberately a human-action item, not an agent decision.

**Maintainer decision, 2026-10-21:** the placeholder is accepted as an **interim classification
only**. The licensing question stays open, and this document, the notices and the placeholder
itself must not be read as evidence that the upstream copyright holders have permitted
redistribution on these terms. Until KDE clarifies the redistribution terms, the honest statement is
"we do not know what licence covers these files".

## 3. The domain question (D3)

The catalogues are installed with `ki18n_install(po)` (`CMakeLists.txt:745`) and the application
domain is `setApplicationDomain("okular")` (`shell/main.cpp:98`). Consequences:

- Charta's **new** strings are untranslated until the domain changes.
- The installed catalogue metadata still says "Okular" — a product-identity issue as much as a
  licence one, and it is D3/D4 territory, not a licence defect.

## 4. Snapshot drift (recorded, not a defect)

The local catalogues are an **older sync** than upstream master: local `POT-Creation-Date`
2026-09-22 vs upstream 2026-10-06. Hash equality with upstream therefore cannot be used as
evidence, and this plan does not claim it. The header metadata (project id, translator copyright
lines, catalogue completeness) is the usable evidence.
