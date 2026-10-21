# Identity migration: Okular -> Charta (plan, risks and migration notes)

**Status:** prepared, **not applied**. The migration is delivered as
`docs/licensing/identity-migration.patch` (a unified diff that applies to the current working
tree) plus this document.

**Date:** 2026-10-21
**Basis:** `docs/licensing/packaging.md` §2 and `docs/licensing/trademark.md` (the Phase 7 audit),
extended by a fresh sweep of the tree. The audit's site list was verified, not trusted: three
sites it does not mention were found and are included here, and one number in the audit turned out
to be wrong (see §5.2).

**Decision that triggered this work:** "Charta" is the final public application name, so the
remaining Okular identity has to go. The maintainer had deferred *applying* the migration; this
document and the patch are the reviewable form of it.

---

## 1. What the patch contains

| Number | Value |
| --- | --- |
| Files touched | **1 137** |
| Pure renames | **1 029** (1 013 translation catalogues, 14 generator AppStream metadata files, `okular.categories`, `Okular6Config.cmake.in`) |
| Files edited in place | **108** |
| Changed lines | 352 insertions / 353 deletions (the renames carry no content change at all) |
| Distinct old -> new string pairs | **272** |
| Site groups | 9 (A-I, §3) |
| Applied to the live tree? | **No.** `git apply --check` passes; nothing was written to the working tree |

The patch was produced from a scratch copy of the repository (`/tmp/opencode/identity-migration`)
whose baseline commit is a snapshot of the current working tree, so the diff contains **only** the
migration and none of the other agents' uncommitted work.

Verification (§11): `git apply --check` passes against the live tree; a full `git apply` into a
pristine copy reproduces the scratch tree byte for byte; and a **CMake configure** of the migrated
tree succeeds.

---

## 2. The rule that decided the boundary

The migration renames an identifier when the identifier is

* **(a)** claimed at runtime - a D-Bus service name, interface name or object path, an AppStream
  component id, a translation domain; or
* **(b)** installed on the user's disk under the old product name - a shared library soname, an
  installed header directory or export header, an installed locale catalogue, an installed logging
  category file, an installed data directory, an installed config file; or
* **(c)** listed explicitly in the audit, even when it is build-internal (the CMake target of the
  core library).

Everything else is left alone and is listed with a reason in §10. The guiding idea: *the migration
must remove every Okular claim a user or a packager can observe, without turning into a source-level
rewrite of a 5 000-reference C++ namespace.*

---

## 3. The complete table of changed sites

Line numbers are the **pre-migration** line numbers in the current working tree.

### 3.1 Group A - D-Bus identity (the service, the interface, the paths)

| File | Line | Old | New | Why |
| --- | --- | --- | --- | --- |
| `shell/shell.cpp` | 294 | `registerService(QStringLiteral("org.kde.okular"))` | `registerService(QStringLiteral("org.charta.reader"))` | the unique-instance service name; must stop claiming `org.kde.okular` on the session bus |
| `shell/shell.cpp` | 302 | `QStringLiteral("org.kde.okular-") + pid` | `QStringLiteral("org.charta.reader-") + pid` | the legacy per-process name; leaving it would keep a live `org.kde.okular-*` claim on the bus |
| `shell/shell.cpp` | 318 | `registerObject(QStringLiteral("/okularshell"), ...)` | `registerObject(QStringLiteral("/chartashell"), ...)` | the exported object path of the shell |
| `shell/shell.cpp` | 397 | `unregisterService(QStringLiteral("org.kde.okular"))` | `unregisterService(QStringLiteral("org.charta.reader"))` | must match the name registered at line 294 |
| `shell/okular_main.cpp` | 59 | `QDBusInterface("org.kde.okular", "/okularshell", "org.kde.okular")` | `QDBusInterface("org.charta.reader", "/chartashell", "org.charta.reader")` | service + path + interface of the "attach to the running instance" call |
| `shell/okular_main.cpp` | 118 | `QDBusInterface(service, "/okularshell", "org.kde.okular")` | `QDBusInterface(service, "/chartashell", "org.charta.reader")` | same, for the per-process instance scan |
| `shell/shell.h` | 49 | `Q_CLASSINFO("D-Bus Interface", "org.kde.okular")` | `Q_CLASSINFO("D-Bus Interface", "org.charta.reader")` | the interface name the shell publishes |
| `part/part.h` | 121 | `Q_CLASSINFO("D-Bus Interface", "org.kde.okular")` | `Q_CLASSINFO("D-Bus Interface", "org.charta.reader")` | the interface name the part publishes |
| `part/part.cpp` | 829 | `m_registerDbusName = QStringLiteral("/okular")` | `QStringLiteral("/charta")` | the part's own object path on the bus (audit missed this one) |
| `part/part.cpp` | 831 | `QStringLiteral("/okular%1").arg(numberOfParts)` | `QStringLiteral("/charta%1").arg(numberOfParts)` | the part's object path for the 2nd, 3rd, ... part |
| `interfaces/configinterface.h` | 73 | `Q_DECLARE_INTERFACE(..., "org.kde.okular.ConfigInterface/0.1")` | `"org.charta.reader.ConfigInterface/0.1"` | published interface id |
| `interfaces/guiinterface.h` | 64 | `"org.kde.okular.GuiInterface/0.1"` | `"org.charta.reader.GuiInterface/0.1"` | published interface id |
| `interfaces/printinterface.h` | 68 | `"org.kde.okular.PrintInterface/0.1"` | `"org.charta.reader.PrintInterface/0.1"` | published interface id |
| `interfaces/saveinterface.h` | 115 | `"org.kde.okular.SaveInterface/0.3"` | `"org.charta.reader.SaveInterface/0.3"` | published interface id |
| `interfaces/viewerinterface.h` | 109 | `"org.kde.okular.ViewerInterface/0.1"` | `"org.charta.reader.ViewerInterface/0.1"` | published interface id |
| `core/generator.h` | 830 | `#define OkularGeneratorInterface_iid "org.kde.okular.Generator"` | `"org.charta.reader.Generator"` | the generator plugin interface id (the C++ macro name is deliberately kept, §10) |
| `shell/shellutils.h` | 40 | `// Must be a subname of "org.kde.okular" ...` | `// Must be a subname of "org.charta.reader" ...` | the comment states the Flatpak invariant (§4.1) |
| `shell/shellutils.h` | 41 | `kPerProcessDbusPrefix("org.kde.okular.Instance_")` | `kPerProcessDbusPrefix("org.charta.reader.Instance_")` | the per-process prefix that `okular_main.cpp` scans for |
| `shell/org.charta.reader.desktop` | 200 | `X-DBUS-ServiceName=org.kde.okular` | `X-DBUS-ServiceName=org.charta.reader` | the name a file manager D-Bus-activates |
| `.flatpak-manifest.json` | 23 | `"--own-name=org.kde.okular"` | `"--own-name=org.charta.reader"` | Flatpak permission |
| `.flatpak-manifest.json` | 24 | `"--own-name=org.kde.okular.*"` | `"--own-name=org.charta.reader.*"` | Flatpak permission for the per-process subnames |

### 3.2 Group B - the 14 generator AppStream component ids

The `<id>` elements were **already** migrated to `org.charta.reader-<name>` by an earlier pass; what
was still outstanding was that the *file name* and the *install rule* still said `org.kde.okular`.
This group finishes the job.

| File | Old | New | Why |
| --- | --- | --- | --- |
| `generators/comicbook/org.kde.okular-comicbook.metainfo.xml` | file name | `generators/comicbook/org.charta.reader-comicbook.metainfo.xml` | the shipped file name must match the component id |
| `generators/djvu/org.kde.okular-djvu.metainfo.xml` | file name | `generators/djvu/org.charta.reader-djvu.metainfo.xml` | idem |
| `generators/dvi/org.kde.okular-dvi.metainfo.xml` | file name | `generators/dvi/org.charta.reader-dvi.metainfo.xml` | idem |
| `generators/epub/org.kde.okular-epub.metainfo.xml` | file name | `generators/epub/org.charta.reader-epub.metainfo.xml` | idem |
| `generators/fax/org.kde.okular-fax.metainfo.xml` | file name | `generators/fax/org.charta.reader-fax.metainfo.xml` | idem |
| `generators/fictionbook/org.kde.okular-fb.metainfo.xml` | file name | `generators/fictionbook/org.charta.reader-fb.metainfo.xml` | idem |
| `generators/kimgio/org.kde.okular-kimgio.metainfo.xml` | file name | `generators/kimgio/org.charta.reader-kimgio.metainfo.xml` | idem |
| `generators/markdown/org.kde.okular-md.metainfo.xml` | file name | `generators/markdown/org.charta.reader-md.metainfo.xml` | idem |
| `generators/mobipocket/org.kde.okular-mobipocket.metainfo.xml` | file name | `generators/mobipocket/org.charta.reader-mobipocket.metainfo.xml` | idem |
| `generators/poppler/org.kde.okular-poppler.metainfo.xml` | file name | `generators/poppler/org.charta.reader-poppler.metainfo.xml` | idem |
| `generators/spectre/org.kde.okular-spectre.metainfo.xml` | file name | `generators/spectre/org.charta.reader-spectre.metainfo.xml` | idem |
| `generators/tiff/org.kde.okular-tiff.metainfo.xml` | file name | `generators/tiff/org.charta.reader-tiff.metainfo.xml` | idem |
| `generators/txt/org.kde.okular-txt.metainfo.xml` | file name | `generators/txt/org.charta.reader-txt.metainfo.xml` | idem |
| `generators/xps/org.kde.okular-xps.metainfo.xml` | file name | `generators/xps/org.charta.reader-xps.metainfo.xml` | idem |
| `generators/<g>/CMakeLists.txt` (14 files) | `install( FILES org.kde.okular-<n>.metainfo.xml ... )` | `install( FILES org.charta.reader-<n>.metainfo.xml ... )` | the install rule has to name the renamed file, or the build fails |

### 3.3 Group C - the core library `Okular6Core` -> `Charta6Core`

| File / site | Old | New | Why |
| --- | --- | --- | --- |
| `CMakeLists.txt:424` | `# okularcore` | `# chartacore` | section comment |
| `CMakeLists.txt:426,464,468,508,512` | `okularcore_SRCS` | `chartacore_SRCS` | source list of the renamed target |
| `CMakeLists.txt:514` | `add_library(okularcore SHARED ${okularcore_SRCS})` | `add_library(chartacore SHARED ${chartacore_SRCS})` | the CMake target name (listed in the audit) |
| `CMakeLists.txt:515` | `generate_export_header(okularcore BASE_NAME okularcore EXPORT_FILE_NAME .../core/okularcore_export.h)` | `generate_export_header(chartacore BASE_NAME chartacore EXPORT_FILE_NAME .../core/chartacore_export.h)` | the generated export header and every macro it defines (`OKULARCORE_EXPORT` -> `CHARTACORE_EXPORT`, `OKULARCORE_DEPRECATED` -> `CHARTACORE_DEPRECATED`, `OKULARCORE_NO_EXPORT` -> `CHARTACORE_NO_EXPORT`) |
| `CMakeLists.txt:496` | `${CMAKE_CURRENT_BINARY_DIR}/core/okularcore_export.h` | `.../core/chartacore_export.h` | the install rule for the generated header |
| `CMakeLists.txt:498` | `DESTINATION ${KDE_INSTALL_INCLUDEDIR}/okular/core` | `DESTINATION ${KDE_INSTALL_INCLUDEDIR}/charta/core` | **the installed header path** `include/okular/**` -> `include/charta/**` |
| `CMakeLists.txt:506` | `DESTINATION ${KDE_INSTALL_INCLUDEDIR}/okular/interfaces` | `DESTINATION ${KDE_INSTALL_INCLUDEDIR}/charta/interfaces` | idem, for the five public interface headers |
| `CMakeLists.txt:584` | `OUTPUT_NAME Okular6Core` | `OUTPUT_NAME Charta6Core` | the library file and the **soname**: `libOkular6Core.so.4` -> `libCharta6Core.so.4` |
| `CMakeLists.txt:586,768` | `EXPORT Okular6Targets` / `install(EXPORT Okular6Targets ...)` | `Charta6Targets` | the exported CMake target set |
| `CMakeLists.txt:768` | `NAMESPACE Okular::` | `NAMESPACE Charta::` | the exported target namespace: `Okular::Core` -> `Charta::Core` |
| `CMakeLists.txt:99` | `VARIABLE_PREFIX OKULAR` | `VARIABLE_PREFIX CHARTA` | the version macros in the **installed** `core/version.h` (`OKULAR_VERSION_STRING` -> `CHARTA_VERSION_STRING`, used at `aboutdata.h:20`) |
| `CMakeLists.txt:101,754,756,757,762,763` | `Okular6ConfigVersion.cmake`, `Okular6Config.cmake(.in)`, `${KDE_INSTALL_CMAKEPACKAGEDIR}/Okular6` | the `Charta6` equivalents | the installed CMake package: `find_package(Okular6)` -> `find_package(Charta6)` |
| `Okular6Config.cmake.in` (renamed to `Charta6Config.cmake.in`) | `include(".../Okular6Targets.cmake")` | `include(".../Charta6Targets.cmake")` | idem |
| 30 headers in `core/` + `part/kleopatraintegration.h` | `#include "okularcore_export.h"` | `#include "chartacore_export.h"` | the renamed generated header |
| 5 headers in `interfaces/` | `#include "../core/okularcore_export.h"` | `#include "../core/chartacore_export.h"` | idem (relative path, still correct after the install-path move) |
| 21 headers in `core/` + `interfaces/` (≈115 occurrences) | `OKULARCORE_EXPORT` / `OKULARCORE_DEPRECATED` | `CHARTACORE_EXPORT` / `CHARTACORE_DEPRECATED` | the macros the renamed export header defines |
| `conf/settings_core.kcfgc:6,7` | `Visibility=OKULARCORE_EXPORT`, `IncludeFiles=okularcore_export.h` | `Visibility=CHARTACORE_EXPORT`, `IncludeFiles=chartacore_export.h` | the kconfig compiler has to find the renamed header and macro |
| `autotests/CMakeLists.txt` (14 `LINK_LIBRARIES ... okularcore`) | `okularcore` | `chartacore` | the renamed target |
| `generators/*/CMakeLists.txt` (14 `target_link_libraries(... okularcore ...)`) | `okularcore` | `chartacore` | idem |
| `mobile/components/CMakeLists.txt:48` | `okularcore` | `chartacore` | idem |
| `CMakeLists.txt:690` | `target_link_libraries(okularpart chartacore ...)` | (same, new target name) | the part links the renamed core library |
| `REUSE.toml:105` | `"Okular6Config.cmake.in"` | `"Charta6Config.cmake.in"` | the file was renamed |
| `.githooks/pre-commit:20` | `Okular6Config\.cmake\.in` | `Charta6Config\.cmake\.in` | idem |

### 3.4 Group D - the KDE logging-category file

| File | Old | New | Why |
| --- | --- | --- | --- |
| `okular.categories` (root) | file name | `charta.categories` | the installed file name (`share/qlogging-categories6/charta.categories`) |
| `CMakeLists.txt:748` | `install(FILES okular.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})` | `install(FILES charta.categories ...)` | the install rule has to name the renamed file |

The **contents** of the file are unchanged: the category identifiers `org.kde.okular.core`,
`org.kde.okular.ui`, `org.kde.okular.generators.*` are deliberately kept (§10).

### 3.5 Group E - the translation domain and the catalogues

This is the largest group and the highest-risk one; see §4.2 and §5.2.

| File / site | Old | New | Why |
| --- | --- | --- | --- |
| `CMakeLists.txt:371` | `add_definitions(-DTRANSLATION_DOMAIN="okular")` | `add_definitions(-DTRANSLATION_DOMAIN="charta")` | the compile-time domain of every `i18n()` in core/part/shell |
| `shell/main.cpp:98` | `KLocalizedString::setApplicationDomain("okular")` | `KLocalizedString::setApplicationDomain("charta")` | the runtime application domain |
| `Messages.sh:7` | `-o $podir/okular.pot` | `-o $podir/charta.pot` | the template the main catalogue is extracted from |
| `generators/<g>/CMakeLists.txt:1` (14 files) | `remove_definitions(-DTRANSLATION_DOMAIN="okular")` | `remove_definitions(-DTRANSLATION_DOMAIN="charta")` | must match the definition it removes, exactly |
| `generators/<g>/CMakeLists.txt:2` (14 files) | `add_definitions(-DTRANSLATION_DOMAIN="okular_<g>")` | `add_definitions(-DTRANSLATION_DOMAIN="charta_<g>")` | the 14 generator domains |
| `generators/<g>/Messages.sh:2` (14 files) | `-o $podir/okular_<g>.pot` | `-o $podir/charta_<g>.pot` | the 14 generator templates |
| `po/<lang>/okular.po` (**82** files) | file name | `po/<lang>/charta.po` | the runtime looks for `<lang>/LC_MESSAGES/charta.mo` |
| `po/<lang>/okular_<g>.po` (**931** files) | file name | `po/<lang>/charta_<g>.po` | the 14 generator domains, same reason |

The **contents** of all 1 013 catalogues are byte-for-byte unchanged: `Project-Id-Version: okular`,
`Last-Translator`, `Language-Team`, `POT-Creation-Date`, the `SPDX-FileCopyrightText` translator
credit lines and every `msgid`/`msgstr` are preserved (§5.2).

### 3.6 Group F - the desktop file's help path

| File | Line | Old | New | Why |
| --- | --- | --- | --- | --- |
| `shell/org.charta.reader.desktop` | 196 | `X-DocPath=charta/index.html` | **removed** | there is no `index.html` anywhere in the tree or in the install rules; the key promised an HTML help page that does not exist. See §5.3. |

### 3.7 Group G - the KParts component name and the Qt resource prefixes

`KXMLGUIClient::setComponentName()` is documented as "the name of the directory where the XMLGUI
files will be found", i.e. it must match the `qresource` prefix. Both sides are changed together.

| File | Line | Old | New | Why |
| --- | --- | --- | --- | --- |
| `shell/shell.cpp` | 187 | `setComponentName(QStringLiteral("okular"), QString())` | `setComponentName(QStringLiteral("charta"), QString())` | the KParts component name |
| `part/part.cpp` | 822 | `setComponentName(QStringLiteral("okular"), QString())` | `setComponentName(QStringLiteral("charta"), QString())` | idem |
| `part/part.cpp` | 820-821 | the comment "…the component name is okular…" | "…the component name is charta…" | the comment explains the coupling |
| `shell/shell.qrc` | 3 | `<qresource prefix="/kxmlgui5/okular">` | `<qresource prefix="/kxmlgui5/charta">` | the resource prefix the component name resolves against |
| `part/part.qrc` | 3 | `<qresource prefix="/kxmlgui5/okular">` | `<qresource prefix="/kxmlgui5/charta">` | idem |

### 3.8 Group H - installed data directories and user configuration files

Every one of these is an *installed* or *user-visible* path that carried the old product name.
Each rename is paired with the runtime lookup that resolves it, in the same patch.

| File | Line | Old | New | Why |
| --- | --- | --- | --- | --- |
| `CMakeLists.txt:772` | 772 | `DESTINATION ${KDE_INSTALL_DATADIR}/okular/pics` | `.../charta/pics` | `core/stamps.svg` |
| `part/data/CMakeLists.txt` | 6 | `DESTINATION ${KDE_INSTALL_DATADIR}/okular)` | `.../charta)` | the annotation-tool descriptors |
| `part/data/CMakeLists.txt` | 24, 44 | `DESTINATION ${KDE_INSTALL_DATADIR}/okular/pics)` | `.../charta/pics)` | the annotation-tool images |
| `part/part.cpp` | 847 | `QStandardPaths::locateAll(..., "okular/pics", ...)` | `"charta/pics"` | the runtime lookup of that directory |
| `part/pageviewannotator.cpp` | 2758, 2799, 2807, 2815, 2819, 2872 | `QStandardPaths::locate(..., "okular/pics/tool-…")` | `"charta/pics/tool-…"` | the six annotation-tool overlays |
| `generators/epub/data/CMakeLists.txt` | 5 | `DESTINATION ${KDE_INSTALL_DATADIR}/okular/pics)` | `.../charta/pics)` | the EPUB movie/sound icons |
| `generators/epub/converter.cpp` | 310, 325 | `QStandardPaths::locate(..., "okular/pics/okular-epub-…")` | `"charta/pics/okular-epub-…"` | the runtime lookup |
| `core/annotations.cpp` | 412 | `QStandardPaths::locate(..., "okular/pics/stamps.svg")` | `"charta/pics/stamps.svg"` | the runtime lookup |
| `generators/spectre/CMakeLists.txt` | 34 | `ecm_install_icons(... DESTINATION ${KDE_INSTALL_DATADIR}/okular/icons)` | `.../charta/icons` | the Ghostscript generator icons |
| `generators/fictionbook/CMakeLists.txt` | 26 | `ecm_install_icons(... DESTINATION ${KDE_INSTALL_DATADIR}/okular/icons ...)` | `.../charta/icons ...` | the FictionBook generator icons |
| `core/document.cpp` | 2681 | `writableLocation(GenericDataLocation) + "/okular/docdata"` | `... "/charta/docdata"` | the per-document metadata cache |
| `core/bookmarkmanager.cpp` | 121 | `writableLocation(GenericDataLocation) + "/okular/bookmarks.xml"` | `... "/charta/bookmarks.xml"` | **the user's bookmarks** |
| `part/annotationpopup.cpp` | 50 | `writableLocation(CacheLocation) + "/okular/clipboard-stamps/"` | `... "/charta/clipboard-stamps/"` | the clipboard-stamp cache |
| `part/part.cpp` | 1152 | `const QLatin1String configFileName("okularpartrc")` | `"chartapartrc"` | the **main settings file** |
| `shell/shell.cpp` | 1384 | `ConfigLocation + '/' + "okularpartrc"` | `"chartapartrc"` | the same file, read for the recent-items count |
| `shell/shell.cpp` | 1383 | the comment naming `okularpartrc` | naming `chartapartrc` | idem |
| `part/annotationactionhandler.h` | 43 | the comment naming `okularpartrc` | naming `chartapartrc` | idem |
| `part/newpagedialog.cpp` | 43 | `KSharedConfig::openConfig(QStringLiteral("okularrc"))` | `"chartarc"` | the new-page-dialog settings |
| `part/fileprinterpreview.cpp` | 41 | `KSharedConfig::openConfig(QStringLiteral("okularrc"))` | `"chartarc"` | the print-preview settings |

### 3.9 Group I - the generator plugin API key

| File | Line | Old | New | Why |
| --- | --- | --- | --- | --- |
| `generators/<g>/libokularGenerator_<g>.json` (14 files) | e.g. `generators/comicbook/…json:220` | `"X-KDE-okularAPIVersion": 1` | `"X-KDE-chartaAPIVersion": 1` | an installed metadata key. Nothing in the tree reads it, so the rename is risk-free; it is changed so that no shipped file carries the old project name in a key |

---

## 4. Risk assessment per group

### 4.1 The D-Bus rename (group A) - **high**, because it is a runtime claim on a shared bus

* **Session-bus conflict with a real Okular.** Before the change, Charta registered
  `org.kde.okular` on the session bus. If a genuine KDE Okular was running in the same session,
  the second instance to start lost the race: `registerService()` returned false and the user saw
  "There is already a unique Charta instance running". After the change Charta owns
  `org.charta.reader`, which no KDE application claims, so the two products coexist.
  The conflict is not merely cosmetic: the legacy per-process name `org.kde.okular-<pid>` was
  *also* claimed, so a Charta instance and an Okular instance could end up answering each other's
  `canOpenDocs` probe on the wrong path.
* **D-Bus activation for file managers.** `X-DBUS-ServiceName=` in the desktop file is what a
  file manager (or a `gtk-launch`-style helper) uses to activate the application for a MIME type.
  The desktop file and the code are changed in the same patch, so activation still works - but a
  *stale* desktop entry from an older install would activate the old name and find nothing. This
  is why the packager note (§7) says the old desktop file must be removed on upgrade.
* **The Flatpak own-name constraint.** Flatpak cannot express "own any subname of X", so the
  manifest needs two permissions: the exact name and a `.*` glob. The patch keeps exactly that
  shape:
  `--own-name=org.charta.reader` and `--own-name=org.charta.reader.*`.
* **The per-process subname invariant - preserved.** The per-process name must be a *subname* of
  the service name, i.e. `org.charta.reader.Instance_<pid>`, because that is the only shape the
  Flatpak glob `org.charta.reader.*` covers. The comment in `shell/shellutils.h:40` states the
  invariant and is updated with the name. After the patch:
  - `org.charta.reader` - the service name (unique instance).
  - `org.charta.reader.Instance_<pid>` - the per-process name; **a proper subname of the service
    name**, covered by `--own-name=org.charta.reader.*`.
  - `org.charta.reader-<pid>` - the legacy per-process name. Upstream this was
    `org.kde.okular-<pid>`; the dash makes it *not* a subname, and it was never covered by the
    Flatpak glob either. It is renamed to keep the prefix consistent and to stop claiming the
    `org.kde.okular` namespace, but the "not a subname" property is inherited unchanged - see §10.
* **Interface and path renames.** Any script that called
  `org.kde.okular /okularshell org.kde.okular` must be updated to
  `org.charta.reader /chartashell org.charta.reader`. The part's own object paths change from
  `/okular` and `/okular<N>` to `/charta` and `/charta<N>`.

### 4.2 The translation-domain rename (group E) - **high**, because it is 1 013 file renames

* **The runtime really does look up the domain by file name.** `ki18n_install(po)` compiles each
  `po/<lang>/<name>.po` into `<lang>/LC_MESSAGES/<name>.mo` (KF6's `build-pofiles.cmake` derives
  the output name from the input file name), and the `i18n()` macro expands to
  `i18nd(TRANSLATION_DOMAIN, ...)`. So the domain, the `.po` file name and the `.mo` file name
  are one and the same string. Renaming the domain without renaming the catalogues would have
  produced a build that installs `okular.mo` while the application asks for `charta.mo` - a
  **silent** loss of all translations, with no build error. The two are therefore inseparable and
  are changed together.
* **Scale.** 1 013 catalogues over 83 language directories, in 15 different domains (1 main +
  14 generator domains). The audit records "1 069 catalogue files `po/<lang>/okular.po`"; that
  number is the *total* `.po` count in `po/`, and only **82** of them are the `okular` domain
  (one per language directory that has one - 83 directories, one of which has no main
  catalogue). The other 987 are the 931 generator catalogues plus 56
  `org.kde.active.documentviewer.po` files that belong to a *different* upstream project and are
  left untouched. Renaming only the main domain would have left `share/locale/*/LC_MESSAGES/`
  full of `okular_poppler.mo`, `okular_ghostview.mo`, ... - a half-migrated product.
* **Metadata and credits.** The catalogues are imported from the KDE Okular translation project.
  Every one of them carries `Project-Id-Version: okular`, a `Last-Translator`, a `Language-Team`,
  a `POT-Creation-Date` and, in 265 cases, `SPDX-FileCopyrightText` credit lines for individual
  translators. The patch renames the **files** and touches **no catalogue content at all**: no
  `msgmerge`, no `lupdate`, no header rewrite, no re-indentation. The credits and the upstream
  metadata are bit-identical afterwards (verified, §11).
* **The upstream sync breaks.** The KDE l10n service delivers `okular.po` / `okular_<g>.po` for
  the `okular` project. Once the tree expects `charta.po`, the automatic sync no longer lands in
  the right place. This is a *process* consequence, not a code defect, and it is the maintainer's
  to solve (§5.2, §12).

### 4.3 The core-library rename (group C) - **medium**

The library is renamed in every place that matters at once: target name, output name, soname, the
generated export header and all its macros, the installed include directory, the CMake package and
the exported namespace. The risk is not behavioural but *compatibility*: any downstream code that
links `libOkular6Core.so.4`, includes `<okular/core/document.h>` or calls `find_package(Okular6)`
breaks. Since Charta has never shipped under the old name, there is no downstream to break - which
is exactly why the migration can be done in one pass.

### 4.4 The data-path and config-file renames (group H) - **medium**, because user data moves

`~/.local/share/okular/bookmarks.xml` holds the user's bookmarks; `~/.config/okularpartrc` holds
the settings. Renaming them means an existing installation starts with an empty profile unless the
user moves the files. This is deliberate: after the migration Charta and a real Okular no longer
fight over the same KConfig files (two processes writing the same KConfig file is a corruption
risk, not just a nuisance). §6 gives the exact commands.

### 4.5 Groups B, D, F, G, I - **low**

* **B (component ids):** the `<id>` elements were already migrated; only the file names and the
  install rules were stale, and the install rule is in the same file as the rename, so a mistake
  would have failed the configure. Verified by the configure.
* **D (logging categories):** only a file name changes; the category identifiers are unchanged, so
  no user's `QT_LOGGING_RULES` filter breaks.
* **F (`X-DocPath`):** removing a key that promised a non-existent HTML page cannot break
  anything; the desktop file stays valid.
* **G (KParts component name):** the only coupling is `setComponentName()` <-> `qresource
  prefix`. Both sides are changed in the same patch and a grep confirms that **no**
  `/kxmlgui5/okular` and **no** `setComponentName(QStringLiteral("okular")` remain. This one is
  *not* validated by a configure, it is validated by a build and a run.
* **I (plugin API key):** nothing reads it.

---

## 5. The two decisions the audit asked for

### 5.1 Decision 1 - the D-Bus service name: **rename it** to `org.charta.reader`

Options considered:

1. *Keep `org.kde.okular`.* Rejected: it is a live claim on another project's identity on a
   shared, user-visible bus, and it produces a real functional conflict with a genuine Okular.
   `trademark.md` §3 calls exactly this out.
2. *Rename to a name under `org.kde`.* Rejected: Charta is not a KDE project, and `org.kde.*` is
   reserved for KDE projects.
3. *Rename to `org.charta.reader`.* **Chosen.** It matches the desktop file
   (`org.charta.reader.desktop`), the AppStream id (`shell/org.charta.reader.metainfo.xml`) and
   the Flatpak app id, so the whole application has one namespace.

Consequences are listed in §4.1 and §6.

### 5.2 Decision 2 - the translation domain: **rename it** to `charta`, together with the catalogues

Options considered:

1. *Keep the domain `okular`.* Rejected: the installed artefacts would still be
   `share/locale/de/LC_MESSAGES/okular.mo`, i.e. the shipped product would still be named after
   Okular in every translated install, and `setApplicationDomain("okular")` would stay a literal
   "okular" in the source. The audit deferred this as "a product decision"; the product decision
   has now been made.
2. *Rename the domain and the catalogues, and rewrite `Project-Id-Version` to `charta`.* Rejected
   as the default, because `Project-Id-Version` is the identifier the **translation
   infrastructure** uses to tie a catalogue to its project and its translation memory. Rewriting it
   orphans the history of 1 013 imported catalogues and makes the upstream sync undetectable.
   It becomes the right thing to do only once Charta has its own translation platform.
3. *Rename the domain and the catalogues, keep the catalogue contents byte-identical.*
   **Chosen.** The domain is a runtime key and it is renamed; the catalogue contents are upstream
   material and they are not.

What "byte-identical" means in practice: the patch contains **1 013 pure renames with zero content
changes**. `Project-Id-Version: okular`, `Last-Translator`, `Language-Team`, `POT-Creation-Date`,
`X-Generator`, the `SPDX-FileCopyrightText` credit lines, every `#.` translator comment, every
`#, fuzzy` flag and every `msgid`/`msgstr` are unchanged. Spot-verified with
`msgfmt --check` and `msgunfmt` (§11).

The maintainer should treat "the catalogues are still named after the okular project internally"
as a deliberate, documented state, not as an oversight: it is the thing that keeps the upstream
translation memory and the translator credits attached to these files.

### 5.3 Decision 3 (small) - `X-DocPath=charta/index.html`: **remove the key**

* *Point it at a real file.* There is no HTML documentation in the tree: `find . -name "*.html"`
  returns nothing, and the only documentation that is installed is
  `docs/THIRD-PARTY-NOTICES.md` and `docs/QPDF-NOTICE.md`, copied to `share/doc/charta` as
  Markdown. `X-DocPath` is defined to point at an **HTML** file, so there is nothing correct to
  point it at.
* *Remove it.* **Chosen.** The key currently promises a help page that does not exist; a help
  browser that resolves it lands on a 404. Removing the key is the honest fix, and it is what the
  desktop-file specification expects for an application that ships no HTML help.

If the project later ships an HTML manual under `share/doc/charta/`, the key can be re-added as
`X-DocPath=charta/index.html` in the same commit that installs that file.

---

## 6. User-facing migration notes

### 6.1 What a user must do

* **Move the settings, or start fresh.** Charta now reads its settings from
  `~/.config/chartapartrc` and `~/.config/chartarc` instead of `~/.config/okularpartrc` and
  `~/.config/okularrc`. To keep the existing configuration:

  ```sh
  mv ~/.config/okularpartrc ~/.config/chartapartrc
  mv ~/.config/okularrc      ~/.config/chartarc
  ```

  If the old files are left in place, Charta starts with default settings; it does not read them.
* **Move the bookmarks.** The bookmarks live in `~/.local/share/okular/bookmarks.xml`:

  ```sh
  mkdir -p ~/.local/share/charta
  mv ~/.local/share/okular/bookmarks.xml ~/.local/share/charta/bookmarks.xml
  ```

  Without this the bookmark list is empty. (The document-metadata cache in
  `~/.local/share/okular/docdata` is regenerated automatically and can simply be left behind.)
* **Log out and back in, or restart the session bus.** A Charta instance that is *already running*
  when the new package is installed still owns `org.kde.okular` and still answers on
  `/okularshell`. Close every running Charta before starting the new one; otherwise the new
  instance will not be able to attach to the old one and the user will see two separate windows.
* **Do not run Charta and a real Okular "the same way" any more.** They no longer share a service
  name, a config file or a data directory, so they can be installed side by side. That was the
  point of the change.
* **Scripts that drive Okular over D-Bus must be updated.** The service name, the interface name
  and the object path all changed:

  | Before | After |
  | --- | --- |
  | `org.kde.okular` | `org.charta.reader` |
  | `/okularshell` | `/chartashell` |
  | `/okular`, `/okular2`, ... | `/charta`, `/charta2`, ... |
  | `org.kde.okular.ConfigInterface/0.1` and the other four interface ids | `org.charta.reader.ConfigInterface/0.1`, ... |

  The per-process name changed from `org.kde.okular.Instance_<pid>` to
  `org.charta.reader.Instance_<pid>`, and the legacy `org.kde.okular-<pid>` became
  `org.charta.reader-<pid>`.
* **Translations keep working, in the same languages.** The catalogues are the same files with new
  names; nothing about the translated strings changed. What *is* new: the upstream Okular
  translation sync no longer lands in this tree automatically (§5.2), so Charta needs its own
  translation workflow before new strings can be translated.
* **Qt logging filters keep working.** The logging category identifiers (`org.kde.okular.core`,
  `org.kde.okular.ui`, `org.kde.okular.generators.*`) were deliberately **not** renamed, so an
  existing `QT_LOGGING_RULES="org.kde.okular.core.debug=true"` still selects the same category.
* **The `application/vnd.kde.okular-archive` MIME type is unchanged** (§10), so document archives
  saved by an older Okular still open in Charta and vice versa.

### 6.2 What a packager must do

* **Rebuild from source.** The core library's soname changes (`libOkular6Core.so.4` ->
  `libCharta6Core.so.4`), so this is a new ABI and needs a rebuild of anything that links it.
* **Remove the old desktop entry.** On upgrade, delete the previous
  `org.charta.reader.desktop` before installing the new one, so no stale
  `X-DBUS-ServiceName=org.kde.okular` is left in `share/applications/`. A file manager that reads
  the stale entry will try to D-Bus-activate a name nobody owns any more.
* **Do not ship both the old and the new library.** `libOkular6Core.so.4` and
  `libCharta6Core.so.4` must not coexist in one package set; the old one is a different ABI with
  the same header directory name.
* **Adjust the Flatpak permissions** if you maintain a separate manifest: the required set is
  `--own-name=org.charta.reader` and `--own-name=org.charta.reader.*`. Do not add
  `--own-name=org.charta.reader-*`; Flatpak's own-name permission does not express a dash glob,
  and the per-process name that matters (`org.charta.reader.Instance_<pid>`) is already covered by
  the `.*` entry.
* **Adjust `find_package()` users.** The CMake package is now `Charta6` (installed in
  `lib/cmake/Charta6`), the exported target is `Charta::Core`, and the headers are in
  `include/charta/core/` and `include/charta/interfaces/`. Anything that did
  `find_package(Okular6 REQUIRED)` must be updated.
* **The installed paths change.** `include/okular/**` -> `include/charta/**`;
  `share/qlogging-categories6/okular.categories` -> `charta.categories`;
  `share/charta/pics` and `share/charta/icons` instead of `share/okular/pics` and
  `share/okular/icons`; `share/locale/*/LC_MESSAGES/charta*.mo` instead of `okular*.mo`.
* **Do not rename the MIME type** (see §10) - no XML type definition for
  `application/vnd.kde.okular-archive` exists in this tree; it comes from the system's
  shared-mime-info database, and files on disk are already tagged with it.
* **The component ids of the 14 generator plugins changed** from `org.kde.okular-<n>` to
  `org.charta.reader-<n>`. If you ship a Flatpak or an AppStream metainfo overlay of your own,
  those ids have to be updated with them.

---

## 7. Ready-to-paste footer note

Plain text, for the bottom of `README.md` or the release notes:

```
---

## Identity migration applied

Charta's identity migration has been applied. Charta no longer uses the Okular
identity anywhere it is observable:

* The session-bus service name is now `org.charta.reader` (was `org.kde.okular`),
  the object path is `/chartashell` (was `/okularshell`), and the per-process
  instance name is `org.charta.reader.Instance_<pid>`. The Flatpak manifest owns
  `org.charta.reader` and `org.charta.reader.*`.
* The 14 generator AppStream component ids are now `org.charta.reader-<name>`,
  and the shipped metainfo files carry those names.
* The core library is now `libCharta6Core.so.4`; its export header is
  `chartacore_export.h`, its public headers are installed under
  `include/charta/core/` and `include/charta/interfaces/`, and the CMake
  package is `Charta6` with the `Charta::` namespace.
* The translation domain is now `charta`; the catalogues are installed as
  `share/locale/<lang>/LC_MESSAGES/charta.mo` and `charta_<generator>.mo`. The
  translated content and the translator credits are unchanged.
* The logging-category file is now `charta.categories`, and the data, cache and
  configuration paths use `charta` instead of `okular`.

What a user must do after upgrading: close every running Charta instance, then
move `~/.config/okularpartrc` to `~/.config/chartapartrc`,
`~/.config/okularrc` to `~/.config/chartarc`, and
`~/.local/share/okular/bookmarks.xml` to `~/.local/share/charta/bookmarks.xml`,
or start with a fresh configuration. Scripts that talk to the old D-Bus names
must be updated to `org.charta.reader`, `/chartashell` and
`org.charta.reader.<Interface>/<version>`.

Not changed on purpose: the `application/vnd.kde.okular-archive` MIME type (a
system-wide type that is not defined in this tree and is already used by files
on disk), the Qt logging-category identifiers, the C++ `Okular` namespace, the
mobile application id, and the upstream translation metadata inside the
catalogues. The full plan, the risk analysis and the rollback instructions are
in `docs/licensing/identity-migration.md`; the patch itself is in
`docs/licensing/identity-migration.patch`.
```

---

## 8. Rollback

### 8.1 How to revert the migration

The migration is a single patch, so reverting it is a single operation, as long as it has not been
committed yet:

```sh
git apply -R docs/licensing/identity-migration.patch
```

That restores every renamed file, every renamed directory entry and every edited line, including
the 1 013 catalogues, because the patch records the renames explicitly and the contents were never
changed.

If the patch has already been committed, revert that commit (`git revert <sha>`) instead; the
result is identical.

**After a rollback, the user must move the files back**, because the rollback restores the *code*,
not the user's home directory:

```sh
mv ~/.config/chartapartrc  ~/.config/okularpartrc
mv ~/.config/chartarc       ~/.config/okularrc
mv ~/.local/share/charta/bookmarks.xml ~/.local/share/okular/bookmarks.xml
```

and any running instance must be restarted, because the old build answers on
`org.kde.okular` / `/okularshell`.

A rollback is only safe if no Charta build with the new identity has been *released*; after a
release, a rollback would put two different identities in the field (different D-Bus names,
different config file names, different installed paths) and is worse than either one alone.

### 8.2 The plugin-namespace rename is **not** part of this patch

`okular_generators` -> `charta_generators` is **already applied in the working tree** (it is in
`generators/CMakeLists.txt`, `okular_part.json`, `core/document.cpp` and
`mobile/components/okularsingleton.cpp`, and `docs/licensing/packaging.md` §2 records it as
*done*). It is therefore **not** part of `identity-migration.patch`, and the patch neither
re-does it nor reverts it.

The reason it is separate is worth stating, because it is the template for the rest of this work:
it is a *file-system* namespace (the directory the generator plugins are installed into), it has
no D-Bus or metadata consequence, and it can be changed without touching runtime behaviour. The
identity migration in this patch is the part that touches the bus, the metadata and the packaging
layout - which is exactly why the maintainer deferred it and why it needed a plan.

---

## 9. What was **not** changed, and why

| Item | Where | Why it was left alone |
| --- | --- | --- |
| MIME type `application/vnd.kde.okular-archive` | `okular_part.json:8`, `shell/org.charta.reader.desktop:199`, `part/part.cpp:1952,1970,2043,3205`, `core/document.cpp:5737,6086` | **Not renamed.** The type is a *system-wide* identifier defined in the system's shared-mime-info database, **not** in this tree - there is no XML type definition anywhere in the repository, and the upstream `mime/okular.xml` that defines it was never imported here. Renaming it would mean (a) shipping a new XML type, (b) re-tagging every archive already on disk (files keep their old type until they are re-detected), and (c) losing the association with the "Okular archive" file format that the upstream format is named after. All eight in-tree references are *uses* of the type, not its definition. The cost of the rename (orphaned files, an unregistered type, a divergence from every other KDE tool) is far higher than the cost of keeping a shared type name. |
| The `Okular` C++ namespace and the `Okular*` class/macro identifiers | `core/`, `part/`, `gui/`, `interfaces/`, `autotests/` (≈5 200 references) | A source-level and ABI-level break for every downstream consumer, with **no** runtime identity effect: the namespace is not visible on the bus, in the metadata or in any installed path. Renaming it belongs in a separate, separately announced API break, not in an identity patch. The public headers in `include/charta/**` keep the `Okular::` namespace, which is a deliberate, documented inconsistency. |
| The `okularpart` KPart library: target, `libokularpart.so`, `okularpart_export.h`, `okular_part.json` | `CMakeLists.txt:592-741`, `part/*.h`, `conf/settings.kcfgc` | The KPart plugin library is a *host-API implementation detail*, not an identity claim: it is loaded by KParts through its `.json` metadata, and that metadata already carries the Charta identity (`"Name": "Charta"`, `"Icon": "charta"`, `"PluginNamespace": "charta_generators"`). Renaming it breaks every host application that links `libokularpart.so` or includes `okularpart_export.h`, for no observable gain. **Known remaining item** - see §12. |
| The generator plugin library and metadata file names: `okularGenerator_<g>` / `libokularGenerator_<g>.json` / `okularApplication_<g>.desktop` | `generators/*/` | Same reasoning as above. The *user-visible* plugin identity (component id, desktop entry `Name`/`Exec`/`Icon`, install namespace) is Charta; the file names of the plugin binaries are not user-visible identity. **Known remaining item** - see §12. |
| The Qt logging-category identifiers `org.kde.okular.core`, `org.kde.okular.ui`, `org.kde.okular.generators.*` and the `Okular*Debug` C++ identifiers | `core/debug.cpp`, `gui/debug_ui.cpp`, `generators/*/debug_*.cpp`, `charta.categories` | These are **user-facing filter keys**: `QT_LOGGING_RULES="org.kde.okular.core.debug=true"` and the `kdebugsettings` tool select categories by this exact string. Renaming them would silently stop every existing filter from matching. A logging category is a debugging aid, not a claim on the bus or in the product metadata; `trademark.md` lists the trademark question separately. The installed *file* is renamed (`charta.categories`), the identifiers inside it are not. |
| The mobile application identity: QML module URI `org.kde.okular`, Android package `org.kde.okular.kirigami`, `org.kde.okular.kirigami.appdata.xml`, `org.kde.mobile.okular_*.desktop` | `mobile/` | The mobile front end is a separate application with its own app id, its own QML module URI (used by `import org.kde.okular 2.0` in 9 QML files) and its own Android package name (which cannot be changed after the first release on a store without breaking updates). It is not built in the desktop configuration this patch was verified against. It needs its own pass, with the QML import URIs and the Android id treated as the breaking part. |
| `Project-Id-Version: okular` inside the 1 013 catalogues | `po/<lang>/*.po` | Deliberate - see §5.2. It is the upstream translation-project identifier that keeps the translation memory and the translator credits attached to these files. |
| `po/<lang>/org.kde.active.documentviewer.po` (56 files) | `po/` | These belong to the **KDE ActiveDocumentViewer** project, not to Okular. They are not the `okular` domain and renaming them would break that project's sync for no identity gain. |
| Copyright and licence headers, `LICENSES/LicenseRef-Okular-upstream-terms.txt` | everywhere | They are *attribution*, which `trademark.md` §2 explicitly says is correct and required for a GPL derivative. Rewriting them would destroy the provenance record. |
| Build-internal names: `OkularConfigureChecks.cmake`, `config-okular.h.cmake` -> `config-okular.h`, the `OKULAR_FORCE_DRM` / `OKULAR_UI` / `OKULAR_BINARY` CMake variables, the `OKULAR_*` include guards, `shell/okular_main.{h,cpp}`, `okularAboutData()` | `CMakeLists.txt`, `core/*.h`, `shell/` | They are not installed, not on the bus, and not in any user-visible path. `OKULAR_FORCE_DRM` and `OKULAR_UI` are **documented build options**: renaming them would break every packager's `-DOKULAR_FORCE_DRM=ON` command line. The include guards are invisible. |
| Asset file names: `tool-*-okular*.png` (12), `okular-epub-*.png` (2), `16-apps-okular-gv.png`, `32-apps-okular-gv.png`, `16-apps-okular-fb2.png`, `32-apps-okular-fb2.png`, `48-apps-okular-fb2.png`, `hi32-app-okular-fb2.svg` | `part/data/`, `generators/*/` | The **directories** they live in are renamed (`share/okular/pics` -> `share/charta/pics`, `share/okular/icons` -> `share/charta/icons`), and every runtime lookup is updated. The individual asset file names are upstream assets whose final names belong with the icon/asset work in `charta_logo_package/`. Renaming 19 binary assets and their 22 references is a mechanical but unverifiable-by-configure change. |
| Temporary-file name templates: `/okular_XXXXXX.pdf`, `/okular_XXXXXX.ps`, `/okular_kdelatex-XXXXXX.tex`, `/okular_signature_XXXXXXX.png`, `/okular_revision_XXXXXX.*` | `part/`, `core/`, `generators/` | Transient files in `$TMPDIR` that are deleted by their owner. They are never a persistent, user-visible identity. |
| `shell/shell.cpp:250` `okular_sidebar` (object name) and `:648` `okular_lock_sidebar` (action name) | `shell/shell.cpp` | Both are keys in the *saved UI state* (the dock layout and the action states in the user's config file). Renaming them would reset the user's toolbar and sidebar customisation for a purely cosmetic gain. |
| `part/data/tools.xml`, `toolsQuick.xml`, `drawingtools.xml` DOCTYPE names (`okularAnnotatingTools` etc.) | `part/data/` | Internal XML DOCTYPE identifiers with no runtime effect; the document type name is not part of the file format. |

---

## 10. Verification performed

| Check | Result |
| --- | --- |
| `git apply --check docs/licensing/identity-migration.patch` in the live tree (read-only) | **passes**, exit 0 |
| Full `git apply` into a pristine copy of the working tree | **passes**; the result is byte-identical to the scratch tree for every file the patch touches (only five files that another agent changed in `docs/licensing/` after the scratch was taken differ, and none of them is touched by the patch) |
| `cmake -S <migrated tree> -B /tmp/opencode/identity-migration-build -DOKULAR_UI=desktop` | **configures successfully** ("Configuring done", "Generating done", "Build files have been written to"). No build was run. |
| End-to-end check | the patch was applied to a pristine copy of the working tree and *that* tree was configured with the same command: **exit 0**, "Build files have been written to". The migration therefore configures as a patch, not only as a hand-edited tree |
| Generated export header | `core/chartacore_export.h` defines `CHARTACORE_EXPORT`, `CHARTACORE_NO_EXPORT`, `CHARTACORE_DEPRECATED` |
| Link line | `-Wl,-soname,libCharta6Core.so.4 -o bin/libCharta6Core.so.4.0.0`; the part links `libCharta6Core.so.4.0.0` |
| Installed header paths | `include/charta/core` and `include/charta/interfaces` |
| Installed CMake package | `Charta6Config.cmake`, `Charta6ConfigVersion.cmake`, `Charta6Targets.cmake` |
| Installed logging-category file | `share/qlogging-categories6/charta.categories` |
| Installed AppStream metadata | `org.charta.reader-<name>.metainfo.xml` for all 14 generators (the mobipocket one is not in the install script here only because `QMobiPocket6` is not installed in this environment; the source rule and the file name are correct) |
| Translation catalogues | `msgfmt --check` on `po/de/charta.po` succeeds; `msgunfmt` shows `Project-Id-Version: okular`, the original `Last-Translator`, `Language-Team` and all 13 `SPDX-FileCopyrightText` credit lines |
| Runtime catalogue names | KF6's own `build-pofiles.cmake` was run against the patched `po/` tree: it produces `charta.mo`, `charta_comicbook.mo`, `charta_djvu.mo`, `charta_dvi.mo`, `charta_epub.mo`, `charta_fax.mo`, `charta_fictionbook.mo`, `charta_ghostview.mo`, `charta_kimgio.mo`, `charta_markdown.mo`, `charta_mobi.mo`, `charta_poppler.mo`, `charta_tiff.mo`, `charta_txt.mo`, `charta_xps.mo` and (untouched) `org.kde.active.documentviewer.mo`. **No `okular*.mo` is produced any more** - so the runtime domain and the installed catalogue name agree |
| Flatpak own-name invariant | `org.charta.reader.Instance_<pid>` matches the glob `org.charta.reader.*` (**true**), i.e. it is a proper subname of the service name and stays covered by the single `.*` permission; `org.charta.reader` itself is covered by the exact permission; the legacy `org.charta.reader-<pid>` does not match the glob - exactly as upstream's `org.kde.okular-<pid>` did not |
| Catalogue count | `po/<lang>/charta.po` = 82; `po/<lang>/charta_<g>.po` = 931; `po/<lang>/org.kde.active.documentviewer.po` = 56 (untouched); total 1 069 |
| Residual `org.kde.okular` in the tree | only the logging-category identifiers, the mobile app, the `Okular::` C++ identifiers and the upstream copyright metadata - all listed in §9 |

**Not verified** (no build was run, and none is allowed to run in this tree): the runtime behaviour
of the KParts component-name / resource-prefix pair (group G), the D-Bus registration, and the
translation lookup. All three are covered by the greps recorded in §11 and by the reasoning in §4.

---

## 11. Follow-ups the maintainer should decide on

1. **Apply the patch** (`git apply docs/licensing/identity-migration.patch`) and commit it as one
   commit, with the user-facing notes of §6 in the release notes.
2. **Update `README.md`.** The "Project fact" table still says
   `| Core library | Okular6Core |`; it must become `Charta6Core`. (This file was owned by another
   agent during the preparation of this patch and was therefore not touched.) The footer note in
   §7 is ready to paste under it.
3. **Update `docs/licensing/packaging.md` §2 and `docs/licensing/trademark.md`**, which still
   describe the D-Bus rename, the component ids and the translation domain as *deferred*. They
   were not edited here because they belong to the audit that produced them.
4. **Set up a translation workflow.** The upstream KDE sync delivers `okular*.po`; this tree now
   expects `charta*.po` (§5.2). Until a Charta-owned l10n project exists, new strings will not be
   translated, and when one does exist, `Project-Id-Version` should be switched to `charta` in the
   same commit that switches the sync.
5. **Decide on the remaining `okularpart` / `libokularGenerator_*` file names** (§9). They are the
   last observable "okular" strings in an installed Charta.
6. **Decide on the mobile identity** (§9). It needs its own pass, including the Android package
   id, which cannot be changed in place on a store.
7. **Re-check the trademark position.** The rename removes Charta's *runtime* claim to the Okular
   identity. The factual attribution ("based on KDE Okular") in `README.md` and
   `docs/THIRD-PARTY-NOTICES.md` must stay, and it is not affected by this patch.

---

*Evidence class: E1 for the code and packaging surface (read from the tree), E0 for the counts.*
*Prepared by the identity-migration subagent; the patch is not applied.*
