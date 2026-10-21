# Legal open questions

**Status: open. Nothing in this repository establishes legal clearance.**

This file records the legal questions that the engineering work could not answer, what the
project currently does in the meantime, and what a qualified lawyer needs to be asked. It is an
organising document, not legal advice.

**Maintainer position, 2026-10-21:** document the remaining uncertainties; make no clearance
claim; seek qualified legal advice before making any definitive statement.

## 1. The two questions that matter most

### Q-A — The copyright position of AI-generated code

**The facts.** All the implementation in this repository was produced by AI tools, under the
maintainer's direction, and reviewed by the maintainer. The brand assets (`charta_logo_package/`,
`icons/charta.svg`) are AI-generated as well. The code is distributed under `GPL-2.0-or-later`
with per-file SPDX headers naming the project as the licensing party; no human authorship is
claimed for the generated code.

**What is not established.**

* Whether the output of a given AI tool is copyrightable at all, in the jurisdictions a
  distributor would care about. If it is not, then "copyright" in the SPDX headers and in the
  notices describes a legal fiction, and the grant that the distributor actually makes is a
  licence of a work that may have no exclusive rights behind it.
* What the terms of the tools actually used say about output ownership. The maintainer has not
  supplied the tool list or their terms; until that happens the position is "unknown", not
  "fine".
* Whether an **indemnity, warranty or provenance obligation** arises for a distributor that
  cannot show the origin of the code it ships. A GPL distributor normally shows origin by
  shipping the source; for AI-generated code, the origin record is the prompt history and the
  tool's terms, and neither is in the repository.

**What the project does meanwhile.**

* Discloses the AI authorship plainly in `README.md` and `docs/THIRD-PARTY-NOTICES.md` §10, and
  claims no human authorship.
* Uses "licensing party" rather than "author" in the copyright lines it adds
  (`2026 Charta contributors (licensing party; AI-assisted)`).
* Keeps the provenance evidence for the parts that are *not* AI-original (Okular, Drawy, Poppler)
  as hashes and diffs, because that evidence is what the licences actually require.
* Does **not** claim that the output is copyrightable, and does not represent that the position
  has been reviewed by a lawyer.

**What a lawyer must be asked.**

1. Is the output of the specific tools used protectable by copyright in the relevant
   jurisdictions, and if not, what is the effect on the SPDX headers and on the GPL distribution?
2. What do the tools' terms require of a distributor (attribution, disclosure, retention of
   records, indemnity)?
3. Does the "licensing party" formulation hold up, or is an alternative needed?
4. Does the position change for the AI-generated brand assets (registration, trademark
   clearance, and the "no commissioned authorship" statement)?

### Q-B — The Drawy expression: LGPL-2.1-only, and the scope of section 3

**The facts.** One arithmetic expression in `part/annotationtools.cpp`,
`pressure() / 1.60 + 0.375`, is derived from KDE Drawy at snapshot `35194a7e` (2025-06-07), where
the project was named *Drawly* and was licensed **LGPL-2.1-only** (the snapshot contains no
"or later" statement). The expression is carried in a file that is otherwise Okular's
`GPL-2.0-or-later` code, and the file is distributed as `GPL-2.0-or-later`. The licence text is
shipped (`LICENSES/LGPL-2.1-only.txt`), the origin is attributed
(`docs/licensing/drawy-expression-notice.txt`), and the analysis is in
`docs/licensing/drawy-provenance.md`.

**What is not established.**

* **Whether LGPL-2.1 section 3 reaches this material at all.** Section 3 speaks of "a given copy
  of the Library", and the Drawy snapshot is an application, not a library. The conversion
  argument is an *interpretation*.
* **Whether the expression is protected by copyright at all.** It is a single arithmetic
  statement with no structure; if it is below the threshold of protection, no licence obligation
  attaches and the question is moot. That is a judgement, not a fact.
* **Whether the combination is compliant in the way the file claims.** If section 3 does not
  apply, the file's `GPL-2.0-or-later` identifier may not cover the expression, and the
  expression's own `LGPL-2.1-only` terms would have to travel with it (which is why the
  expression was also written out, under its own terms, in
  `docs/licensing/drawy-expression-notice.txt`).

**What the project does meanwhile.**

* Ships the licence text, attributes the origin with the exact snapshot SHA, and states the
  modification.
* Records the expression under its own original terms in a dedicated file, so the LGPL text has
  something to apply to whatever the answer turns out to be.
* Does not state in any document that the combination is "cleared".

**What a lawyer must be asked.**

1. Does LGPL-2.1 section 3 permit the conversion for application code carried into a
   GPL-2.0-or-later work, and what exactly must be altered in the notices for it to be effective?
2. If section 3 does not apply, what is the compliant way to distribute the file?
3. Is the expression protectable at all, and does the answer change the notice obligations?

## 2. The other open questions (not legal advice, but unresolved)

| # | Question | State | Where it is tracked |
| --- | --- | --- | --- |
| C-1 | The licence of the 1 069 translation catalogues | **unresolved**; interim placeholder only, explicitly not a permission | `translations.md`, `LICENSES/LicenseRef-Okular-upstream-terms.txt` |
| C-2 | The terms of the AI tools used | **unknown** — the tool list and their terms have not been supplied | `REAUDIT.md` §2 |
| C-3 | The licence of the `poppler-data` CMap files inside the Flatpak | not re-verified in this pass | `third-party-inventory.csv` |
| C-4 | Whether the AI-generated logo is registrable / clearable as a trademark | not assessed | `trademark.md`, `assets.md` |
| C-5 | KDE trademark guidance for the "Charta" name and the KDE association | to be reviewed before a store listing | `trademark.md` |
| C-6 | The private-key material question in the test fixtures | not completed | `assets.md` §5 |

## 3. What "done" means here

This file is closed when a qualified lawyer has answered Q-A and Q-B, and the answers are written
down here, with the date, the adviser, and what changed in the repository as a result. Until then,
every user-facing document says "known licensing questions are documented and resolved" — never
"licensing is cleared".
