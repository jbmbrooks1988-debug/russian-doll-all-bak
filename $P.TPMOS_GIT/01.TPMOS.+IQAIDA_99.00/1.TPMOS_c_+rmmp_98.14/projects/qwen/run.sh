#!/bin/bash
# run.sh for qwen project
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
cd "$PROJECT_DIR"

if [ ! -f "manager/+x/qwen_manager.+x" ]; then
    echo "Building..."
    bash build.sh || exit 1
fi

echo "=== Launching Qwen Project (Foreground) ==="
echo "Project Root: $PROJECT_ROOT"

./manager/+x/qwen_manager.+x "$PROJECT_ROOT"
