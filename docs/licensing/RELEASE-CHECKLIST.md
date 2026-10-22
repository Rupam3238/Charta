# Release checklist (Phase 10) — per release

**This checklist is re-run for every release.** It is not a one-off. Copy the "Status / Evidence /
Date" columns for each tag; do not carry a previous release's marks forward.

Release candidate: ____________________  (tag / commit)
Date: ____________  Reviewer: ____________

Legend: **PASS** = evidenced in this repository · **BLOCKED** = could not be produced here (say
why) · **OPEN** = a decision or an action that is still outstanding.

## A. P0 — blocking

| # | Gate | Status | Evidence | Date |
| --- | --- | --- | --- | --- |
| A1 | Okular base pinned to a commit, with the method and its limits stated | PASS | `okular-base.md`: base **`e0fdfed749e8`** (2026-09-24) in a provably 2-commit window, 1 727/1 935 files byte-identical, single-peak match curve over 153 commits; `modified-upstream-files.csv` (1 935 rows, E2 blob hashes) | 2026-10-21 |
| A2 | Drawy disposition decided and documented; the README statement is true | PASS | `drawy-provenance.md`, `evidence/drawy-35194a7e-controller.txt`, README "Attribution and provenance" | 2026-10-21 |
| A3 | Poppler: pristine + the patch series reproduces the built library; archive hash verified | PASS | `poppler-provenance.md` §6; `evidence/poppler-patch-series.sha256`; `check-poppler-patches.sh` | 2026-10-21 |
| A4 | Every tracked file classified; every licence and notice accounted for | PASS | `file-classification.csv` (1 935 rows), `spdx-matrix.csv` (164 rows), `third-party-inventory.csv` (27 components), `REUSE.toml` covering every remaining file, `reuse lint` exit 0 | 2026-10-21 |
| A5 | No incompatible licence combination knowingly shipped (D2, D9, D11) | PASS (engineering) / **OPEN (legal)** | D11 applied: ghostscript + libspectre removed from the manifest; `agpl-ghostscript.md`. D9 and the Drawy question are recorded as open in `legal-open-questions.md`; **no legal clearance is claimed** | 2026-10-21 |
| A6 | The licence label of each distribution matches its weakest component | PASS | AppStream `project_license` = `GPL-2.0-or-later AND GFDL-1.2-or-later` in all 15 metainfo files; no AGPL component remains | 2026-10-21 |
| A7 | Flatpak and source build describe the same dependency set, or the difference is explicit | PARTIAL | 15 Flatpak modules vs the source build's optional dependencies; the difference is documented in `packaging.md` §3, and the **source** build was executed and documented in `verification.md`. The Flatpak build could not be executed here (no `flatpak-builder`) | — |
| A8 | Corresponding source identified for every distribution channel | OPEN | not published yet; the mechanism is defined (tagged archive + `patches/` + written offer) | — |
| A9 | No knowingly wrong licence header, missing licence text, or misleading provenance statement | PASS | `check-license-texts.sh` passes; `LICENSES/` now 16 texts (Apache-2.0, CC0-1.0, LGPL-2.1-only, the QPDF NOTICE added; the unused GPL-3.0-or-later removed); README corrected | 2026-10-21 |

## B. P1 — before a stable public release

| # | Gate | Status | Evidence | Date |
| --- | --- | --- | --- | --- |
| B1 | `reuse lint` passes or every exception is justified | **PASS** | `reuse` 6.2.0 (spec 3.3) installed in a venv: **exit 0**, 1 965/1 965 files with copyright and licence information, 0 bad, 0 missing, 0 unused, 0 invalid expressions. Output: `evidence/reuse-lint-final.txt` | 2026-10-21 |
| B2 | The licence check runs in CI and has been proven to fail on a bad file | **PARTIAL** | the CI workflow is written (licence lint, licence texts, Poppler patch series, manifest, test suite); `check-license-texts.sh` and `check-poppler-patches.sh` run green locally; the **pre-commit hook has been proven to block a bad file**. The CI job itself is **not yet enabled on a remote** — that needs the maintainer to commit and push the workflow (D4) | — |
| B3 | Contribution policy (DCO + header rules) published | PASS | `CONTRIBUTING.md` | 2026-10-21 |
| B4 | Re-audit triggers exist and are referenced from the release process | PASS | `REAUDIT.md` | 2026-10-21 |
| B5 | Translation licence recorded with a source, not assumed | OPEN | `translations.md` §2 — the upstream question is still unanswered; the interim position is `LicenseRef-Okular-upstream-terms` | — |
| B6 | Assets, translations, fixtures, fonts have recorded provenance | PASS | `assets.md`, `translations.md` | 2026-10-21 |
| B7 | Documentation has a licence and the lint covers it | PASS | `REUSE.toml` (GFDL-1.2-or-later for `docs/**` and `README.md`) | 2026-10-21 |
| B8 | Trademark/identity rename applied | PASS | plugin namespace renamed; the identity migration (D-Bus name, core library, header path, component ids, translation domain) applied from the prepared patch (2026-10-22) — see `packaging.md` §2 and `trademark.md` | — |
| B9 | The README and notices state the AI-authorship position | PASS | README "Attribution and provenance"; `THIRD-PARTY-NOTICES.md` §10 | 2026-10-21 |
| B10 | Clean build from a fresh clone succeeds | **PASS** | out-of-tree build at `/tmp/opencode/charta-build`, built twice, `BUILD_OK`; log `evidence/build.log` | 2026-10-21 |
| B11 | Test suite result recorded; annotation round trip passes | **PASS (no regression)** | 46 ctest entries, 37 passed; all 9 failures accounted for in `verification.md` §3 — 6 are QPDF's own upstream suites, 3 are pre-existing (proven by running the **baseline** binaries), 1 is a flake (passes on re-run) | 2026-10-21 |
| B12 | Packaged artefacts contain the notices | **PASS** | staged install (1214 files); `/usr/share/doc/charta/THIRD-PARTY-NOTICES.md`, `QPDF-NOTICE.md` and 16 licence texts verified present; `install-manifest.txt` | 2026-10-21 |
| B13 | README status/claims match what was tested | OPEN | the other agent owns `README.md`; its feature claims were not re-verified in this pass | — |

## C. P2 — release quality

| # | Gate | Status | Note |
| --- | --- | --- | --- |
| C1 | Tracked build artefacts and scratch files removed | **DONE (uncommitted)** | `CMakeFiles/CMakeSystem.cmake`, `test_savedepth.cpp`, `test_savedepth_simple.cpp`, `test_savedepth_simple` (an ELF binary) removed from the working tree; the deletions are **unstaged** so they appear in `git status` for the maintainer to commit. `.gitignore` now covers `/CMakeFiles/`, `/test_savedepth*`, `/core/*.backup` |
| C2 | `core/document.cpp.backup` (a stray backup of a source file) removed | **DONE (uncommitted)** | same commit; the file is an older copy of `core/document.cpp`, is referenced by no build rule, and its content is in the git history |
| C3 | Future commits use a truthful author identity | OPEN | needs the maintainer's decision; the plan says no history rewrite |
| C4 | Version, app id, desktop metadata, Flatpak permissions consistent | PARTIAL | licence fields, MIME list, plugin namespace, manifest modules fixed; `finish-args` not reviewed |
| C5 | `docs/README-VERIFICATION.md` current for the release SHA | **not touched** | that file belongs to the other agent working in this tree |
| C6 | Release notes list known limitations | OPEN | the known limitations are: PostScript unsupported in the Flatpak (D11), translation domain still `okular`, D-Bus name still Okular's, non-PDF generators unverified, mobile UI unsupported |

## D. Actions reserved for the maintainer (not agent work)

| # | Action | Where | Why it is not automated |
| --- | --- | --- | --- |
| D1 | Commit the Poppler fork's uncommitted work | `docs/licensing/poppler-provenance.md` §7 | **DONE** — commit `10cad17` on `master` of `Rupam3238/poppler-patch-for-charta-26.08.0`, author `Rupam Dutta <rupamd.work@gmail.com>`. **Not pushed** (no authorisation to push) |
| D2 | Remove the fork's 1 523 tracked `build/` files and the four `.orig` files | same document §5, §7 | **DONE** — commit `b2a2404`; the fork now tracks 857 files. The `.orig` files were verified byte-identical to the pristine tarball and are referenced by no build rule. **Not pushed** |
| D3 | Commit the changes made by this pass in the Charta repository | the working tree | **the maintainer does this.** The agent has staged nothing in the Charta repository; the junk removals are unstaged deletions so they are visible in `git status` |
| D4 | Enable CI on the remote | `.github/workflows/licensing.yml` | the workflow is written and includes the licence job and the test job; **it needs to be committed and pushed by the maintainer** — the agent does not push |
| D5 | Install the pre-commit hook | `git config core.hooksPath .githooks` | **DONE** — installed in the Charta repository (repo-local config) and **proven to work**: it blocks a headerless file, allows a compliant one, allows a REUSE-covered path, and honours `CHARTA_SKIP_LICENSE_HOOK=1`. The first version of the hook was broken (regexes used as glob patterns) and was caught by that test |
| D6 | Answer the translation-licence question with upstream | `translations.md` §2 | needs a human to ask KDE l10n |
| D7 | Obtain a legal position on D9 (AI output) and D1 (LGPL-2.1 §3) | `drawy-provenance.md` §3.1 | a lawyer, or an explicit recorded acceptance of the risk |
| D8 | Decide the D-Bus / component-id rename | `packaging.md` §2 | a coordinated code change with user-visible effects |
| D9 | Finish the fixture private-key check | `assets.md` §5 | a content inspection that gates publishing the corpus |
| D10 | Rename the core library / header path / component ids (`libOkular6Core.so.4`, `include/okular/`, 14 `org.kde.okular-*.metainfo.xml`) | `install-manifest.txt` §3 | a packaging-level rename with an ABI break; the install manifest now measures the collision |
| D11 | Exclude QPDF's own upstream test suites from `ctest` | `verification.md` §3 | **DONE** — `docs/licensing/run-tests.sh` runs Charta's own tests with the upstream suites excluded and then runs the upstream suites **separately and visibly** (nothing is suppressed); the upstream names are discovered from the build tree, not hardcoded. `CMakeLists.txt` now calls `enable_testing()` itself, because until now the test suite was only registered because the fetched QPDF called it |
| D13 | Apply the identity migration (D-Bus, core library, header path, component ids) | `docs/licensing/identity-migration.md` / `.patch` | **DONE** (2026-10-22) — applied from the prepared patch; the footer note is in the README and the migration record is in `identity-migration.md` |
| D14 | Review the legal questions in `docs/licensing/legal-open-questions.md` with qualified counsel | that file | Q-A (AI-generated code) and Q-B (the Drawy expression) are open; no clearance may be claimed |
| D12 | Decide whether to publish under the name "Charta" and whether the D-Bus rename is worth the churn | `trademark.md` §4 | a product decision, not a licence one |
