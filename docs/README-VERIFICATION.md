# README verification

Source: shallow clones of `Rupam3238/Charta` (commit `66b15b4`) and
`Rupam3238/poppler-patch-for-charta-26.08.0` (commit `0fe4c9d`). Static inspection only.

| Claim | Evidence | Result |
| --- | --- | --- |
| Logo, executable `charta`, app ID `org.charta.reader` | files, `shell/CMakeLists.txt`, manifest | Confirmed |
| Poppler mandatory | `find_package(Poppler 24.08.0 COMPONENTS Qt6)` optional-package | Optional in CMake; effectively required |
| Fork provides `/AP` writing and annotation reorder | `setAppearanceStream`, `moveAnnotation` in `qt6/src` | Confirmed |
| Fork has committed `build/` directory | 1523 tracked files under `build/` | Confirmed (fork, not Charta) |
| `patches/poppler-annotation-rotation.patch`, `ChartaRotate/LocalW/LocalH`, generic wrapper | not in either repo | **Unsubstantiated; removed** |
| `Ctrl+Shift+B` opens Document Builder | `shell/shell.cpp:588` | Confirmed; also bound to add-to-quick-tools in `annotationactionhandler.cpp:2204` |
| `File → Document Builder…` | `shell/shell.cpp:584` | Confirmed |
| CLI flags incl. `--noraise`, `--editor-cmd` | `shell/main.cpp:127-134` | Confirmed |
| Autosave guards, POSIX atomic rename, QPDF fallback | `part.cpp`, `documentbuilderexport.cpp`, `generator_pdf.cpp` | Confirmed |
| Flatpak uses full fork | manifest applies only local patch | No |

## Not performed
Clean build, install, Flatpak build, the test suite, fork build; stock-Poppler ink behaviour;
password handling and bookmark remapping at runtime; SPDX audit; Drawy and fork provenance.

## Correction note
An earlier revision of this document wrongly stated that `Ctrl+Shift+B` was not the Document
Builder shortcut and that Charta had a committed `build/` directory. Both were corrected above. 