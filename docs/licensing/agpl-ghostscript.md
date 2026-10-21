# Ghostscript / AGPL: the chain, the decision, and the removal (Phase 6)

**Decision:** D11 — **drop `ghostscript` and `libspectre` from the Flatpak**. The shipped bundle
stays `GPL-2.0-or-later`; nothing in the release contains AGPL code.
**Evidence class:** `E0` (commands run locally).
**Date:** 2026-10-21

## 1. The chain, established with commands

```
ldd $(ldconfig -p | awk '/libspectre\.so/{print $NF; exit}') | grep libgs
    ->  libgs.so.10 => /usr/lib/x86_64-linux-gnu/libgs.so.10
```

- `libspectre.so.1` -> `libgs.so.10`. **Ghostscript is AGPL-3.0.**
- `generators/CMakeLists.txt:19` — `if(LIBSPECTRE_FOUND) add_subdirectory(spectre)`: the Spectre
  generator is built whenever libspectre is present.
- `generators/spectre/CMakeLists.txt:24` — `target_link_libraries(okularGenerator_ghostview ...
  ${LIBSPECTRE_LIBRARY} ...)`: the plugin links libspectre, therefore libgs.
- `.flatpak-manifest.json` builds the `ghostscript` module **before** `libspectre`, so the Flatpak
  shipped an AGPL-3.0 binary inside a package labelled GPL-2.0-or-later.

## 2. Why this is a P0 and not a footnote

A distribution that contains AGPL-3.0 code may not be described as GPL-2.0-or-later. The
combination *can* lawfully be distributed under AGPL-3.0 (GPL-2.0-or-later may be upgraded to
GPL-3.0, and AGPL-3.0 section 13 permits combining with GPL-3.0), but then:

- the package's licence label changes everywhere (AppStream `project_license`, the about box, the
  notices, the source offer);
- the four `LGPL-2.0-only` files inside `Okular6Core` (`part/ktreeviewsearchline.*`,
  `part/okmenutitle.*`) have no automatic GPL-3.0 upgrade path, so the upgrade is an argument, not
  a mechanical fact.

## 3. Options considered

| Option | Consequence |
| --- | --- |
| (a) **Drop `ghostscript` + `libspectre` from the Flatpak** | No AGPL in the shipped bundle; PostScript documents unsupported in the Flatpak; source builds may still link libspectre optionally, documented as AGPL |
| (b) Declare the bundle AGPL-3.0 | Relabel everything, ship full AGPL notices and source; the LGPL-2.0-only wrinkle needs a legal opinion |
| (c) Argue a weaker position (e.g. mere aggregation) | Depends on how the plugin is shipped and how the user links it; not a position to take without counsel |

**Decision: (a).** It removes the question from the shipped artefact instead of arguing it, and
the cost (no PostScript in the Flatpak) is smaller than the cost of an AGPL label on the project.

## 4. What Phase 7 does with this

- `.flatpak-manifest.json`: the `ghostscript` and `libspectre` modules are removed.
- The release notes state that PostScript (and therefore the `okularGenerator_ghostview` plugin) is
  unavailable in the Flatpak build.
- The source-build path keeps `LibSpectre` optional (`find_package(LibSpectre)` is not `REQUIRED`),
  with the AGPL note recorded in `docs/licensing/packaging.md`.
- Proof (Phase 9): the built Flatpak contains no `libgs`, and the plugin is absent from the
  generator directory.

## 5. Residual risk

A source build on a machine that has libspectre installed **will** pull in `libgs` at runtime. That
is a property of the *user's* system, not of the distributed bundle, and it is documented — but the
notice must say so explicitly, or a user who builds from source with libspectre will be surprised.
