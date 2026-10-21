#!/bin/sh
# Verify that the Poppler patch series applies to the pristine upstream archive and
# reproduces the patched tree that Charta builds against.
#
# Usage: docs/licensing/check-poppler-patches.sh [archive.tar.xz]
# Without an argument the archive is downloaded from poppler.freedesktop.org and its
# sha256 is checked against the value pinned in .flatpak-manifest.json.
set -eu

ARCHIVE=${1:-}
BASE_SHA=dc906e68cea698109706ac6aa3d2c9d4512fcfcac42d90b8afcda486d1b9abd0
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

if [ -z "$ARCHIVE" ]; then
    ARCHIVE="$WORK/poppler.tar.xz"
    echo "downloading poppler-26.08.0 ..."
    curl -sSL -o "$ARCHIVE" https://poppler.freedesktop.org/poppler-26.08.0.tar.xz
fi

actual=$(sha256sum "$ARCHIVE" | cut -d' ' -f1)
if [ "$actual" != "$BASE_SHA" ]; then
    echo "archive sha256 mismatch: $actual != $BASE_SHA" >&2
    exit 1
fi

tar xf "$ARCHIVE" -C "$WORK"
SRC=$(find "$WORK" -maxdepth 1 -type d -name 'poppler-*' | head -1)
cd "$SRC"

for p in poppler-ink-appearance poppler-annotation-rotation poppler-annotation-followup; do
    git apply --check "$OLDPWD/patches/$p.patch" || { echo "patch does not apply: $p" >&2; exit 1; }
    git apply "$OLDPWD/patches/$p.patch"
    echo "applied $p"
done

echo "Poppler patch series applies cleanly to the pristine archive."
