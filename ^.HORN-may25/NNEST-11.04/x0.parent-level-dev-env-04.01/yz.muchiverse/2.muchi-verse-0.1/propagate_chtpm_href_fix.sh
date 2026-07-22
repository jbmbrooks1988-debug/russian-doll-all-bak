#!/bin/bash
# propagate_chtpm_href_fix.sh
# Propagates the CHTPM HREF navigation fix to all consumer projects
#
# Fix: Added href handler in active mode Enter key handler (lines 3396-3404)
# This enables href navigation for buttons inside ACTIVATE menus
#
# Date: 2026-07-22

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CANONICAL="$SCRIPT_DIR/shared-ops/chtpm_parser_pal.c"

echo "=========================================="
echo "CHTPM HREF FIX PROPAGATION"
echo "=========================================="
echo ""

if [ ! -f "$CANONICAL" ]; then
    echo "❌ ERROR: Canonical file not found: $CANONICAL"
    exit 1
fi

echo "✅ Canonical source: $CANONICAL"
echo ""
echo "Fix preview:"
grep -A 8 "else if (strlen(child_el->href) > 0)" "$CANONICAL" | head -10
echo ""
echo "=========================================="
echo ""

SUCCESSFUL=0
FAILED=0

# Propagate to each found project
for project_dir in "$SCRIPT_DIR"/*; do
    [ -d "$project_dir" ] || continue
    [ "$project_dir" = "$SCRIPT_DIR/shared-ops" ] && continue
    [ "$project_dir" = "$SCRIPT_DIR/pal-scripts" ] && continue
    [ "$project_dir" = "$SCRIPT_DIR/net" ] && continue

    project_name=$(basename "$project_dir")
    target="$project_dir/system/chtpm_parser_pal.c"

    if [ ! -f "$target" ]; then
        continue
    fi

    # Backup
    backup="${target}.pre-href-fix-2026-07-22"
    if [ ! -f "$backup" ]; then
        cp "$target" "$backup" 2>/dev/null || true
    fi

    # Propagate
    if cp "$CANONICAL" "$target" 2>/dev/null; then
        echo "✅ $project_name"
        ((SUCCESSFUL++))
    else
        echo "❌ $project_name (copy failed)"
        ((FAILED++))
    fi
done

echo ""
echo "=========================================="
echo "RESULT: $SUCCESSFUL propagated, $FAILED failed"
echo "=========================================="
echo ""
echo "Backups created with: .pre-href-fix-2026-07-22 suffix"
echo "Next: Run 'bash scripts/build.sh' in each project"
echo ""
