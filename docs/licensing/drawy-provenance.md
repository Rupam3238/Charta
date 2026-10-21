# Drawy provenance and disposition (Phase 4)

**Evidence class:** `E0` (commands run; raw output in
`docs/licensing/evidence/drawy-35194a7e-controller.txt`).
**Decision:** D1 — **retain** the expression, ship the notice, correct the README.
**Date:** 2026-10-21
**Status:** first pass complete. The README paragraph is deferred to Phase 8 (see §6).

## 1. The question

Charta's README claimed that "pressure mapping and seven-point stroke smoothing" came from
[KDE Drawy](https://invent.kde.org/graphics/drawy). The audit found that the *pressure mapping*
matches a specific Drawy snapshot exactly, and that the *smoothing* does not exist in Drawy at all
in that snapshot. This document fixes both facts and records what has to be shipped.

## 2. What the evidence shows

| Claim | Verdict | Evidence |
| --- | --- | --- |
| The pressure expression came from Drawy | **True, with a pinned snapshot** | Drawy `35194a7ec850ee54454917597f34d5b4c62bc81c` (2025-06-07), `src/controller/controller.cpp:45`: `m_context->event().setPressure(event->pressure() / 1.60 + 0.375);` |
| Charta's line is the same expression | **True** | `part/annotationtools.cpp:86`: `*pressure = qBound(0.0, tabletEvent->pressure() / 1.60 + 0.375, 1.0);` — identical constants, bounded with `qBound`, different variable names |
| The seven-point smoothing came from Drawy | **False** | `git grep -i smooth 35194a7e -- 'src/*'` → no matches in the snapshot. The smoothing is in Okular's `SmoothPathEngine`/`SmoothPath` (present in Okular `master` and extended by Charta with a 7-point moving average and pressure windows) |
| The two paths named in Charta's README existed in the snapshot | **True** | `src/controller/controller.cpp` and `src/item/freeform.cpp` both exist at `35194a7e` |
| The project was called "Drawy" then | **False — it was "Drawly"** | Snapshot README title: "# ✏️ Drawly". The rename to Drawy happened later |
| Today's Drawy is the source | **False** | Current HEAD `eb7e925a` has no `LICENSE`; it uses `REUSE.toml` + `LICENSES/` and the README says "GNU General Public License V3". The relicensing commit is `801a5640` ("chore: adopt reuse-tool for linting and add SPDX headers") |

**What is *not* proved:** that the Charta line was copied from this exact snapshot rather than
re-derived from it, or from Drawy's history, or from an AI model's output that reproduced the
expression. The evidence is a *match*, and a match plus the README's own acknowledgement is the
strongest available evidence. No stronger proof exists for text of this size. The maintainer has
since confirmed that the code was AI-generated (§2.13 of the plan), which is why the notice route
(retain + attribute) is the correct choice rather than a "replace" route that would imply a
provenance claim nobody can support.

## 3. Licence of the snapshot

- `LICENSE` at `35194a7e` is the **GNU Lesser General Public License, Version 2.1** text.
- The snapshot README says only: "This project uses the GNU Lesser General Public Licence." There is
  **no "or later" statement** anywhere in the snapshot. The correct SPDX identifier is therefore
  **`LGPL-2.1-only`**, not `LGPL-2.1-or-later`. Shipping the wrong text would itself be a defect,
  so `LICENSES/LGPL-2.1-only.txt` was added (SPDX canonical text,
  sha256 `5749785c8bdefafcb5d798270ed0a967036fe2ca63dcedade1627565dfef81d2`, 176 lines).

### 3.1 How this combines with the rest of Charta

Charta is `GPL-2.0-or-later` per file. LGPL-2.1 §3 allows the licensee to apply the ordinary GPL
instead, to version 2 or any later version. The shipped text says (line 79):

> "3. You may opt to apply the terms of the ordinary GNU General Public License instead of this
> License to a given copy of the Library. To do this, you must alter all the notices that refer to
> this License, so that they refer to the ordinary GNU General Public License, version 2, instead
> of to this License. (If a newer version than version 2 of the ordinary GNU General Public License
> has appeared, then you can specify that version instead if you wish.)"

**Residual doctrinal wrinkle (flagged, not hidden):** §3 speaks of "a given copy of the Library",
and the Drawy snapshot is an application, not a library. The conversion argument is therefore an
*interpretation*, not a certainty. Two independent fallbacks exist, and the plan does not depend on
the conversion being accepted:

1. The expression is a single arithmetic statement with no structure — very likely below the
   threshold of copyright protection, so no licence obligation attaches at all.
2. If it is protected and the conversion is refused, the file can be distributed under
   `LGPL-2.1-only` terms for that expression while the rest of the file stays
   `GPL-2.0-or-later`; but a single file cannot carry two licences, which is why the notice,
   the shipped text, and the "or later" upgrade of LGPL-2.1 are the practical basis.

**This is a point for legal review**, not something to settle in a repository. See §6.

## 4. What was changed in this phase

| Change | File | Why |
| --- | --- | --- |
| Added | `LICENSES/LGPL-2.1-only.txt` | The licence text the notice refers to must be shipped (D1, P4-10) |
| Added | comment above the expression in `part/annotationtools.cpp` | P4-13: point the reader at the notice instead of at Drawy's current master, which no longer contains the expression |
| Not changed | the file's SPDX header | It stays `GPL-2.0-or-later` with Enrico Ros's copyright line. The file is overwhelmingly Okular's; adding an LGPL tag to a GPL file would mislabel Okular's code (P4-12) |

The file header was deliberately **not** rewritten. `part/annotationtools.cpp` is an inherited Okular
file that Charta modified; its header is inherited too, and the correct treatment of an inherited
header is Phase 8's job, with an evidence row per file.

## 5. The notice text that must ship

To be inserted into `docs/THIRD-PARTY-NOTICES.md` in Phase 8, verbatim:

> **KDE Drawy** — <https://invent.kde.org/graphics/drawy>, snapshot
> `35194a7ec850ee54454917597f34d5b4c62bc81c` (2025-06-07), at that time named *Drawly*.
> Licence: **LGPL-2.1-only** (no "or later" grant in that snapshot); the licence text is
> `LICENSES/LGPL-2.1-only.txt`. Used: one pressure-mapping expression
> (`pressure() / 1.60 + 0.375`) in `part/annotationtools.cpp`, derived from
> `src/controller/controller.cpp:45` of that snapshot and bounded with `qBound`. The seven-point
> stroke smoothing in this project is **not** from Drawy; it extends Okular's own
> `SmoothPathEngine`. Today's Drawy is licensed GPL-3.0-or-later and no longer contains this
> expression.

## 6. Human review needed (do not skip)

1. **Confirm the "or later" question is settled as `LGPL-2.1-only`.** If you consider the snapshot's
   silence to be an "or later" grant, the shipped text is right anyway; if you consider it
   `-only`, the text is right. The notice and the file name must agree with the answer you accept.
2. **The §3 conversion argument** (or the de-minimis argument) is a legal position. You have recorded
   that no lawyer is engaged; Phase 10 will log it as an accepted residual risk unless you obtain a
   written opinion.
3. **The README paragraph has not been rewritten yet.** It is deferred to Phase 8 so that the
   concurrent documentation work in `README.md` is not overwritten. The exact replacement text is
   in `docs/licensing/readme-replacements.md` (Phase 8).
4. **The AI-authorship context** (plan §2.13) means the wording must not claim the expression was
   "copied from Drawy" as a statement of fact about human action. The notice above says
   "derived from", which is what the evidence supports.
