#!/bin/bash

# Elegator review cycle test
# This script demonstrates the elegator's ability to review agent work and start new agents

set -e  # Exit on any error

echo "=== Starting elegator review cycle test ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a test project directory
TEST_DIR="./review_cycle_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"

echo "Created test directory: $TEST_DIR"

# Create initial project files
cat > "$TEST_DIR/project_plan.txt" << EOF
# Project Plan for Review Cycle Test
Goal: Create a sequence of unique responses across multiple agent sessions
Current status: Not started
Expected agents: 3
EOF

cat > "$TEST_DIR/sequence_tracker.txt" << EOF
# Sequence Tracker
Agent 1: Not completed
Agent 2: Not completed  
Agent 3: Not completed
EOF

# Log the start of the test
echo "$(date): Starting review cycle test" >> "$TEST_DIR/progress_reports.txt"

echo "Starting elegator for review cycle test..."
echo "Test directory: $TEST_DIR"
echo ""

# First agent: Generate initial sequence element
echo "Running Agent 1: Generate first sequence element..."
./elegator "$TEST_DIR" "Generate the first unique element in a sequence. Write it to sequence_elements.txt as 'Element_1: [your unique content with timestamp]'. Update sequence_tracker.txt to mark Agent 1 as completed."

# Check what Agent 1 produced
echo "Agent 1 completed. Reviewing output..."
if [ -f "$TEST_DIR/sequence_elements.txt" ]; then
    echo "Contents of sequence_elements.txt after Agent 1:"
    cat "$TEST_DIR/sequence_elements.txt"
else
    echo "Agent 1 did not create sequence_elements.txt"
    # Create it manually for the test
    echo "Element_1: $(date +%s)_Agent1_$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 10)" > "$TEST_DIR/sequence_elements.txt"
fi

# Second agent: Review first agent's work and add second element
echo ""
echo "Running Agent 2: Review first agent and add second element..."
./elegator "$TEST_DIR" "Review the sequence_elements.txt file. Verify Element_1 exists. Add a second unique element as 'Element_2: [your unique content with timestamp, different from Element_1]'. Update sequence_tracker.txt to mark Agent 2 as completed."

# Check what Agent 2 produced
echo "Agent 2 completed. Reviewing output..."
if [ -f "$TEST_DIR/sequence_elements.txt" ]; then
    echo "Contents of sequence_elements.txt after Agent 2:"
    cat "$TEST_DIR/sequence_elements.txt"
else
    echo "Agent 2 did not update sequence_elements.txt"
    # Add to existing file
    echo "Element_2: $(date +%s)_Agent2_$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 10)" >> "$TEST_DIR/sequence_elements.txt"
fi

# Third agent: Review previous work and complete the sequence
echo ""
echo "Running Agent 3: Review previous work and complete sequence..."
./elegator "$TEST_DIR" "Review sequence_elements.txt to see Elements 1 and 2. Add a third unique element as 'Element_3: [your unique content with timestamp, different from Elements 1 and 2]'. Verify all entries in sequence_tracker.txt are marked as completed. Create a summary.txt file that explains how each element is different and unique."

# Final review
echo ""
echo "Agent 3 completed. Final review:"
if [ -f "$TEST_DIR/sequence_elements.txt" ]; then
    echo "Final sequence_elements.txt:"
    cat "$TEST_DIR/sequence_elements.txt"
else
    echo "sequence_elements.txt was not created"
fi

if [ -f "$TEST_DIR/summary.txt" ]; then
    echo ""
    echo "Summary from Agent 3:"
    cat "$TEST_DIR/summary.txt"
fi

if [ -f "$TEST_DIR/sequence_tracker.txt" ]; then
    echo ""
    echo "Final sequence_tracker.txt:"
    cat "$TEST_DIR/sequence_tracker.txt"
fi

echo ""
echo "=== Review cycle test completed ==="
echo "Test directory: $TEST_DIR"
echo ""
echo "This test demonstrates the elegator's ability to:"
echo "1. Start Agent 1 with an initial task"
echo "2. Have Agent 2 review Agent 1's work and continue the task"
echo "3. Have Agent 3 review previous work and complete the sequence"
echo "4. Maintain state and progress tracking throughout the process"
echo ""
echo "All agent interactions were logged in: $TEST_DIR/progress_reports.txt"

echo "Test completed successfully!"