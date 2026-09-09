#!/bin/bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_DIR"

echo "Scanning for nested git repositories..."

# Find all nested .git directories (excluding root)
NESTED_GITS=$(find "$REPO_DIR" -type d -name '.git' | grep -v "^$REPO_DIR/\.git$" || true)

if [ -z "$NESTED_GITS" ]; then
    echo "No nested git repositories found."
    exit 0
fi

echo "Found nested git repositories:"
echo "$NESTED_GITS"
echo ""

FIXED=0

for NESTED_GIT in $NESTED_GITS; do
    # Get the path relative to repo root
    REL_PATH="${NESTED_GIT#$REPO_DIR/}"
    REL_PATH="${REL_PATH%/.git}"

    echo "Processing: $REL_PATH"

    # Check if it's tracked as a gitlink in the index
    if git ls-files -s "$REL_PATH" 2>/dev/null | head -1 | grep -q "^160000"; then
        echo "  -> Removing gitlink from index..."
        git rm --cached "$REL_PATH"
    fi

    # Remove the .git directory
    echo "  -> Removing nested .git directory..."
    rm -rf "$NESTED_GIT"

    # Add .git directory to .gitignore if not already there
    IGNORE_ENTRY="$REL_PATH/.git"
    if ! grep -qF "$IGNORE_ENTRY" .gitignore 2>/dev/null; then
        echo "  -> Adding $IGNORE_ENTRY to .gitignore..."
        echo "$IGNORE_ENTRY" >> .gitignore
    fi

    # Stage the actual files from the folder (not the .git)
    echo "  -> Staging folder contents..."
    git add "$REL_PATH/"

    FIXED=$((FIXED + 1))
done

if [ $FIXED -gt 0 ]; then
    echo ""
    echo "Fixed $FIXED nested git repository(ies)."
    echo "Review the changes and commit when ready."
else
    echo ""
    echo "No changes needed."
fi
