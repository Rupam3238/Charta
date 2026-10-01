# Charta

Charta is a desktop document viewer and annotator for PDF and other document formats.

## Build

Charta uses CMake and Qt/KDE Frameworks. Configure and build the desktop application with:

```bash
cmake -S . -B build -DOKULAR_UI=desktop
cmake --build build --target charta
```

To build and install the configured targets:

```bash
cmake --build build
cmake --install build
```

The existing `OKULAR_UI` CMake option selects `desktop`, `mobile`, or `both` builds.
That internal option name is retained for compatibility.

Report bugs and feature requests in the [Charta issue tracker](https://github.com/Rupam3238/Charta/issues).
Source code: [github.com/Rupam3238/Charta](https://github.com/Rupam3238/Charta).
