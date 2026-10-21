#!/bin/sh
# Every SPDX identifier used in the tree must have a licence text in LICENSES/.
# Run from the repository root. Exits non-zero and lists what is missing.
set -eu

missing=$(mktemp)
trap 'rm -f "$missing"' EXIT

git grep -h 'SPDX-License-Identifier' -- . 2>/dev/null \
  | sed 's/.*SPDX-License-Identifier: *//; s/ *$//' \
  | tr ' ' '\n' | tr -d '()' | sort -u > "$missing" || true

status=0
for id in $(cat "$missing"); do
    [ -z "$id" ] && continue
    if [ ! -f "LICENSES/$id.txt" ]; then
        # OR/AND expressions: accept if every component has a text.
        ok=1
        for part in $(echo "$id" | tr '()' ' ' | tr ' ' '\n' | grep -v '^OR$\|^AND$\|^$'); do
            [ -f "LICENSES/$part.txt" ] || ok=0
        done
        if [ "$ok" -eq 0 ]; then
            echo "MISSING licence text for: $id"
            status=1
        fi
    fi
done

if [ "$status" -ne 0 ]; then
    echo "Add the missing texts to LICENSES/ (canonical source: the SPDX licence list)."
    exit 1
fi
echo "All identifiers in use have a licence text in LICENSES/."
