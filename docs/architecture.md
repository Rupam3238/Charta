# Charta developer notes

Facts below were checked against the source at commit `66b15b4`; see
[README-VERIFICATION.md](README-VERIFICATION.md).

## Layout
- `core/` document model and `okularcore`; `generators/` format backends; `part/` the KParts
  UI (page view, annotation toolbar, Document Builder); `shell/` the `charta` executable
  (target `charta`, app ID `org.charta.reader`); `autotests/` tests.

## Saving page-structure changes
`generators/poppler/generator_pdf.cpp` writes added/removed/reordered pages with QPDF when
`HAVE_QPDF` is set. Otherwise it needs `pdfseparate` and `pdfunite` and reports an error if
they are missing. CMake discovers QPDF via CMake config, then pkg-config (`libqpdf`), then
fetches v11.9.1 with FetchContent.

## Document Builder export (`part/documentbuilderexport.cpp`)
Writes to a temporary file in the destination directory, re-opens it with QPDF and compares the
page count, then replaces the destination. On `Q_OS_UNIX` the replacement is `::rename`; on
other platforms it is remove-then-rename (non-atomic). Errors remove the temporary file.
Source files are not opened for writing.

## Autosave (`Part::canAutoSaveNow`, `part/part.cpp`)
Refuses when saving is in progress, the part is read-only or unmodified, the document is
untitled, print-preview, password-opened, non-local, cannot save changes, would lose data in its
format, or its on-disk modification time differs from `m_fileLastModified`.

## Untitled documents
Autosave shares the `Ctrl+S` path, so it is blocked for untitled documents to avoid opening a
Save dialog unprompted.