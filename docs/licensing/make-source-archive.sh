#!/bin/sh
# Build the source archive for a release and check it against the release contract.
#
# Usage: docs/licensing/make-source-archive.sh <output.tar.xz>
#
# The archive is built from the WORKING TREE (not from a commit), because a release candidate may
# contain work that is not committed yet. It therefore contains whatever the working tree contains;
# review the tree first. Committed-only archives are `git archive --prefix=Charta-<version>/ <tag>`.
set -eu

OUT=${1:?usage: make-source-archive.sh <output.tar.xz>}
ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"

echo "creating $OUT from the working tree ..."
tar --exclude-vcs \
    --exclude='./build' \
    --exclude='./.git' \
    --exclude='./CMakeFiles' \
    --exclude='./shutdown.py' \
    --exclude='./test_savedepth.cpp' \
    --exclude='./test_savedepth_simple.cpp' \
    --exclude='./test_savedepth_simple' \
    --exclude='./core/document.cpp.backup' \
    --exclude='*.o' --exclude='*.so' --exclude='*.a' \
    --exclude='./.claude' \
    -cJf "$OUT" .

echo "checking the archive contract ..."
status=0
for required in LICENSE LICENSES/GPL-2.0-or-later.txt LICENSES/LGPL-2.1-only.txt \
                LICENSES/CC0-1.0.txt REUSE.toml CONTRIBUTING.md \
                docs/THIRD-PARTY-NOTICES.md docs/licensing/RELEASE-CHECKLIST.md \
                patches/poppler-ink-appearance.patch \
                patches/poppler-annotation-rotation.patch \
                patches/poppler-annotation-followup.patch; do
    if ! tar tf "$OUT" | grep -qx "./$required"; then
        echo "MISSING from the archive: $required"
        status=1
    fi
done

# The scratch files below are still tracked and scheduled for removal (RELEASE-CHECKLIST C1/C2);
# they are excluded here so that an archive can be produced before the removal happens.
for forbidden in './shutdown.py' './build/' './CMakeFiles/CMakeSystem.cmake' \
                 './test_savedepth_simple' './test_savedepth.cpp' './test_savedepth_simple.cpp' \
                 './core/document.cpp.backup'; do
    if tar tf "$OUT" | grep -qx "$forbidden"; then
        echo "FORBIDDEN in the archive: $forbidden"
        status=1
    fi
done

count=$(tar tf "$OUT" | wc -l)
echo "archive contains $count entries"
if [ "$status" -ne 0 ]; then
    echo "archive contract NOT satisfied" >&2
    exit 1
fi
echo "archive contract satisfied: $OUT"
