#!/bin/bash
# run_xo-pet.sh - Launcher for the XO-PET V1 PoC
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$PROJECT_DIR/../../.." && pwd)"

echo "Launching XO-PET V1..."
echo "Root: $ROOT_DIR"
echo "Project: $PROJECT_DIR"

# 1. Start the Manager in the background
cd "$PROJECT_DIR/manager"
./xo-pet_manager > manager.log 2>&1 &
MGR_PID=$!
echo "Manager started (PID $MGR_PID)"

# 2. Point the system to our layout
mkdir -p "$ROOT_DIR/pieces/display"
echo "projects/xo-pet-v1/layouts/xo-pet.chtpm" > "$ROOT_DIR/pieces/display/current_layout.txt"

# 3. Launch the Parser (Foreground)
# We use the parser from the mew project
cd "$ROOT_DIR/x0.5-liz.fiter4-mew-00.00"
./pieces/chtpm/plugins/+x/chtpm_parser.+x projects/xo-pet-v1/layouts/xo-pet.chtpm

# Cleanup
kill $MGR_PID
echo "Shutdown complete."
EOF
chmod +x x0.5-liz.fiter4-mew-00.00/projects/xo-pet-v1/run_xo-pet.sh
