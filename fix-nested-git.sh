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

# Load existing gitmodules if any
if [ -f .gitmodules ]; then
    GITMODULES_CONTENT=$(cat .gitmodules)
else
    GITMODULES_CONTENT=""
fi

FIXED=0

for NESTED_GIT in $NESTED_GITS; do
    # Get the path relative to repo root
    REL_PATH="${NESTED_GIT#$REPO_DIR/}"
    REL_PATH="${REL_PATH%/.git}"

    echo "Processing: $REL_PATH"

    # Check if this is already a submodule in .gitmodules
    if echo "$GITMODULES_CONTENT" | grep -q "path = $REL_PATH"; then
        echo "  -> Already in .gitmodules, skipping."
        continue
    fi

    # Check if it's tracked as a gitlink in the index
    if git ls-files -s "$REL_PATH" 2>/dev/null | head -1 | grep -q "^160000"; then
        echo "  -> Removing gitlink from index..."
        git rm --cached "$REL_PATH"
    fi

    # Remove the .git directory
    echo "  -> Removing nested .git directory..."
    rm -rf "$NESTED_GIT"

    # Add to .gitignore
    if ! grep -qF "$REL_PATH" .gitignore 2>/dev/null; then
        echo "  -> Adding to .gitignore..."
        echo "$REL_PATH" >> .gitignore
    fi

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
