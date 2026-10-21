# Verification — build and test at the audited commit (Phase 9)

**Audit base:** `73ee063` (working tree also carries this pass's changes; `00-baseline.md` §0.1
records the concurrency condition)
**Date:** 2026-10-21
**Build directory:** `/tmp/opencode/charta-build` — deliberately **out of tree**, because the
repository's own `build/` directory belongs to the other agent working in this checkout.

## 1. Configuration

```sh
cmake -S . -B /tmp/opencode/charta-build \
      -DOKULAR_UI=desktop -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/home/workstation/tools/deps/poppler-26.08-staging
```

Log: `evidence/build-configure.log`

| Component | Resolved to | Note |
| --- | --- | --- |
| Poppler | `/home/workstation/tools/deps/poppler-26.08-staging`, 26.08.0 | **the patched one** — this prefix has `setUserRotation`; the `poppler-26.08` prefix does not, and the repository's stale `build/CMakeCache.txt` points `CMAKE_PREFIX_PATH` at exactly the wrong tree. The prefix was therefore given explicitly |
| QPDF | 11.9.1, `FetchContent`, built **static** | `libqpdf.a`; the Apache-2.0 obligation applies |
| Qt 6 | 6.10.2 | system |
| KDE Frameworks 6 | 6.24.0 | system |
| libspectre | 0.2.12 (system) | the Spectre generator **is** built in a source build — see §5 |
| DjVuLibre | 3.5.29 | |
| QMobipocket6 | **not found** | the mobipocket generator is not built in this configuration |
| EPub, Discount, Exiv2/KExiv2, TIFF, Freetype, ZLIB | found | |

## 2. Build result: **PASS**

The build completed successfully (`BUILD_OK`, 100 %, all targets). Log: `evidence/build.log`.
It was built twice: once before the Phase 8/9 edits and once after, both green.

* 13 warnings in the whole build; none licence-related.
* The namespace rename (`okular_generators` -> `charta_generators`) compiles and installs
  consistently: the build tree produces `build/bin/charta_generators/` and the install tree
  `/usr/lib/.../qt6/plugins/charta_generators/`.

## 3. Test suite: **no regression**, upstream suites reported separately

```sh
xvfb-run --server-args="-screen 0 1024x768x24" docs/licensing/run-tests.sh /tmp/opencode/charta-build
```

Log: `evidence/ctest-separated.log`. The script splits the suite by **discovering** the upstream
QPDF test names from the build tree (`docs/licensing/list-upstream-tests.py`), runs Charta's own
tests with them excluded, and then runs the upstream suites **separately and visibly**. Nothing is
suppressed: an upstream failure is reported as an upstream failure.

### 3.1 The exact numbers

| Suite | Tests | Passed | Failed |
| --- | --- | --- | --- |
| **Charta's own** (`ctest -E <upstream>`) | 39 | 36 | **3** |
| **Upstream QPDF** (`ctest -R <upstream>`) | 7 | 1 (`check-assert`) | 6 |
| Total | 46 | 37 | 9 |

The upstream names: `check-assert`, `compare-for-test`, `examples`, `fuzz`, `libtests`, `qpdf`,
`zlib-flate`.

### 3.2 The exact failed-test list, with its verified status

| # | Test | Failure | Verified status |
| --- | --- | --- | --- |
| 7 | `parttest` | SIGSEGV in `Okular::Document::removeObserver` | **pre-existing.** The *baseline* binary (`build/bin/parttest`, built 2026-10-20, before this pass) segfaults identically. Mechanism: the branding SVG lives in `shell/shell.qrc` (`prefix="/branding"`), not in `part/part.qrc`, so `:/branding/charta.svg` is empty in a part-only test. The `.qrc` files and `icons/` are unchanged by this pass |
| 24 | `addremoveannotationtest` | 2 of 13 cases: `testPixmapAnnotationRevisions` (`observer.pixmapRevisions.size() >= 2` false) and `testClipboardImageStampSaveReloadRemainsEditable` (`afterMoveBounds.width() > 10` false) | **pre-existing.** The *baseline* binary fails the **same two cases** with the same messages |
| 35 | `annotationtoolbartest` | 2 of 24 cases: `testAnnotationToolBar(first tab)` and `testAnnotationToolBar(second tab)` (`annToolBar->isVisible()` false) | **pre-existing, and slightly better than baseline:** the baseline binary fails **3** cases (these two plus `testAnnotationToolBarActionsEnabledState(pdf)`) |
| — | `mainshelltest` | 1 of 40 cases (`testShell(page number attach unique)`) in one earlier run | **flaky, not a failure:** 40/40 in the first full run, and 40/40 on two standalone re-runs. It did not fail in the final run |

### 3.3 The upstream QPDF failures

`compare-for-test`, `qpdf`, `libtests`, `examples`, `zlib-flate` and `fuzz` fail in this
environment. They are **upstream failures in the vendored library**: qpdf's own test harness
(`qtest`, driven by perl) needs network access and the reference data it downloads. The failure
output shows the harness running and comparing, not a Charta symbol anywhere. They are recorded in
the release checklist as upstream failures, not as Charta regressions.

**Why they cannot be switched off:** qpdf's `CMakeLists.txt` adds them unconditionally — no
`BUILD_TESTING` guard, no option — and they cannot be labelled from outside either, because
CMake's `set_tests_properties()` only sees tests defined in the current directory and
`cmake_language(DEFER DIRECTORY ...)` refuses an already-processed directory. The separation in
`run-tests.sh` is therefore done by name, with the names read from the generated CTest files.

### 3.4 A defect this verification found

**Charta never called `enable_testing()`.** Its own test suite was registered only because the
*fetched QPDF* calls `enable_testing()` in its own `CMakeLists.txt`. Had QPDF stopped being
fetched — or had the upstream suites been excluded the way they are now — Charta would have had
**no tests at all**, silently. `enable_testing()` is now called by Charta itself, in the
`if(BUILD_TESTING)` block, so the suite no longer depends on a third party.

**Conclusion: this pass introduced no test regression.** The only product-code change with runtime
effect is the plugin-namespace rename, and the generator-dependent tests (ink appearance, add and
remove annotations, reorder) pass, which shows the generators are still discovered under the new
namespace.

## 4. Staged install: **PASS**

```sh
DESTDIR=/tmp/opencode/charta-install cmake --install /tmp/opencode/charta-build
```

1 214 files installed into the staging prefix. The compliance artefacts are present and were
verified by inspection:

```
/usr/share/doc/charta/THIRD-PARTY-NOTICES.md
/usr/share/doc/charta/QPDF-NOTICE.md
/usr/share/licenses/charta/*.txt      (16 texts)
```

The collision analysis against a distro Okular install is in `install-manifest.txt`.

## 5. What the build proves about the AGPL decision

This is a **source** build on a machine that has libspectre, so the Spectre generator is built and
links `libspectre`, which links `libgs.so.10` (AGPL-3.0):

```
okularGenerator_ghostview.so -> libspectre.so.1 -> libgs.so.10
```

That is exactly the case D11 covers: **a source build may carry AGPL code; the Flatpak must not.**
The Flatpak manifest no longer builds `ghostscript`/`libspectre`, so the shipped bundle has no AGPL
component. A user who builds from source on a machine with libspectre gets the AGPL-linked plugin —
documented in `agpl-ghostscript.md` §5.

## 6. What could not be verified in this environment

| Item | Why | What would close it |
| --- | --- | --- |
| Flatpak build | `flatpak-builder` is not installed | build the manifest on a machine that has it; the manifest is valid JSON and its patch references are checked by the CI job |
| The Flatpak's behaviour after the AGPL removal | same | a real Flatpak build |
| `reuse lint` against a **committed** tree | it ran against the working tree and exits 0; the files are uncommitted | re-run after committing |
| QPDF's own test suites | see §3 | a patch to the fetched qpdf source, or a CI filter that excludes them |
| The fixture private-key check | not completed in this pass | inspect `autotests/data/fake_okular_certstore/` |
| DRM / signature behaviour | no test certificates in this environment | — |
