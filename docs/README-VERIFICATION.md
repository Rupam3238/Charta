# README verification

## Scope and baseline

A static comparison of the `main` branch of
[Rupam3238/Charta](https://github.com/Rupam3238/Charta) at commit `0f13a44` (`Rotation Done`,
2026-10-21). It covers the README, the notes in `docs/`, the CMake files, the Flatpak manifest,
selected sources and the registered tests. It is primarily a source inspection, not a build or
runtime validation; the one runtime observation used for the README screenshots is recorded
below. Re-run these checks after source, dependency or packaging changes.

Results for the Poppler fork `Rupam3238/poppler-patch-for-charta-26.08.0` (commit `0fe4c9d`) were
carried forward from an earlier revision and **not rechecked** at this baseline.

## Claim-to-evidence table

| Claim | Evidence | Result |
| --- | --- | --- |
| Executable `charta`, app ID `org.charta.reader`, version `26.11.70` | `CMakeLists.txt:4-9`, `shell/CMakeLists.txt:24`, `.flatpak-manifest.json` | Confirmed in source; no release or install test |
| Poppler is mandatory | `CMakeLists.txt:318` (`find_package(Poppler "24.08.0" COMPONENTS Qt6)`, optional package) | Optional in CMake; effectively required |
| Charta calls Poppler APIs that stock Poppler lacks | `setAppearanceStream` (`generators/poppler/annots.cpp:590,606,617`), `moveAnnotation` (`:190`), `setUserRotation` (`:118`); no feature guard | Confirmed. A build against stock Poppler is not expected to compile (not tested) |
| Fork provides `/AP` writing and annotation reorder | `setAppearanceStream`, `moveAnnotation` in the fork's `qt6/src` | Carried forward, not rechecked |
| Fork has a committed `build/` directory | 1523 tracked files under `build/` in the fork | Carried forward, not rechecked (fork, not Charta) |
| `patches/poppler-annotation-rotation.patch` and `ChartaRotate/LocalW/LocalH` | Patch present in `patches/` and contains the three keys | Confirmed. Presence does not prove that an installed Poppler contains the changes |
| Rotation scope: text, stamps, shapes, line, ink | `README.md`; `annots.cpp:118,1617`; `autotests/annotationrotationtest.cpp`; `autotests/annotationpersistencetest.cpp` | Documented and implemented. The persistence test saves and reopens through Poppler; neither test was run |
| `Ctrl+Shift+B` opens Document Builder | `shell/shell.cpp:588`; runtime check with an annotation tool active | Ambiguous: the shortcut displayed an ambiguity warning and triggered neither action. `File → Document Builder…` worked. Also bound to add-to-quick-tools in `part/annotationactionhandler.cpp:2204` |
| `File → Document Builder…` | `shell/shell.cpp:584` | Confirmed |
| Default shortcut table | `annotationactionhandler.cpp:2185-2205`, `part/pageview.cpp:922,942,949` | Confirmed |
| 180 ms hold threshold; seven-point smoothing | `annotationactionhandler.cpp:78`; `part/annotationtools.cpp:457` | Confirmed |
| Last page cannot be deleted | `part/part.cpp:4793` | Confirmed |
| CLI flags incl. `--noraise`, `--editor-cmd` | `shell/main.cpp:127-134` | Confirmed |
| Autosave guards | `Part::canAutoSaveNow` (`part/part.cpp:3690`) and `Part::slotAutoSave` (`:3717`); the timer defers while a mouse button is held, within 2 s of the last input, or while a modal dialog is open | Confirmed in source; timing not exercised |
| Document Builder export replaces atomically on POSIX | `part/documentbuilderexport.cpp:395-398` (`::rename` under `Q_OS_UNIX`) | Confirmed; other platforms remove then rename |
| Page saving uses QPDF, else `pdfseparate`/`pdfunite` | `generators/poppler/generator_pdf.cpp:15,2231,2387-2431` | Confirmed in source; tools not tested |
| QPDF fetched as v11.9.1 when not found | `CMakeLists.txt:267` | Confirmed |
| Flatpak uses the full fork | `.flatpak-manifest.json`: upstream Poppler 26.08.0 plus `patches/poppler-ink-appearance.patch` only | **No.** The rotation patch is not applied, and the manifest is not expected to build while the generator calls the patched APIs unguarded (not tested) |
| Charta-specific tests are registered | `autotests/CMakeLists.txt` | Confirmed. Also `annotationrotationtest`, `annotationpersistencetest` (needs Poppler Qt6) and `annotationclipboardtest` (desktop UI). Registration is not evidence that tests pass |

## Runtime observation (2026-10-21)

The existing development build was launched under Xvfb to capture the README screenshots. With
an annotation tool active, `Ctrl+Shift+B` produced an ambiguity warning and opened nothing;
`File → Document Builder…` opened the builder. The screenshots use an original sample PDF and
are illustrative; they were not produced from a clean build.

## Checks not performed

- Clean configure, compile, install or launch of Charta from a clean tree.
- Flatpak build or runtime verification.
- Build or runtime verification of the Poppler fork.
- Execution of the CTest suite.
- Stock-Poppler ink behaviour.
- Runtime tests for page-edit failure recovery, autosave timing, password handling, bookmark
  remapping or Document Builder output fidelity.
- SPDX audit; Drawy and Poppler-fork provenance and licensing.

## Repository hygiene (outside the documentation)

The tree contains files that no document describes and that look like local artefacts:
`CMakeFiles/CMakeSystem.cmake`, `test_savedepth.cpp`, `test_savedepth_simple.cpp` and a compiled
ELF binary `test_savedepth_simple`. Consider removing them or adding them to `.gitignore`.

## Correction history

An earlier revision wrongly stated that `Ctrl+Shift+B` was not the Document Builder shortcut and
that Charta had a committed `build/` directory. It also reported the rotation patch as absent;
it is present as of commit `0f13a44`.

## Maintenance rule

Update this document whenever the relevant source, patches, build configuration or manifest
changes. Record the inspected commit, and distinguish source-level evidence from tests actually
executed. Do not carry forward a conclusion from an earlier commit without rechecking it.
