# Charta

Charta is an early usable prototype intended primarily for studying and annotating PDFs. It is substantially based on KDE Okular, but it is not an official KDE or KDE Okular release. Some annotation behavior was adapted from KDE Drawy.

AI tools were used to generate or modify essentially all of Charta's implementation under the direction of its project creator and maintainer, who integrated and tested the work. AI systems are disclosed as tools, not as authors or copyright holders.

## Attribution and Provenance

Charta retains applicable upstream components, copyright notices, and license terms, including those from KDE Okular. Existing upstream notices remain with their respective files.

Some annotation behavior, including pressure mapping and seven-point stroke smoothing, was reimplemented or adapted from [KDE Drawy](https://invent.kde.org/graphics/drawy), particularly `src/controller/controller.cpp` and `src/item/freeform.cpp`. The exact Drawy source snapshot and license applicable to this adaptation are not established in the project records and must be resolved before release.

## Features

- PDF reading, zoom, and navigation
- Freehand handwriting, highlighting, and erasing
- Line and arrow annotations
- Selecting, moving, and resizing annotations

## Build

The desktop build uses CMake, C++20, Qt 6, KDE Frameworks, ECM, and Zlib. Optional dependencies enable additional document backends. Linux is the only platform build-verified for this prototype; other inherited platform targets are not claimed as supported.

```bash
cmake -S . -B build -DOKULAR_UI=desktop
cmake --build build --target charta --parallel 3
```

To install the configured targets:

```bash
cmake --install build
```

### Poppler

The Flatpak manifest pins Poppler 26.08.0, applies `patches/poppler-ink-appearance.patch`, then builds Charta. The custom patch adds the Poppler Qt6 API used to write Charta's ink-annotation appearance. It is a Charta-specific downstream patch; project history records no upstream source or commit, and the patch has no reliable human-author metadata. Poppler's applicable license and notices remain relevant to the modified Poppler code. Standalone builds that need this appearance integration must build and install Poppler Qt6 with the patch applied before configuring Charta. Point CMake at that installation with `CMAKE_PREFIX_PATH`, for example:

```bash
cmake -S . -B build -DOKULAR_UI=desktop -DCMAKE_PREFIX_PATH=/path/to/patched-poppler
```

The patch is based on the Poppler version pinned by the Flatpak manifest. A stock Poppler build does not provide the custom API.

## Known Limitation

The Text annotation tool is not ready for use and may not work correctly. Some other parts of the application remain prototype-quality, and the UI and behavior may change.

Tablet pressure is kept only for the current session. PDF ink annotations do not preserve per-point pressure, so saved and reopened strokes use the ordinary uniform-width PDF ink representation.

Report issues in the [Charta issue tracker](https://github.com/Rupam3238/Charta/issues).
Source: [github.com/Rupam3238/Charta](https://github.com/Rupam3238/Charta).
