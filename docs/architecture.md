# Charta architecture and implementation notes

These notes describe the source at commit `0f13a44` (2026-10-21), checked by static inspection
only; no build or runtime tests were run. See [README-VERIFICATION.md](README-VERIFICATION.md)
for the claim-to-evidence summary.

## Source layout

- `core/`: document model and `okularcore`.
- `generators/`: format backends, including the Poppler PDF generator.
- `part/`: KParts UI, page view, annotation tools, autosave and Document Builder.
- `shell/`: the `charta` executable (app ID `org.charta.reader`) and shell-level actions.
- `autotests/`: unit and integration tests registered by CMake.

Charta is substantially based on KDE Okular.

## Annotation pipeline and Poppler patches

The repository carries two Poppler patches:

- `patches/poppler-ink-appearance.patch` adds `setAppearanceStream` for ink annotations. Charta
  uses it to store a pressure-shaped outline in the PDF.
- `patches/poppler-annotation-rotation.patch` adds the `ChartaRotate`, `ChartaLocalW` and
  `ChartaLocalH` entries and appearance handling for rotated annotations.

`generators/poppler/annots.cpp` calls `setAppearanceStream`, `moveAnnotation` and
`setUserRotation` (and reads `userRotation()`) without a feature guard, so the Poppler generator
needs the patched fork. The patch files alone do not guarantee that the running application uses a
Poppler built with them. The README's source build uses the fork. The Flatpak manifest builds
upstream Poppler 26.08.0 with only the ink patch, so rotation is unavailable there and the build
is not expected to succeed until the manifest is updated and tested.

Rotation state is applied in `core/document.cpp` and undone through `RotateAnnotationCommand` in
`core/documentcommands_p.h`.

Pressure samples shape the stroke during the editing session. The saved appearance stream keeps
the rendered outline, not editable per-point pressure, so a reopened PDF does not restore the
original samples.

Tests: `annotationrotationtest` covers frame geometry, undo/redo and XML round trips.
`annotationpersistencetest` saves and reopens PDFs through Poppler and is registered only when
Poppler Qt6 is found. Neither was run for this revision.

## Saving page-structure changes

`generators/poppler/generator_pdf.cpp` writes added, removed and reordered pages with QPDF when
`HAVE_QPDF` is set. Otherwise it needs `pdfseparate` and `pdfunite` and reports an error if they
are missing. CMake looks for QPDF via CMake config, then pkg-config (`libqpdf`), then fetches
v11.9.1 with FetchContent. Replacing the PDF after a page edit is not guaranteed to be atomic, and
links or named destinations may point to deleted pages.

## Document Builder export (`part/documentbuilderexport.cpp`)

The exporter writes to a temporary file in the destination directory, reopens it with QPDF,
compares the page count, then replaces the destination. On `Q_OS_UNIX` the replacement is
`::rename`; elsewhere it is remove-then-rename (non-atomic). Errors remove the temporary file.
An output path that resolves to a source file is refused, except for the explicit Save action,
which replaces the builder's seed document after confirmation. Only page content is copied:
outlines, form fields, attachments, metadata, signatures and encryption are not carried over.

## Autosave (`Part::canAutoSaveNow`, `Part::slotAutoSave`, `part/part.cpp`)

`canAutoSaveNow` refuses when a save is in progress, the part is read-only or unmodified, the
document is untitled, print-preview, password-opened or non-local, cannot save changes, would lose
data in its format, or its on-disk modification time differs from `m_fileLastModified`.
`slotAutoSave` additionally retries later (2.5 s) while a mouse button is held, within 2 s of the
last user input, or while a modal dialog is open.

Autosave shares the `Ctrl+S` path, so it is blocked for untitled documents to avoid opening a
Save dialog unprompted.

## Tests

Charta-specific tests registered in `autotests/CMakeLists.txt` cover Document Builder, blank-page
writing, untitled documents, width presets, annotation colour history, the annotation toolbar,
the main shell, annotation rotation, persistence and clipboard. Optional dependencies and build
options change the registered set. See the README for the test command and the verification
document for what remains unrun.
