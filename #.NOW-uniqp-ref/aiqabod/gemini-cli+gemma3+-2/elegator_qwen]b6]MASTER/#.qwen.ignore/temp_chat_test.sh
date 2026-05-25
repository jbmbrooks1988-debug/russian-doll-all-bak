#!/bin/bash

# Temporary chat test script for the_elegator_qwen
# This script creates a temporary project directory and starts a chat session

set -e  # Exit on any error

echo "=== Starting temporary chat session with elegator ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a temporary chat directory
TEMP_CHAT_DIR="./temp_chat_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEMP_CHAT_DIR"

echo "Created temporary chat directory: $TEMP_CHAT_DIR"

# Create a basic README for the temp project
cat > "$TEMP_CHAT_DIR/README.md" << EOF
# Temporary Chat Project
Created on $(date) for testing elegator chat functionality.
EOF

# Log the start of the chat session
echo "$(date): Starting temporary chat session" >> "$TEMP_CHAT_DIR/progress_reports.txt"

echo "Starting elegator chat session..."
echo "Project directory: $TEMP_CHAT_DIR"
echo "Task: General discussion and assistance"
echo "Note: This will run the AI agent with the task description."
echo ""

# Run the elegator with the temporary directory
./elegator "$TEMP_CHAT_DIR" "General discussion and assistance"

echo ""
echo "=== Chat session ended ==="
echo "Chat logs are available in: $TEMP_CHAT_DIR/progress_reports.txt"
echo "Agent output files are in: $TEMP_CHAT_DIR/"

# Ask if user wants to keep the temporary directory or clean it up
echo ""
echo -n "Do you want to keep the temporary files? (y/N): "
read REPLY
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cleaning up temporary directory..."
    rm -rf "$TEMP_CHAT_DIR"
    echo "Temporary directory removed."
else
    echo "Temporary directory preserved at: $TEMP_CHAT_DIR"
fi

echo "Chat session completed."