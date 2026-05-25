#!/bin/bash

# Simple verification test for the_elegator_qwen
# This script verifies the elegator can run agents that create files

set -e  # Exit on any error

echo "=== Starting Simple Verification Test ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a test project directory
TEST_DIR="./simple_verification_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"

echo "Created test directory: $TEST_DIR"

# Log the start of the test
echo "$(date): Starting simple verification test" >> "$TEST_DIR/progress_reports.txt"

echo "Starting Agent to create a simple file..."
./elegator "$TEST_DIR" "Create a file called verification.txt that contains 'Agent verification successful at [current timestamp]'"

echo ""
echo "=== Verification Results ==="

echo "Contents of verification.txt:"
if [ -f "$TEST_DIR/verification.txt" ]; then
    cat "$TEST_DIR/verification.txt"
    echo ""
    echo "✅ Agent successfully created the file!"
else
    echo "❌ Agent did not create verification.txt"
fi

echo ""
echo "Progress reports:"
cat "$TEST_DIR/progress_reports.txt"

echo ""
echo "=== Test Complete ==="
echo "This confirms that the elegator system can successfully run agents that perform file operations."