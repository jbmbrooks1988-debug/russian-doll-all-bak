#!/bin/bash

# Nuanced task test for the_elegator_qwen
# This script tests the elegator's ability to run agents with time-based nuanced tasks

set -e  # Exit on any error

echo "=== Starting nuanced task test with elegator ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a test project directory
TEST_DIR="./nuanced_task_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"

echo "Created test directory: $TEST_DIR"

# Create a task file that will be updated by agents
cat > "$TEST_DIR/task_tracker.txt" << EOF
# Task Tracker for Nuanced Task Test
Started: $(date)
Expected tasks:
1. Generate unique string for minute 1
2. Generate unique string for minute 2  
3. Generate unique string for minute 3
EOF

# Log the start of the test
echo "$(date): Starting nuanced task test" >> "$TEST_DIR/progress_reports.txt"

echo "Starting elegator for nuanced task test..."
echo "Test directory: $TEST_DIR"
echo ""

# Create first prompt for minute 1
cat > "$TEST_DIR/prompt_minute_1.txt" << 'EOF'
You are an agent in a nuanced task test. Your specific task:

Generate a unique string that represents the first minute of this test. The string should:
1. Contain the word "Minute1"
2. Include a random element (like a random word or number)
3. Be different from any other strings generated in this test
4. Include the current timestamp

Write this unique string to a file called 'unique_strings.txt' in the project directory. If the file doesn't exist, create it. If it exists, append to it.

Also update the task_tracker.txt file to mark task 1 as completed.
EOF

echo "Running agent for minute 1..."
./elegator "$TEST_DIR" "Generate unique string for minute 1 with timestamp and random element"

# Wait 1 minute
echo "Waiting 1 minute before next task..."
sleep 60

# Create second prompt for minute 2
cat > "$TEST_DIR/prompt_minute_2.txt" << 'EOF'
You are an agent in a nuanced task test. Your specific task:

Generate a unique string that represents the second minute of this test. The string should:
1. Contain the word "Minute2"
2. Include a different random element from minute 1
3. Be significantly different from the first string
4. Include the current timestamp

Append this unique string to the 'unique_strings.txt' file in the project directory.

Also update the task_tracker.txt file to mark task 2 as completed.
EOF

echo "Running agent for minute 2..."
./elegator "$TEST_DIR" "Generate unique string for minute 2 with timestamp and different random element"

# Wait 1 minute
echo "Waiting 1 minute before next task..."
sleep 60

# Create third prompt for minute 3
cat > "$TEST_DIR/prompt_minute_3.txt" << 'EOF'
You are an agent in a nuanced task test. Your specific task:

Generate a unique string that represents the third minute of this test. The string should:
1. Contain the word "Minute3"
2. Include a completely different random element from minutes 1 and 2
3. Be significantly different from the previous strings
4. Include the current timestamp

Append this unique string to the 'unique_strings.txt' file in the project directory.

Also update the task_tracker.txt file to mark task 3 as completed.

Finally, summarize what has been accomplished in this test in summary.txt.
EOF

echo "Running agent for minute 3..."
./elegator "$TEST_DIR" "Generate unique string for minute 3 with timestamp and unique random element, then summarize test"

echo ""
echo "=== Nuanced task test completed ==="
echo "Results are available in: $TEST_DIR/"
echo "Unique strings: $TEST_DIR/unique_strings.txt"
echo "Task tracker: $TEST_DIR/task_tracker.txt"
echo "Progress reports: $TEST_DIR/progress_reports.txt"
echo "Summary: $TEST_DIR/summary.txt"

echo ""
echo "Contents of unique_strings.txt:"
if [ -f "$TEST_DIR/unique_strings.txt" ]; then
    cat "$TEST_DIR/unique_strings.txt"
else
    echo "File not created by agents"
fi

echo ""
echo "Contents of task_tracker.txt:"
cat "$TEST_DIR/task_tracker.txt"

echo ""
echo "Test completed successfully!"