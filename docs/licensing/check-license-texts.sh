#!/bin/sh
# Every SPDX identifier used in the tree must have a licence text in LICENSES/.
# Run from the repository root. Exits non-zero and lists what is missing.
#
# Only source/data files that can carry SPDX headers are scanned. Documentation,
# CSV inventories, evidence logs, and the script itself merely *mention*
# SPDX-License-Identifier and must not be treated as declarations.
set -eu

missing=$(mktemp)
trap 'rm -f "$missing"' EXIT

# File patterns that can carry SPDX-License-Identifier headers.
patterns='*.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx *.c++ \
          *.qml *.js *.mjs *.py *.sh *.bash *.zsh \
          *.cmake *.CMakeLists.txt \
          *.ui *.rc *.qrc *.desktop *.metainfo.xml \
          *.json *.xml *.svg *.png *.jpg *.jpeg *.gif *.ico \
          *.diff *.patch *.pc.in *.pc'

git grep -l 'SPDX-License-Identifier' -- $patterns 2>/dev/null | while read -r f; do
    sed -n 's/.*SPDX-License-Identifier:[[:space:]]*//p' "$f" \
      | tr -d '()' \
      | tr ' ' '\n' \
      | grep -v '^OR$\|^AND$\|^$'
done | sort -u > "$missing" || true

status=0
while IFS= read -r id; do
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
done < "$missing"

if [ "$status" -ne 0 ]; then
    echo "Add the missing texts to LICENSES/ (canonical source: the SPDX licence list)."
    exit 1
fi
echo "All identifiers in use have a licence text in LICENSES/."
