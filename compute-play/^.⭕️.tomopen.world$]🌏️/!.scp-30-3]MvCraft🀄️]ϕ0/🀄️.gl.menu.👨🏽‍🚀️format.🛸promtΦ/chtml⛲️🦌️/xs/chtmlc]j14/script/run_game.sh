#!/bin/bash

# Script to run the CHTML module platform with game module
# Usage: ./run_game.sh [markup_file] [module_executable]
# Default: runs demo.chtml with game module

MARKUP_FILE=${1:-"demo_game.chtml.txt"}
MODULE_EXECUTABLE=${2:-"./module/game"}

echo "Starting CHTML Module Platform..."
echo "Markup file: $MARKUP_FILE"
echo "Module executable: $MODULE_EXECUTABLE"

# Run the application with the specified arguments
./main_prototype_0 "$MARKUP_FILE" "$MODULE_EXECUTABLE"

echo "Application finished."
