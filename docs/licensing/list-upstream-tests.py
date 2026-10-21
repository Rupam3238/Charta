#!/usr/bin/env python3
"""List the CTest test names that come from the fetched upstream QPDF build.

    docs/licensing/list-upstream-tests.py <build-dir>

The fetched QPDF adds its own test suites (check-assert, compare-for-test, qpdf, libtests,
examples, zlib-flate, fuzz) unconditionally: its CMakeLists.txt has no BUILD_TESTING guard and
offers no option to switch them off, and they cannot be labelled from outside, because CMake's
set_tests_properties() only sees tests defined in the current directory.

So the split is done by name, and the names are read from the generated CTestTestfile.cmake files
instead of being hardcoded. The output is a single '|'-separated list for use as a ctest -E / -R
regular expression, or a plain list when called without --regex.
"""
import argparse
import glob
import os
import re
import sys

# add_test(name "cmd")  |  add_test("name" "cmd")  |  add_test([=[name]=] ...)
ADD_TEST = re.compile(
    r'add_test\(\s*(?:\[\[?=([^\]]*)\]\]?|"([^"]*)"|([^\s()]+))'
)


def collect(build_dir):
    names = set()
    pattern = os.path.join(build_dir, "_deps", "**", "CTestTestfile.cmake")
    for path in glob.glob(pattern, recursive=True):
        if "qpdf-build" not in path:
            continue
        try:
            with open(path, errors="ignore") as handle:
                text = handle.read()
        except OSError:
            continue
        for match in ADD_TEST.finditer(text):
            name = (match.group(1) or match.group(2) or match.group(3) or "").strip()
            if name:
                names.add(name)
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", help="the CMake build directory")
    parser.add_argument("--regex", action="store_true", help="print a '|'-separated regex")
    args = parser.parse_args()

    if not os.path.exists(os.path.join(args.build_dir, "CTestTestfile.cmake")):
        print(f"no CTestTestfile.cmake in {args.build_dir}", file=sys.stderr)
        return 2

    names = sorted(collect(args.build_dir))
    if not names:
        print("no upstream qpdf tests found", file=sys.stderr)
        return 1
    print("|".join(names) if args.regex else "\n".join(names))
    return 0


if __name__ == "__main__":
    sys.exit(main())
