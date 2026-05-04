#!/bin/bash
# build.sh for qwen project

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

echo "--- Building Qwen Manager ---"
gcc -Wall -Wextra -O2 manager/qwen_manager.c -o manager/+x/qwen_manager.+x

echo "--- Building Qwen Bridge ---"
gcc -Wall -Wextra -O2 ops/src/qwen_bridge.c -o ops/+x/qwen_bridge.+x

# Ensure directories exist
mkdir -p manager/+x ops/+x

echo "--- Build Complete ---"
ls -l manager/+x/qwen_manager.+x
