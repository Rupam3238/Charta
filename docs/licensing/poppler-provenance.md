# Poppler fork provenance and patch-series reproducibility (Phase 5)

**Evidence class:** `E0` — every command below was run; raw output in
`docs/licensing/evidence/poppler-patch-series.sha256` and this file.
**Date:** 2026-10-21
**Status:** first pass complete, **except** the fork commit, which is prepared but not executed
(see §7) and the Poppler rebuild in a scratch prefix (§8).

## 1. The defect this phase fixes

The Flatpak manifest applied one patch (`patches/poppler-ink-appearance.patch`) that did not apply
to the pristine tarball, and did not apply the patch that introduces the API the Charta generator
calls. Meanwhile the Poppler that the build actually uses came from a **fourth** Poppler directory
whose source changes were never committed. The result was: nothing in the repository could
reproduce the library that ships.

## 2. Base archive (verified)

| Item | Value |
| --- | --- |
| Upstream | <https://poppler.freedesktop.org/poppler-26.08.0.tar.xz> |
| sha256 (local tarball) | `dc906e68cea698109706ac6aa3d2c9d4512fcfcac42d90b8afcda486d1b9abd0` |
| sha256 (`.flatpak-manifest.json`) | `dc906e68cea698109706ac6aa3d2c9d4512fcfcac42d90b8afcda486d1b9abd0` |
| Verdict | **Identical** — the manifest pins the official archive |

The archive was extracted to `/tmp/opencode/poppler-pristine/poppler-26.08.0` and used as the
reference tree for every diff below.

## 3. The four local Poppler trees

| Path | What it is | Verdict |
| --- | --- | --- |
| `/home/workstation/tools/deps/poppler-26.08.0` | git clone, the **source of truth** (uncommitted changes) | **Canonical** |
| `/home/workstation/tools/deps/poppler-26.08.0.tar.xz` | the official archive | **Canonical base** |
| `/home/workstation/tools/deps/poppler-26.08-staging` | a build/install prefix | Build output only, not a source |
| `/home/workstation/tools/deps/poppler-26.08` | a stale install prefix | **Must not be used** (lacks `setUserRotation`) |
| `/home/workstation/tools/deps/build-poppler-stacking` | an extra build tree | Not used by this pass |

**Configuration trap (still live):** `build/CMakeCache.txt` has
`CMAKE_PREFIX_PATH=/home/workstation/tools/deps/poppler-26.08` but
`Poppler_Core_INCLUDE_DIR=/home/workstation/tools/deps/poppler-26.08-staging/include/poppler`.
The build therefore uses a Poppler that `CMAKE_PREFIX_PATH` does not name. Phase 9 must configure
with an explicit, single prefix and record which one was used.

## 4. What the fork's Git history actually contains

| Commit | Message | Real content |
| --- | --- | --- |
| `5799dc0` | "Baseline Poppler 26.08.0 with custom ink appearance support" | root commit: the whole 26.08.0 tree **with the ink-appearance feature already applied** (4 files differ from pristine) |
| `0fe4c9d` | "First pass to add reorder functionality" | **1 523 `build/` files and nothing else** — a CMake build directory committed to the repository. No source change, despite the message |
| working tree | — | 5 modified files, **never committed**: the actual rotation/`setUserRotation` work |

Two consequences:

1. The commit messages are misleading: the "reorder functionality" commit contains no reorder code.
2. `Annotator::setUserRotation()` — the API `generators/poppler/` calls — exists **only** in the
   uncommitted working tree. Verified: `git show 5799dc0:qt6/src/poppler-annotation.h` and
   `git show 0fe4c9d:qt6/src/poppler-annotation.h` contain **zero** occurrences of
   `setUserRotation`; the working tree contains one.

## 5. The delta is 10 files, not 5

The earlier audit counted the 5 *uncommitted* files. The true difference between the official
tarball and the built tree is larger:

| File | pristine | `5799dc0` | `0fe4c9d` | working tree |
| --- | --- | --- | --- | --- |
| `poppler/Annot.cc` | differs | differs | differs | differs |
| `poppler/Annot.h` | differs | differs | differs | differs |
| `poppler/Page.cc` | same | differs | differs | differs |
| `poppler/Page.h` | same | differs | differs | differs |
| `qt6/src/poppler-annotation.cc` | differs | differs | differs | differs |
| `qt6/src/poppler-annotation.h` | differs | differs | differs | differs |
| `qt6/src/poppler-annotation-private.h` | same | differs | differs | differs |
| `qt6/src/poppler-page.cc` | same | differs | differs | differs |
| `qt6/src/poppler-qt6.h` | same | differs | differs | differs |
| `qt6/tests/check_annotations.cpp` | same | differs | differs | differs |

Additionally the working tree contains four **`.orig` files** that are byte-identical to the
pristine versions — residue from manual `patch`-style editing, and they are *tracked* by the fork:

```
poppler/Annot.cc.orig        qt6/src/poppler-annotation.cc.orig
poppler/Annot.h.orig         qt6/src/poppler-annotation.h.orig
```

They are not part of any patch and must not be shipped.

## 6. The regenerated patch series (the deliverable)

Three patches, in order, each applying to the previous state:

| Order | File | Applies to | Introduces | sha256 |
| --- | --- | --- | --- | --- |
| 1 | `patches/poppler-ink-appearance.patch` | pristine tarball | ink appearance support (4 files) | `e5ce470baab1e342…` |
| 2 | `patches/poppler-annotation-rotation.patch` | after patch 1 | rotation / page reorder (9 files) | `451b0addacebcbe4…` |
| 3 | `patches/poppler-annotation-followup.patch` | after patch 2 | the never-committed follow-up, **including `setUserRotation()`** (5 files) | `8f2441136059ff64…` |

**Verification performed (E0):**

```sh
cp -a /tmp/opencode/poppler-pristine/poppler-26.08.0/. /tmp/opencode/verify2/
cd /tmp/opencode/verify2
git apply --check <patch> && git apply <patch>      # for each of the three, in order
# then byte-compare the 10 files against the fork working tree
```

Result: all three applied cleanly in order; **all 10 files byte-identical to the built tree.**
This is the "pristine + patches == built tree" property the plan required, and it did not hold
before this phase.

The two pre-existing patch files were **replaced** (their content no longer described the real
delta, and the ink patch did not apply to pristine). The old rotation patch had an `index` line
(`db09166..d130d53`) that does not correspond to any state in the canonical tree.

## 7. Human action prepared but NOT executed: the fork commit

The maintainer authorised committing the fork's uncommitted work. **This pass did not run it**,
because it is an irreversible write to a foreign repository, it needs the maintainer's identity in
the commit, and a first pass should not mutate shared state. The exact commands:

```sh
cd /home/workstation/tools/deps/poppler-26.08.0
git add poppler/Annot.cc poppler/Annot.h \
        qt6/src/poppler-annotation.cc qt6/src/poppler-annotation.h \
        qt6/src/poppler-annotation-private.h
git commit -m "Charta: annotation rotation support (setUserRotation, rotated appearance)" \
           -m "Adds Annotator::setUserRotation() and the rotated appearance handling that the
Charta Poppler generator calls. Base: poppler-26.08.0 (tarball sha256
dc906e68cea698109706ac6aa3d2c9d4512fcfcac42d90b8afcda486d1b9abd0), matching the patch series in
the Charta repository (patches/poppler-*.patch)." \
           --author="Rupam Dutta <rupamd.work@gmail.com>"
```

Two further fork cleanups are recommended and are **not** part of the commit above, because they
remove tracked files: the four `.orig` files, and the 1 523 tracked `build/` files (the `0fe4c9d`
commit). Removing the `build/` tree from the fork is a separate, reviewable commit.

## 8. Not done in this pass

- **Rebuilding Poppler into a scratch prefix** (`/tmp/opencode/poppler-check`) was skipped: the
  build is CPU-heavy and the patch-equivalence proof above is the property the plan asks for. Phase 9
  should still do it, with `-DCMAKE_PREFIX_PATH` pointed at that prefix only.
- **The Flatpak manifest was not edited in this phase**; it now has to apply all three patches.
  That is Phase 7 (see the manifest change recorded there).
- The fork's `build/CMakeCache.txt` records machine-specific paths and is tracked. It is a
  credential-shaped leak (absolute paths, compiler ids) rather than a licence defect, but it should
  not be in a published source tree.

## 9. Fork-only features (for upstream-drift cost)

| Feature | Where | Upstream status |
| --- | --- | --- |
| `Annotator::setUserRotation(double, double, double)` | `qt6/src/poppler-annotation.h:338` | Not upstream; the reason the fork exists |
| `setAppearanceStream(const QByteArray &, const QRectF &)` | `qt6/src/poppler-annotation.h:940` | Not upstream |
| Page reorder / `Page` API changes | `poppler/Page.{cc,h}`, `qt6/src/poppler-page.cc` | Not upstream |
| Ink appearance support | `poppler/Annot.{cc,h}`, `qt6/src/poppler-annotation.{cc,h}` | Not upstream |
| Annotation test additions | `qt6/tests/check_annotations.cpp` | Not upstream |

Every one of these is a maintenance liability if upstream ever changes the same files, and each
upstream merge will need the series re-based. This is recorded as a re-audit trigger in Phase 11.
