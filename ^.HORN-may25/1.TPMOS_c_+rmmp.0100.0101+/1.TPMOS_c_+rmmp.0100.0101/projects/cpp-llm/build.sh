#!/bin/bash
# build.sh for cpp-llm project

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

# Ensure directories exist
mkdir -p manager/+x ops/+x

echo "--- Building cpp-llm Manager ---"
gcc -Wall -Wextra -O2 manager/cpp-llm_manager.c -o manager/+x/cpp-llm_manager.+x

echo "--- Building cpp-llm Agent Tools ---"
for tool_src in ops/src/*.c; do
    tool_name=$(basename "$tool_src" .c)
    
    echo "  Compiling $tool_name..."
    gcc -Wall -Wextra -O2 "$tool_src" -o "ops/+x/$tool_name"
    
    # Also create .+x version for compatibility with some loaders
    cp "ops/+x/$tool_name" "ops/+x/$tool_name.+x"
done

echo "--- Build Complete ---"
ls -l manager/+x/cpp-llm_manager.+x ops/+x/
