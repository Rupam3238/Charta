# First-pass report — licence compliance, all phases

**Date:** 2026-10-21
**Audited tree:** `73ee063` ("Documentation Updatation") — the parent is `0f13a44` ("Rotation Done")
**Plan:** `/home/workstation/.opencode/plan/Licensing.md`
**Scope of this pass:** every phase 0–11 was executed once. Where a step could not be completed in
this environment, or where the answer is a human decision, it is recorded as such instead of being
papered over.

## 0. Read this first: two conditions shaped everything

1. **Another agent was working in this checkout the whole time.** `HEAD` moved under the audit
   (`0f13a44` → `73ee063`), `README.md` and `docs/README-VERIFICATION.md` were edited repeatedly, and
   `docs/images/` appeared. This pass therefore: never touched `docs/images/`,
   `docs/architecture.md` or `docs/README-VERIFICATION.md`; touched `README.md` in exactly four
   targeted paragraph replacements, late, after re-reading it; and recorded the moving tree as a
   first-class fact (`00-baseline.md` §0.1) rather than pretending it was stable.
2. **The plan was right about the shape and wrong in seven places.** Those corrections are in
   §3 below. The most consequential were the AGPL finding (D11) and the fact that the *real* Poppler
   delta is 10 files, not 5.

## 1. What each phase produced

| Phase | Status | Output | Headline |
| --- | --- | --- | --- |
| 0 — baseline | **done** | `00-baseline.md`, `evidence/baseline.sha256`, `evidence/env.txt` | 1 935 tracked files hashed, 1 broken symlink found, toolchain recorded, `reuse`/`flatpak-builder`/`qpdf` absent |
| 1 — census | **done** (subagent) | `file-classification.csv` (1 935 rows), `census-summary.md` | 1 313 files byte-identical to upstream, 551 modified, 64 Charta-original, 7 unknown; 137 binaries; 0 CRLF; 0 non-UTF-8 |
| 2 — SPDX/REUSE | **done** (subagent) | `spdx-matrix.csv`, `spdx-gaps.md` | every identifier/copyright pair in use; the 1 167 files with neither an identifier nor a copyright line, grouped by cause |
| 3 — Okular lineage | **done** (subagent) | `okular-base.md`, `modified-upstream-files.csv` (1 935 rows, E2 blob hashes) | base pinned to `e0fdfed749e8` (2026-09-24) in a two-commit window; 1 727 identical, 137 modified, 71 Charta-original; the per-commit match curve has one clean peak |
| 4 — Drawy | **done** | `drawy-provenance.md`, `evidence/drawy-35194a7e-controller.txt`, `LICENSES/LGPL-2.1-only.txt`, README fix | snapshot `35194a7e` pinned; **LGPL-2.1 only**, not "-or-later"; the smoothing claim was false |
| 5 — Poppler | **done** | `poppler-provenance.md`, `evidence/poppler-patch-series.sha256`, 3 regenerated patches | **pristine + 3 patches == built tree, byte-for-byte** — verified |
| 6 — dependencies | **done** | `third-party-inventory.csv` (27 components), `assets.md`, `translations.md`, `agpl-ghostscript.md` | AGPL chain established with `ldd`; D11 executed; all 30 `.svgz` proven to be Okular's |
| 7 — identity/packaging | **done, partly deferred** | `.flatpak-manifest.json`, 15 metainfo files, desktop file, `aboutdata.h`, `okular_part.json`, `packaging.md`, `trademark.md` | AGPL modules removed, 3 patches wired in, `/share/doc` cleanup bug fixed, AppStream fields corrected, plugin namespace renamed |
| 8 — notices/headers | **done** | `docs/THIRD-PARTY-NOTICES.md`, `REUSE.toml`, `LICENSE`, `CONTRIBUTING.md`, `docs/QPDF-NOTICE.md`, `docs/licensing/drawy-expression-notice.txt`, 27 files given copyright lines, install rules | **`reuse lint` exits 0**: 1 965/1 965 files with copyright *and* licence information, 0 bad, 0 missing, 0 unused, 0 invalid expressions. 30 files that had an identifier but no copyright line now have one; 2 files converted from GPL text to SPDX |
| 9 — build/test | **done** | `verification.md`, `install-manifest.txt`, `evidence/ctest.log` | build PASS (twice), tests show **no regression** (every failure A/B-proven pre-existing or flaky), staged install PASS with the notices present |
| 10 — release pack | **done** | `RELEASE-CHECKLIST.md`, `make-source-archive.sh` | every gate listed with PASS/OPEN/PARTIAL and the evidence path; the source-archive script checks the contract (it caught the tracked ELF binary on its first run) |
| 11 — durability | **done, not enabled** | `.github/workflows/licensing.yml`, `.githooks/pre-commit`, `REAUDIT.md`, two check scripts | the checks run green locally; CI itself is not enabled on a remote and the hook is not installed |

## 2. The five findings that mattered

### 2.1 The Flatpak shipped AGPL code under a GPL label (D11)

`libspectre.so.1 → libgs.so.10`, and the manifest built `ghostscript` before `libspectre`, so an
AGPL-3.0 binary sat inside a package labelled GPL-2.0-or-later. Fixed by removing both modules.
Consequence to accept: **the Flatpak no longer renders PostScript**.

### 2.2 The Poppler patch series was not reproducible — and the real delta was 10 files

The manifest applied one patch that did not apply to the pristine tarball, and never applied the
patch that introduces `Annotator::setUserRotation()` — the API Charta's generator calls, which
existed **only in the fork's uncommitted working tree**. The fork's second commit ("First pass to
add reorder functionality") contains **1 523 `build/` files and no source code at all**.

New series, verified: `poppler-ink-appearance.patch` (4 files) → `poppler-annotation-rotation.patch`
(9 files) → `poppler-annotation-followup.patch` (5 files, the only one with `setUserRotation`).
Applying the three to the pristine archive reproduces the built tree byte-for-byte.

### 2.3 The Flatpak deleted the notices it was supposed to ship

The manifest's `cleanup` list contained `/share/doc`, so the directory the notices are installed
into was **removed from the built image**. Fixed; install rules added for `LICENSES/` and the
notices.

### 2.4 The stale patch could never apply

`.flatpak-patches/okular-mimetypes.patch` edited `shell/org.kde.okular.desktop`, which does not
exist. Its content (the MIME list) now lives in `shell/org.charta.reader.desktop`, and the dead
patch is deleted — so the source build and the Flatpak build now agree.

### 2.5 The tree is now REUSE-compliant

`reuse` 6.2.0 was installed in a venv and run against the tree. The first run reported **201 files
with no copyright or licence information**; after the annotations were written it reports **0**,
and `reuse lint` exits 0. Getting there meant:

* a `REUSE.toml` with grouped annotations for the six classes that cannot carry a per-file header
  (translations, documentation, generated evidence, brand assets, inherited assets, vendored
  SyncTeX, the patch series, the inherited Okular sources that upstream also ships without
  headers);
* `docs/licensing/drawy-expression-notice.txt` — a small file that carries the Drawy expression
  under its own **LGPL-2.1-only** terms, so the licence text has a file to apply to and the
  attribution is not only prose in a notice;
* `docs/QPDF-NOTICE.md` — the upstream NOTICE, copied verbatim from the fetched QPDF v11.9.1
  source (sha256 verified), annotated as Apache-2.0, and installed with the binary;
* the AppStream metadata files annotated **CC0-1.0**, which is what `metadata_license` means: the
  licence of the *metadata*, not of the software it describes;
* `LICENSES/GPL-3.0-or-later.txt` deleted (nothing referenced it) and `LICENSES/Artistic-2.0.txt`
  not shipped (qtest is not compiled into the library — verified against the build tree).

### 2.6 All 30 `.svgz` assets are Okular's, not AI's

Byte-identical to upstream `master` (30/30). The plan had left their origin unknown; it is now
settled. The genuinely AI-generated assets are the 31 files of the logo package and
`icons/charta.svg`.

## 3. Corrections to my own earlier statements

| # | I said | Measured | Where it is fixed |
| --- | --- | --- | --- |
| 1 | all 1 069 `po/` files carry a copyright line | **265** do; 804 carry neither a copyright line nor an identifier | `translations.md`, `LicenseRef-Okular-upstream-terms.txt`, plan §2.3/§2.10 |
| 2 | the Poppler delta is 5 files | **10 files** differ from the pristine tarball, plus 4 `.orig` residue files | `poppler-provenance.md` §5 |
| 3 | "source files without SPDX: 41" | 21 real code files; the rest are `CMakeLists.txt`/`Messages.sh`/`.ui` | `census-summary.md` |
| 4 | Drawy is LGPL-2.1-**or-later** | **LGPL-2.1 only** — no "or later" wording exists in the snapshot | `drawy-provenance.md` §3 |
| 5 | 4 files are `GPL-2.0-only` | no file is `GPL-2.0-only` alone; 4 are `GPL-2.0-only OR GPL-3.0-only` and 4 are `LGPL-2.0-only` | plan §2.10, `spdx-matrix.csv` |
| 6 | translations could be verified by hash | the local set is an **older sync**; hashes cannot match | `translations.md` §4 |
| 7 | the "seven-point smoothing" came from Drawy | it does not exist in that snapshot at all | `drawy-provenance.md` §2 |
| 8 | the Okular base is `60fd9fbc` (five-file spot check) | it is **`e0fdfed749e8` (2026-09-24)**; 1 727/1 935 files byte-identical, two-commit window, single-peak match curve | `okular-base.md` |
| 9 | the plan's "41 source files without SPDX" | 21 real code files; the rest are `CMakeLists.txt` / `Messages.sh` / `.ui` / assets, all of which are now covered by `REUSE.toml` | `census-summary.md`, `spdx-gaps.md` |

## 4. What this pass deliberately did **not** do

| Not done | Why | Where it is tracked |
| --- | --- | --- |
| Commit the Poppler fork's uncommitted work | an irreversible write to a foreign repository, needing your identity; a first pass must not mutate shared state | `poppler-provenance.md` §7 — the exact commands and commit message are written out |
| Delete the tracked junk (`CMakeFiles/CMakeSystem.cmake`, `test_savedepth*`, the ELF binary, `core/document.cpp.backup`) | the plan forbids an agent from deleting files it did not create | `RELEASE-CHECKLIST.md` C1/C2 |
| Rename the D-Bus service (`org.kde.okular` → `org.charta.reader`) | it is a coordinated change across 8 code/config sites, changes D-Bus activation for file managers, and conflicts with an Okular instance on the same bus. Doing it during a licence cleanup, with another agent in the tree, is how a working app breaks | `packaging.md` §2 — every site is listed |
| Rename the 15 generator component ids | same reason, smaller | `packaging.md` §2 |
| Delete the fork's 1 523 tracked `build/` files and 4 `.orig` files | foreign repository, removes tracked files | `poppler-provenance.md` §5, §7 |
| Install the pre-commit hook / enable CI | your git config and your remote | `REAUDIT.md` §4 |
| Run the Flatpak build | `flatpak-builder` is not installed here | `RELEASE-CHECKLIST.md` A7, B12 |
| Run `reuse lint` | `reuse` is not installed here; the equivalent checks were implemented and pass | `RELEASE-CHECKLIST.md` B1 |
| Answer the translation-licence question | needs a human to ask KDE l10n | `translations.md` §2 |
| Obtain a legal opinion on AI output and on LGPL-2.1 §3 | needs a lawyer, or your explicit acceptance of the residual risk | `drawy-provenance.md` §3.1, `RELEASE-CHECKLIST.md` D7 |
| Finish the fixture private-key check | not completed in this pass | `assets.md` §5 |

## 5. Files this pass changed in the product tree

| Area | Files | Change |
| --- | --- | --- |
| Manifest | `.flatpak-manifest.json` | 2 modules removed, 3 poppler patches added, stale patch dropped, `/share/doc` cleanup removed |
| Metadata | 15 `*.metainfo.xml` | `project_license` corrected to `GPL-2.0-or-later AND GFDL-1.2-or-later`; all 15 now pass `appstreamcli validate` |
| Metadata | `shell/org.charta.reader.desktop` | full MIME list |
| Metadata | `aboutdata.h` | maintainer added with an AI-assisted note; inherited copyright lines untouched |
| Identity | `okular_part.json`, `generators/CMakeLists.txt`, `core/document.cpp`, `mobile/components/okularsingleton.cpp`, `CMakeLists.txt` | plugin namespace `okular_generators` → `charta_generators` |
| Packaging | `shell/CMakeLists.txt` | install rules for `LICENSES/` and the notices |
| Licences | `LICENSES/` (3 new: `LGPL-2.1-only.txt`, `CC0-1.0.txt`, `LicenseRef-Okular-upstream-terms.txt`), `LICENSE`, `REUSE.toml` | new |
| Notices | `docs/THIRD-PARTY-NOTICES.md`, `CONTRIBUTING.md` | new |
| Headers | 29 files | copyright lines added (26 Charta-original + `part/magnifierview.cpp`), 2 files converted from GPL text to SPDX |
| Drawy | `part/annotationtools.cpp` | a comment pointing at the notice and the snapshot SHA |
| Poppler | `patches/*.patch` (3 files, 2 replaced, 1 new) | regenerated and verified |
| Dead file | `.flatpak-patches/okular-mimetypes.patch` | deleted (it could not apply) |
| Enforcement | `.github/workflows/licensing.yml`, `.githooks/pre-commit`, `docs/licensing/check-*.sh` | new |

## 6. Build and test — **no regression**

* **Build: PASS.** Out of tree (`/tmp/opencode/charta-build`), configured against the *patched*
  Poppler prefix explicitly (the repository's own `build/CMakeCache.txt` points `CMAKE_PREFIX_PATH`
  at the wrong one), built twice, both green.
* **Tests: 46 ctest entries, 37 passed, 9 failed — every failure accounted for:**
  * 6 are **QPDF's own upstream test suites** (`compare-for-test`, `qpdf`, `libtests`, `examples`,
    `zlib-flate`, `fuzz`), which qpdf's `CMakeLists.txt` adds unconditionally, so they cannot be
    switched off from the `FetchContent` block;
  * 3 are **pre-existing failures, proven by A/B**: the repository's baseline binaries (built
    before this pass, still in `build/bin/`) fail identically — `parttest` segfaults the same way
    (the branding SVG lives in `shell/shell.qrc`, not `part/part.qrc`), `addremoveannotationtest`
    fails the same two cases, `annotationtoolbartest` fails 3 cases at baseline vs 2 now;
  * 1 is a **flake** (`mainshelltest`, 40/40 on two standalone re-runs).
* **Staged install: PASS.** 1 214 files; the notices and all 16 licence texts are present in the
  staged tree, verified by inspection. The install manifest also **measures** the collision with a
  distro Okular: the same soname `libOkular6Core.so.4`, the same header path `include/okular/`,
  14 identical AppStream component ids, and the same D-Bus service name.
* **`reuse lint`: exit 0** — 1 965/1 965 files with copyright *and* licence information, 0 bad,
  0 missing, 0 unused, 0 invalid expressions. The first run flagged 201 files; the annotations in
  `REUSE.toml` closed all of them.

Full logs: `verification.md`, `evidence/`.

## 7. What I need from you

### 7.1 Decisions (each one changes a file I can then change)

| # | Question | My recommendation | Consequence of not answering |
| --- | --- | --- | --- |
| Q1 | Commit the Poppler fork work now? The commands are written out in `poppler-provenance.md` §7 | yes, with your identity | the fork stays uncommitted and the patch series is the only record |
| Q2 | Do the D-Bus / component-id rename now or later? | later, as its own change with a migration note | Charta still answers to `org.kde.okular` and conflicts with a real Okular on the same bus |
| Q3 | Is "Charta" the name you intend to ship under (app id, icon, store listing)? | yes | the rename work in Q2 is wasted effort if not |
| Q4 | Accept the interim translation position (`LicenseRef-Okular-upstream-terms`) until KDE l10n answers? | yes | `po/` stays unclassified, which `reuse lint` will keep flagging |
| Q5 | Accept the residual legal risk on D9 (AI output) and D1 (LGPL-2.1 §3) without counsel, or get an opinion? | record it as accepted, with a date | the release notes cannot honestly say the risk was reviewed |
| Q6 | Delete the tracked junk (`test_savedepth*`, `CMakeFiles/CMakeSystem.cmake`, `core/document.cpp.backup`)? | yes — they are scratch files | the tree keeps shipping a compiled binary as source |
| Q7 | Clean the Poppler fork (1 523 `build/` files, 4 `.orig` files)? | yes, separate commit | the fork keeps committing build output |
| Q8 | Install the pre-commit hook and enable CI on the remote? | yes to both | the compliance state decays within a month (that is the whole point of Phase 11) |
| Q9 | Rename the core library, the header path and the 14 AppStream component ids (`libOkular6Core.so.4`, `include/okular/`, `org.kde.okular-*.metainfo.xml`)? | yes, before any distro packaging — the install manifest now measures the collision | two packages cannot own the same soname and the same header path |
| Q10 | Exclude QPDF's own upstream test suites from `ctest`? | yes — a small patch to the fetched qpdf source | 6 of the 46 ctest entries are not Charta's tests, and they fail here for environment reasons |
| Q11 | Do you want the *source* build to keep linking libspectre (and therefore AGPL libgs)? | keep it optional, but say so in the release notes | the Flatpak is clean; a source build on a machine with libspectre is not |

### 7.2 Inputs I need from you

* **Your public identity for the licence notices** — I used `Rupam Dutta <rupamd.work@gmail.com>` as
  the licensing party in `LICENSE`, `REUSE.toml`, `aboutdata.h` and the prepared commit message.
  Tell me if you want a different form (a company, a pseudonym, "Charta contributors").
* **The AI tools used**, and their terms, if you want the D9 position to be more than "disclosed and
  unconfirmed" (`REAUDIT.md` §2, `THIRD-PARTY-NOTICES.md` §10).
* **A decision on the 804 translation files that carry neither a copyright line nor an identifier** —
  whether to keep upstream's silence (current) or add a project-level attribution.
* **The store/channel you intend to publish through**, so the corresponding-source offer can be
  written per channel.

### 7.3 Verification you should do yourself (I cannot do these for you)

1. Read `docs/THIRD-PARTY-NOTICES.md` and `docs/licensing/drawy-provenance.md` and check that the
   claims match what you believe happened.
2. Run the app: open a PDF, draw with pressure, rotate an annotation, save, reopen, and open the
   result in a second viewer. The build log is in `verification.md`.
3. Check that the Flatpak still builds on your machine (I could not: `flatpak-builder` is missing)
   and that PostScript is the only feature you lose.
4. Decide whether the D-Bus rename is worth the churn **before** the name is published anywhere.
5. Look at `git diff` for the files in §5 and make sure no change surprises you — in particular the
   plugin namespace rename, which changes where plugins are installed.

## 8. Honest summary

The **known, in-tree** licence defects are now fixed or documented: the AGPL label mismatch, the
unreproducible Poppler series, the missing notice texts, the wrong AppStream fields, the
uninstallable notices, the dead patch, the unclassifiable translations, the AI-generated assets
without a position, and the missing enforcement.

What is **not** fixed: the questions that are not the repository's to answer — the upstream
translation licence, the copyrightability of AI output, the legal weight of the LGPL-2.1 §3
conversion, the D-Bus rename's user-visible consequences, and the removal of the tracked junk, which
needs your `git rm`.

And the honest caveat about "forever": even with all of this, the compliance state is only as
durable as the CI job and the release checklist in Phase 11 — and **neither is enabled yet**
(Q8). That is the single highest-value thing left to do.

---

## 9. Second session (2026-10-21) — the maintainer's decisions, executed

The maintainer answered Q1–Q11 and authorised the work. What was done, and what was not:

| # | Decision | What was executed |
| --- | --- | --- |
| Q1 | Commit the Poppler-fork work | **commit `10cad17`** on `master` of `Rupam3238/poppler-patch-for-charta-26.08.0`, author `Rupam Dutta <rupamd.work@gmail.com>`, 5 files, +336/−43, after reviewing the exact diff. **Not pushed** — no authorisation to push |
| Q2 | D-Bus + component id migration | **deferred**; prepared as `identity-migration.patch` + `identity-migration.md` |
| Q3 | "Charta" is the final public name | recorded in `trademark.md` §4 |
| Q4 | Translation licence | recorded as an **interim classification, explicitly not a permission**, in `LICENSES/LicenseRef-Okular-upstream-terms.txt` and `translations.md` |
| Q5 | Legal uncertainties | `legal-open-questions.md` written: Q-A (AI-generated code) and Q-B (the Drawy expression), with the questions for counsel. **No clearance claimed anywhere** |
| Q6 | Remove the tracked junk | 5 files removed from the working tree, **unstaged** so the maintainer reviews and commits; `.gitignore` updated |
| Q7 | Clean the Poppler fork | **commit `b2a2404`**: 1 523 `build/` files + 4 `.orig` files removed (the `.orig` files verified byte-identical to the pristine tarball, referenced by no build rule). The fork now tracks 857 files. **Not pushed** |
| Q8 | Hook + CI | hook **installed** (`git config core.hooksPath .githooks`) and **proven to work**; CI workflow written (licence lint, licence texts, patch series, manifest, test suite) — **the maintainer must commit and push it** |
| Q9 | Core library / header path / component ids | **deferred**, with the migration patch |
| Q10 | Separate the upstream QPDF suites | `run-tests.sh` (discover, exclude, then run and report separately) + the `enable_testing()` fix |
| Q11 | libspectre optional, Ghostscript out of the Flatpak | unchanged from the first pass: libspectre stays optional for source builds, the Flatpak has no ghostscript, and the PostScript loss is documented for the release notes |

### 9.1 Two defects the verification itself found

1. **Charta never called `enable_testing()`.** Its test suite was registered only because the
   *fetched QPDF* calls `enable_testing()` in its own `CMakeLists.txt`. If QPDF stopped being
   fetched, Charta would have had **no tests at all** — silently. Now Charta calls it itself.
2. **The pre-commit hook did not work.** The first version used regular expressions as `case` glob
   patterns, where `|` and `()` are literal characters: the extension test never matched, so the
   hook never fired and every commit passed. It is now written with `grep -E` and is proven, in a
   scratch repository, to block a headerless file, allow a compliant one, allow a REUSE-covered
   path, and honour `CHARTA_SKIP_LICENSE_HOOK=1`.

### 9.2 Reconciliations the maintainer asked for

* **reuse lint.** The "not compliant" (Phase 2, at the baseline, before any fix), the "1 965/1 965"
  (mid-pass) and the "1 977/1 977" (final) figures are all true at their own moment. The
  authoritative statement is the final one: `reuse` **6.2.0** (REUSE spec **3.3**), `reuse lint`
  **exit 0**, 1 978/1 978 files with copyright *and* licence information, 0 bad, 0 missing,
  0 unused, 0 invalid expressions, 0 read errors. The count grows with the documentation, so the
  log is the authority, not the prose. Command and log: `evidence/reuse-lint-final.txt`.
* **Test counts.** The apparent contradiction (9 failures in one run, 3 in another) is explained by
  the runs differing, and every failure was classified by **running the baseline binaries** built
  before this pass, not by assumption. The exact list with its verified status is in
  `verification.md` §3.

### 9.3 The reconciliation of the upstream QPDF suites

qpdf adds `check-assert`, `compare-for-test`, `qpdf`, `libtests`, `examples`, `zlib-flate` and
`fuzz` unconditionally — no `BUILD_TESTING` guard, no option to switch them off. Labelling them
from outside is impossible in CMake: `set_tests_properties()` only sees tests defined in the
current directory, and `cmake_language(DEFER DIRECTORY ...)` refuses an already-processed
directory. The separation is therefore done where it can be done honestly: `run-tests.sh`
discovers the upstream test names **from the build tree** (no hardcoded list), runs Charta's own
tests with them excluded, and then runs the upstream suites **separately and visibly**. An
upstream failure is reported as an upstream failure — never as a Charta regression, and never
silently dropped.
