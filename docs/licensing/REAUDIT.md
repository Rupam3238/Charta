# Re-audit triggers and procedure (Phase 11)

**Purpose:** make the compliance state survive the next commit. The clean-up described in
`docs/licensing/` is a snapshot; this file says when to look again.

## 1. Triggers — re-audit when any of these happen

| Trigger | What to re-run | Why |
| --- | --- | --- |
| A merge or rebase from upstream Okular | Phase 3 (base pin + ledger), then Phase 2 for new files | new inherited files bring their own headers, notices and possibly their own licence questions |
| A Poppler version bump or rebase | Phase 5 from scratch: pristine + patches == built tree | the patch series must be re-based; the `Annot.cc`/`Annot.h` context will move |
| A new third-party dependency, vendored snippet, script or asset | Phase 6 row **first**, merge **second** | an unrecorded dependency is an unrecorded obligation |
| A new contributor | DCO present, SPDX headers correct, `REUSE.toml` updated if needed | the licence of the project is only as good as each contributor's grant |
| An upstream licence change (Qt, KF, Poppler, Okular, Drawy) | the affected inventory row and notice entry | the obligation changes without any code change |
| A new distribution channel (store, package manager, binary tarball) | Phase 7 install manifest, Phase 10 source offer | each channel needs its own corresponding-source offer |
| A change to the AI tooling used for contributions | the D9 position and the README statement | the disclosure must stay accurate |
| **Every release** | the whole of Phase 9 and Phase 10 | the checklist is per-release, not one-off |
| **Annually**, even if nothing changed | `reuse lint`, the inventory diff, the notices | upstream moves on its own |

## 2. Procedure for a routine re-audit

```sh
# 1. state
git rev-parse HEAD; git status --porcelain=v1
sha256sum -c docs/licensing/evidence/baseline.sha256 | grep -v ': OK' | head

# 2. licences
reuse lint
./docs/licensing/check-license-texts.sh
./docs/licensing/check-poppler-patches.sh

# 3. manifest
python3 -c "import json;d=json.load(open('.flatpak-manifest.json'));print(len(d['modules']))"

# 4. diff the inventory against reality
#    compare docs/licensing/third-party-inventory.csv with the manifest module list
```

Then: update the affected rows, re-run `reuse lint`, and append a row to the progress log.

## 3. What must never silently happen

- A new dependency enters the build without an inventory row.
- A licence text is deleted while its identifier is still used.
- A patch file is edited without re-verifying that the series still applies.
- The release checklist is copied forward from a previous release instead of being re-run.
- An AI-generated contribution is merged without the commit-message disclosure.

## 4. Enforcement that already exists in the tree

| Mechanism | Where | What it catches |
| --- | --- | --- |
| CI licence job | `.github/workflows/licensing.yml` | missing SPDX headers, missing licence texts, a patch series that no longer applies, a manifest that references a missing file |
| Pre-commit hook | `.githooks/pre-commit` | a staged source file with no `SPDX-License-Identifier` (install: `git config core.hooksPath .githooks`) |
| Contribution rules | `CONTRIBUTING.md` | inbound = outbound, DCO, third-party code needs an inventory row, AI disclosure |

**Not yet enabled:** the CI workflow is written but the repository has no remote CI configured, and
the hook is not installed (`git config core.hooksPath .githooks` has not been run). Both are
one-command steps for the maintainer.
