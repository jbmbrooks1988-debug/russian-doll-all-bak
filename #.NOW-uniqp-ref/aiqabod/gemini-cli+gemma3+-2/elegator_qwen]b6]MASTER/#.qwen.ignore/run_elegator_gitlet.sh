#!/bin/bash

# Script to run the elegator with the gitlet project
# Usage: ./run_elegator_gitlet.sh

set -e  # Exit on any error

PROJECT_PATH="./gitlet.c]j0009"
TASK_DESCRIPTION="Implement a local version control system similar to git"

echo "Starting elegator for gitlet project..."
echo "Project: $PROJECT_PATH"
echo "Task: $TASK_DESCRIPTION"

# Make sure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Run the elegator
./elegator "$PROJECT_PATH" "$TASK_DESCRIPTION"