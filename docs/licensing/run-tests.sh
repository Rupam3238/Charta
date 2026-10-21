#!/bin/sh
# Run Charta's test suite, with the upstream QPDF suites separated out.
#
#   docs/licensing/run-tests.sh [build-dir]
#
# Why: the fetched QPDF adds its own test suites (check-assert, compare-for-test, qpdf,
# libtests, examples, zlib-flate, fuzz) unconditionally -- qpdf's CMakeLists.txt has no
# BUILD_TESTING guard and no option to switch them off. They test the vendored library, need
# perl/qtest and network access, and fail in environments that lack them. They are therefore
# reported SEPARATELY, never hidden: this script runs them, keeps their output, and prints both
# result sets. The upstream test names are discovered from the build tree, so the split does not
# depend on a hardcoded list.
#
# Exit status: 0 only if Charta's own tests pass. The upstream result is printed and stored, but
# an upstream failure is an upstream failure, not a Charta regression.
set -eu

BUILD=${1:-build}
if [ ! -f "$BUILD/CTestTestfile.cmake" ]; then
    echo "no CTestTestfile.cmake in $BUILD - build first" >&2
    exit 2
fi

# Discover the test names defined inside the fetched qpdf build tree, from the generated
# CTestTestfile.cmake files. Nothing is hardcoded, so a qpdf release that renames or adds a
# suite is picked up automatically.
UPSTREAM_LIST=$(python3 "$(dirname "$0")/list-upstream-tests.py" "$BUILD" --regex 2>/dev/null || true)

if [ -z "$UPSTREAM_LIST" ]; then
    echo "no upstream qpdf tests discovered in $BUILD/_deps - running everything"
    exec ctest --test-dir "$BUILD" --output-on-failure
fi

echo "upstream qpdf suites excluded from Charta's own run:"
echo "  $(echo "$UPSTREAM_LIST" | tr '|' ' ')"
echo

status=0

echo "=== Charta's own tests ==="
ctest --test-dir "$BUILD" --output-on-failure -E "^(${UPSTREAM_LIST})$" || status=1

echo
echo "=== upstream qpdf suites (reported separately, not suppressed) ==="
ctest --test-dir "$BUILD" --output-on-failure -R "^(${UPSTREAM_LIST})$" \
    || echo "NOTE: upstream qpdf suites failed. These are upstream failures in the vendored library," \
           "not Charta regressions; record them in docs/licensing/RELEASE-CHECKLIST.md."

echo
if [ "$status" -eq 0 ]; then
    echo "RESULT: Charta's own test suite passed."
else
    echo "RESULT: Charta's own test suite FAILED."
fi
exit "$status"
