# `docs/licensing/` — index

The compliance evidence for Charta. Every document states its **evidence class**: `E0` = a command
was run and the output is saved, `E1` = a file was read, `E2` = compared with a pinned upstream
commit, `E3` = reported and not rechecked.

Plan: `/home/workstation/.opencode/plan/Licensing.md` (phases, gates, decisions).

## Start here

| If you want to know… | Read |
| --- | --- |
| what the licence of this project is | [`LICENSE`](../../LICENSE), `docs/THIRD-PARTY-NOTICES.md` |
| where the code came from | `00-baseline.md`, `okular-base.md`, `drawy-provenance.md`, `poppler-provenance.md` |
| what every file is | `file-classification.csv` (1 935 rows), `census-summary.md` |
| what every dependency obliges us to do | `third-party-inventory.csv` |
| what is still open | `RELEASE-CHECKLIST.md` (OPEN rows), `translations.md` §2, `agpl-ghostscript.md` §5 |
| what changed and why | `README.md` (Attribution and provenance, License) |

## The documents

| File | Phase | What it establishes |
| --- | --- | --- |
| `00-baseline.md` | 0 | the audited tree, the toolchain, the concurrency condition, the broken symlink |
| `file-classification.csv` | 1 | one row per tracked file: category, size, SPDX state, origin |
| `census-summary.md` | 1 | the counts, the rules, the anomalies, the guesses |
| `spdx-matrix.csv` | 2 | every identifier/copyright combination in use, and whether a text exists |
| `okular-base.md` | 3 | the pinned upstream commit, the method, its limits |
| `modified-upstream-files.csv` | 3 | per-file ledger: identical / modified / absent upstream |
| `drawy-provenance.md` | 4 | the Drawy snapshot, its licence, the decision, the evidence |
| `poppler-provenance.md` | 5 | the base archive, the fork's real history, the regenerated patch series |
| `third-party-inventory.csv` | 6 | every component: version, licence, linkage, obligation |
| `assets.md` | 6 | provenance of the artwork, the fixtures and the logo package |
| `translations.md` | 6 | the 1 069 catalogues and the open licence question |
| `agpl-ghostscript.md` | 6 | the AGPL chain, the options, the D11 decision, the removal |
| `packaging.md` | 7 | the manifest changes, the identity surface, what was renamed and what was not |
| `trademark.md` | 7 | the KDE trademark surface and the two questions only you can answer |
| `RELEASE-CHECKLIST.md` | 10 | the per-release gate table — re-run for every tag |
| `REAUDIT.md` | 11 | when to re-audit and how |
| `verification.md` | 9 | the build and test result at the audited commit, with the A/B proof of no regression |
| `install-manifest.txt` | 7 | the staged install, the notices that ship, and the collision with a distro Okular |
| `FIRST-PASS-REPORT.md` | — | what this pass did, what it did not, and the task list |
| `legal-open-questions.md` | — | the open legal questions (AI-generated code, the Drawy expression) and what a lawyer must be asked |
| `identity-migration.md` + `.patch` | 7 | the prepared (deferred) rename of the remaining Okular identity, with the migration notes and the footer note |

## Machine-readable helpers

| Script | What it checks |
| --- | --- |
| `check-license-texts.sh` | every SPDX identifier used in the tree has a text in `LICENSES/` |
| `check-poppler-patches.sh` | the patch series applies to the pristine upstream archive |
| `run-tests.sh` | Charta's own tests, with the upstream QPDF suites run and reported **separately** |
| `make-source-archive.sh` | builds the release source archive and checks the archive contract |

Both are wired into `.github/workflows/licensing.yml`.
