# Packaging and identity (Phase 7)

**Evidence class:** `E0` for the manifest and metadata edits; `E1` for the code surface.
**Date:** 2026-10-21
**Decisions applied:** D4/D12 (rename where safe), D11 (drop AGPL modules), D2 (licence label).

## 1. What changed in this phase

| File | Change | Why |
| --- | --- | --- |
| `.flatpak-manifest.json` | removed the `ghostscript` and `libspectre` modules | D11: no AGPL code in the shipped bundle |
| `.flatpak-manifest.json` | the `poppler` module now applies **three** patches in order | the old manifest applied one stale patch and none of the code that introduces `setUserRotation` |
| `.flatpak-manifest.json` | removed `"/share/doc"` from `cleanup` | the cleanup list **deleted** `/share/doc` from the built image, so the notices installed there would have been stripped. This was a live packaging bug. |
| `.flatpak-manifest.json` | the `charta` module no longer applies a patch | the patch targeted `shell/org.kde.okular.desktop`, a file that does not exist; its content (the MIME list) is now in the desktop file itself, so both builds get the same result |
| `shell/org.charta.reader.desktop` | full MIME type list | replaces the content of the dead patch; both build paths now agree |
| all 15 `*.metainfo.xml` | `project_license` `GPL-2.0+ and GFDL-1.3` -> `GPL-2.0-or-later AND GFDL-1.2-or-later` | `GPL-2.0+` is deprecated SPDX syntax and there is no GFDL-1.3 text in the repository; `GFDL-1.2-or-later` is present in `LICENSES/` |
| `aboutdata.h` | added the Charta maintainer (Rupam Dutta) and a note that the code is AI-assisted | D7/D9; the inherited copyright lines are untouched |
| `okular_part.json`, `generators/CMakeLists.txt`, `core/document.cpp`, `mobile/components/okularsingleton.cpp` | plugin namespace `okular_generators` -> `charta_generators` | removes the plugin-directory collision with a distro Okular install (§2.10 ABI item); both the installer and the runtime lookup are changed together |
| `LICENSES/CC0-1.0.txt`, `LICENSES/LGPL-2.1-only.txt` | added | the licence fields and the Drawy notice now reference texts that exist |
| `shell/CMakeLists.txt` | install rules for `LICENSES/` and `docs/THIRD-PARTY-NOTICES.md` | a GPL distribution must convey the licence; the QPDF Apache-2.0 NOTICE is assembled into the same place at release time |
| `.flatpak-patches/okular-mimetypes.patch` | **deleted** | it could not apply (target file does not exist) and its content now lives in the desktop file |

## 2. The identity surface: what was renamed and what was not

Renaming the D-Bus identity is a **coordinated code change**, not a metadata edit. The service name
appears in six places that must agree:

| Place | Current | Action |
| --- | --- | --- |
| `shell/shell.cpp:294` | registers `org.kde.okular` | **deferred** |
| `shell/shell.cpp:302` | per-process name `org.kde.okular-<pid>` | **deferred** |
| `shell/shell.cpp:397` | unregisters `org.kde.okular` | **deferred** |
| `shell/okular_main.cpp:59,118` | `QDBusInterface("org.kde.okular", "/okularshell", "org.kde.okular")` | **deferred** |
| `shell/shell.h:49`, `part/part.h:121` | `Q_CLASSINFO("D-Bus Interface", "org.kde.okular")` | **deferred** |
| `shell/shellutils.h:41` | `kPerProcessDbusPrefix = "org.kde.okular.Instance_"` | **deferred** |
| `shell/org.charta.reader.desktop:200` | `X-DBUS-ServiceName=org.kde.okular` | **deferred** |
| `.flatpak-manifest.json` | `--own-name=org.kde.okular`, `--own-name=org.kde.okular.*` | **deferred** (must follow the code) |
| `part/data/sources`, `okular_generators` | plugin namespace | **done** -> `charta_generators` |
| `application/vnd.kde.okular-archive` | MIME type | **kept** (a system MIME type name; renaming it means a new XML type plus desktop updates, and other software references it) |
| 15 `org.kde.okular-*.metainfo.xml` file names and `<id>`s | Okular component ids | **deferred** (they are upstream component identifiers; renaming them touches every generator's install rules and the plugin metadata) |
| `po/` catalogue metadata and the `okular` translation domain | Okular | **deferred** (D3: product decision, not a licence issue) |

**Why the D-Bus rename is deferred rather than done here.** The per-process D-Bus name must be a
subname of the service name (the comment in `shell/shellutils.h` says this explicitly, because
Flatpak cannot express wildcard own-name permissions). Renaming therefore touches the code, the
desktop file, the Flatpak permissions and any external tool that talks to `org.kde.okular`
(`/okularshell`). It also changes the D-Bus activation path used by file managers. Doing it in the
same pass as a licensing cleanup — while another agent is editing the same tree — is the kind of
change that breaks a working application for an identity reason.

**Update (2026-10-21):** the maintainer confirmed that "Charta" is the final public name and
**deferred** the application of the migration. The complete migration is now prepared as a
reviewable patch and plan:

* `docs/licensing/identity-migration.patch` — 1 137 files (1 029 renames, 108 edits), verified
  with `git apply --check` against the current tree; a CMake configure of the patched tree succeeds
  and produces `libCharta6Core.so.4`, `include/charta/**` and `Charta6Config.cmake`.
* `docs/licensing/identity-migration.md` — every site, the risks, the user and packager migration
  notes, the rollback note, and the ready-to-paste footer note.

Two findings from that preparation that this document did not have:

* the `po/` tree is **1 069 `.po` files, of which only 82 are the `okular` domain**; the other 987
  are 14 generator domains plus KDE's ActiveDocumentViewer catalogues. Renaming the domain
  therefore means renaming the main domain **and** the 14 generator domains, or `okular*.mo`
  files would be left behind in `share/locale`.
* beyond the sites listed above, the sweep found the part's own D-Bus paths (`/okular[N]`), the
  `setComponentName` and `/kxmlgui5/` resource prefixes, `share/okular`, the config files
  (`~/.config/okularrc`, `okularpartrc`), the data directories (`~/.local/share/okular`,
  `~/.cache/okular`) and `X-KDE-okularAPIVersion`.

**One consequence to accept now:** until that rename happens, Charta still registers Okular's
D-Bus service name, so a Charta instance and an Okular instance on the same session bus can
conflict. The per-process suffix (`org.kde.okular-<pid>`) limits, but does not remove, that.

## 3. The Flatpak dependency set after this phase

15 modules (was 17):

```
gnustep-make, gnustep, unarchiver, discount, boost, libzip, ebook-tools, djvulibre,
poppler-data, poppler, inih, exiv2, libkexiv2, kdegraphics-mobipocket, charta
```

Removed: `ghostscript`, `libspectre`.

**User-visible consequence:** the Flatpak no longer renders PostScript documents, and the
`okularGenerator_ghostview` plugin is not built. This must appear in the release notes.

**Residual:** a *source* build on a machine with libspectre installed still pulls in `libgs` at
runtime through the plugin. That is a property of the user's system, not of a distributed bundle,
and it is documented in `docs/licensing/agpl-ghostscript.md`.

## 4. Still open in packaging

| Item | Status | Who |
| --- | --- | --- |
| D-Bus service/interface rename (8 places) | deferred, list above | you, as a coordinated change |
| 15 generator `metainfo` ids | deferred | you |
| `X-DocPath=charta/index.html` points at a path that does not exist in the tree | noted, not changed | you (needs a real docs URL) |
| `--filesystem=host`, `--share=network`, `--socket=cups`, `--allow=bluetooth` in `finish-args` | not reviewed in this pass | you |
| `flatpak-builder` is not installed here | the Flatpak build could not be executed | environment |
