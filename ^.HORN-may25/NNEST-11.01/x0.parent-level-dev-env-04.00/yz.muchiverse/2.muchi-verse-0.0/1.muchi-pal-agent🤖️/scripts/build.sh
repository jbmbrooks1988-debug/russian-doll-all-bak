#!/bin/bash
# scripts/build.sh - compile everything, warning-free, matching
# mutaclsym's standing bar (see its dox/01-cdda-architecture.md §8).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p system ops/+x manager/+x

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
echo "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c) ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building ops ---"
for src in ops/*.c; do
    name="$(basename "$src" .c)"
    echo "  Compiling $name..."
    gcc $CFLAGS "$src" -o "ops/+x/$name.+x"
done

echo "--- Building manager (PERSISTENT process - path_nav_manager, see"
echo "    that file's own header comment) ---"
gcc $CFLAGS "manager/path_nav_manager.c" -o "manager/+x/path_nav_manager.+x"

echo "--- Build Complete ---"
ls -l system/prisc+x system/keyboard_input system/renderer system/chtpm_parser_pal ops/+x/ manager/+x/
