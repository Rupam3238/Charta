# 00 — Baseline and evidence harness (Phase 0)

**Evidence class:** `E0` for everything produced here; commands are listed at the end.
**Audit base:** `73ee06351c30501c9aa990c7d976f8e604ef0c06` (`Docuementation Updatation`), parent `0f13a44dd83e15e17e1d6a7c0871e70ab3642f51`.
**Repository:** `/home/workstation/tools/project/Charta`
**Date:** 2026-10-21
**Status of this phase:** first pass, complete. Nothing outside `docs/licensing/` was written.

## 0.1 Read this before trusting the baseline

A second agent was working in this same checkout while this baseline was taken (screenshots and
documentation). During Phase 0:

- `HEAD` moved from `0f13a44` ("Rotation Done") to `73ee06351c30501c9aa990c7d976f8e604ef0c06` ("Documentation Updatation"), which
  changed `.gitignore`, `README.md`, `docs/README-VERIFICATION.md` and `docs/architecture.md`;
- `README.md` and `docs/README-VERIFICATION.md` were modified again *after* that commit;
- `docs/images/` appeared as untracked content.

Consequences, stated plainly because they affect every later phase:

1. `evidence/baseline.sha256` is a **moment-in-time snapshot** of the tracked files, not a stable
   invariant. It can detect drift, but a mismatch is not automatically a defect.
2. No hash comparison involving `README.md`, `docs/architecture.md`,
   `docs/README-VERIFICATION.md`, `.gitignore`, or anything under `docs/images/` is meaningful.
3. This pass **did not modify** any of those paths, and no phase may revert, reformat or "tidy" them.
   They belong to the other agent until it stops.
4. The plan's original audit SHA (`0f13a44`) is now the **parent** of the audit base. The content
   difference is documentation-only, so the licence findings in the plan still hold, but every
   citation to `0f13a44` must be read as "content equivalent at `73ee06351c30501c9aa990c7d976f8e604ef0c06`".

## 0.2 Tree state at T0

```
HEAD 73ee06351c30501c9aa990c7d976f8e604ef0c06
tracked files        1935
non-merge commits    90
working tree (porcelain, verbatim)
M README.md
 M docs/README-VERIFICATION.md
?? docs/images/
?? docs/licensing/
```

`docs/licensing/` is this pass's output directory and is untracked on purpose.

## 0.3 Hash baseline and its anomalies

`evidence/baseline.sha256` — one line per tracked file, SHA-256 for regular files, a marker for the
rest. Result: **1 934 hashed, 1 symlink, 0 plain-missing**.

Anomaly found while building it (not a licence issue, but a packaging one):

```
symlink BROKEN mobile/android/icon-playstore.png -> ../../icons/512-apps-okular.png
```

`mobile/android/icon-playstore.png` is a symlink to `../../icons/512-apps-okular.png`, which does
not exist (`icons/` contains only `CMakeLists.txt` and `charta.svg`). A naive `sha256sum` fails on
it; more importantly, the Android artefact in the tree is broken. Recorded for Phase 7/8; not fixed
here because it is outside the licence scope and the mobile UI is explicitly unsupported.

Note on `autotests/data/kartöffelchen.jpg`: an early shell loop reported it missing, but that was
shell quoting of non-ASCII paths, not absence. It is present and hashed normally. No further
non-ASCII paths were affected once the baseline was generated with `git ls-files -z`.

## 0.4 Toolchain and environment (`evidence/env.txt`)

```
HEAD: 73ee06351c30501c9aa990c7d976f8e604ef0c06
cmake: cmake version 4.2.3
ninja: /bin/sh: 1: ninja: not found
cc: cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
c++: c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0
python3: Python 3.14.4
Qt6: 6.10.2
KF6CoreAddons: 6.24.0
poppler-qt6 (system): 26.01.0
reuse: not installed
appstreamcli: AppStream version: 1.1.2
flatpak-builder: not installed
xvfb-run: /usr/bin/xvfb-run
qpdf: not installed
pdfinfo: pdfinfo version 26.01.0
mutool: mutool version 1.27.0
deps dir: /home/workstation/tools/deps/build-poppler-stacking /home/workstation/tools/deps/poppler-26.08 /home/workstation/tools/deps/poppler-26.08-staging /home/workstation/tools/deps/poppler-26.08.0 /home/workstation/tools/deps/poppler-26.08.0.tar.xz
build cache poppler: CMAKE_BUILD_RPATH:UNINITIALIZED=/home/workstation/tools/deps/poppler-26.08-staging/lib;CMAKE_PREFIX_PATH:UNINITIALIZED=/home/workstation/tools/deps/poppler-26.08;Poppler_Core_INCLUDE_DIR:PATH=/home/workstation/tools/deps/poppler-26.08-staging/include/poppler;
```

What this means for the gates:

- **No `reuse`** → Phase 2 must install it in a venv (network) or mark lint-dependent rows `E3`.
- **No `flatpak-builder`** → Phase 7/9 cannot execute a Flatpak build here; the gate will be
  "blocked by environment", not "passed".
- **No `qpdf` CLI** → QPDF export checks use the linked library only; CLI verification is unavailable.
- **No `ninja`** → builds use the default generator (Makefiles); parallel build is `make -j`.
- **Poppler**: system `poppler-qt6` is 26.01.0, but `build/CMakeCache.txt` resolves
  `Poppler_Core_INCLUDE_DIR` from `/home/workstation/tools/deps/poppler-26.08-staging` while
  `CMAKE_PREFIX_PATH` points at `poppler-26.08`. This is the configuration trap recorded in the plan
  (§2.6, P5-06): the build does **not** use the tree that `CMAKE_PREFIX_PATH` names.

## 0.5 What this phase establishes, and what it does not

Establishes: the exact tree that later phases edited, the file inventory, the toolchain, and the
concurrency condition that limits every claim.

Does not establish: that the tree is reproducible (needs Phase 9), that any licence claim is correct
(Phases 3–6), or that the working tree is quiescent (it is not).

## 0.6 Commands

```sh
git rev-parse HEAD; git log -1 --format=%s; git rev-parse HEAD^
git rev-list --count HEAD; git ls-files | wc -l
git status --porcelain=v1
git ls-files -z | xargs -0 sha256sum   # failed on the broken symlink, then:
python3 …  # per-file lstat: hash regular files, mark symlinks/missing
cmake --version; cc --version; python3 --version
pkg-config --modversion Qt6Core KF6CoreAddons poppler-qt6
reuse --version; appstreamcli --version; flatpak-builder --version
grep -E 'Poppler_Core_INCLUDE_DIR|CMAKE_PREFIX_PATH' build/CMakeCache.txt
readlink -f mobile/android/icon-playstore.png; test -e mobile/android/icon-playstore.png
```

**Human review needed?** Yes, one decision was taken rather than asked: recording `73ee06351c30501c9aa990c7d976f8e604ef0c06` as the
audit base instead of `0f13a44`, and excluding the other agent's paths from any hash claim. This is
reversible and documented; if you want the plan to keep citing `0f13a44` as the base, say so.
