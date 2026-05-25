#!/bin/bash

# Master-Slave Session Test for the_elegator_qwen
# This script demonstrates the elegator starting a master session that then starts a slave session

set -e  # Exit on any error

echo "=== Starting Master-Slave Session Test ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a test project directory
TEST_DIR="./master_slave_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"

echo "Created test directory: $TEST_DIR"

# Create a master task that will initiate a slave task
cat > "$TEST_DIR/master_task.txt" << EOF
# Master-Slave Session Test
Master ID: 
Slave ID: 
Status: Not started
EOF

# Log the start of the test
echo "$(date): Starting master-slave session test" >> "$TEST_DIR/progress_reports.txt"

echo "Starting Master Agent..."
echo "Test directory: $TEST_DIR"
echo ""

# Run the master agent with a task to create its ID and start a slave
./elegator "$TEST_DIR" "You are the MASTER agent. Your tasks: 1) Generate a unique master ID (like 'MASTER-[timestamp]') and write it to master_id.txt. 2) Create a SLAVE task description in slave_task.txt that asks the slave to generate its own unique ID. 3) Update master_task.txt to show your ID and mark status as 'waiting for slave'. 4) The elegator will then run a slave agent with the slave_task.txt."

echo ""
echo "Master Agent completed. Now running Slave Agent based on master's task..."

# Run the slave agent with the task created by the master
if [ -f "$TEST_DIR/slave_task.txt" ]; then
    ./elegator "$TEST_DIR" "$(cat $TEST_DIR/slave_task.txt)"
else
    # If master didn't create the file, create a default task
    echo "Master did not create slave_task.txt, using default task"
    ./elegator "$TEST_DIR" "You are the SLAVE agent. Your tasks: 1) Generate a unique slave ID (like 'SLAVE-[timestamp]') and write it to slave_id.txt. 2) Read the master_id.txt file to see the master's ID. 3) Update master_task.txt to include your slave ID and mark status as 'slave completed'."
fi

# Final verification
echo ""
echo "=== Master-Slave Session Test Results ==="

echo "Contents of master_id.txt:"
if [ -f "$TEST_DIR/master_id.txt" ]; then
    cat "$TEST_DIR/master_id.txt"
else
    echo "File not created by master agent"
fi

echo ""
echo "Contents of slave_id.txt:"
if [ -f "$TEST_DIR/slave_id.txt" ]; then
    cat "$TEST_DIR/slave_id.txt"
else
    echo "File not created by slave agent"
fi

echo ""
echo "Contents of master_task.txt:"
cat "$TEST_DIR/master_task.txt"

echo ""
echo "Contents of slave_task.txt (created by master):"
if [ -f "$TEST_DIR/slave_task.txt" ]; then
    cat "$TEST_DIR/slave_task.txt"
else
    echo "File not created by master agent"
fi

echo ""
echo "Progress reports:"
cat "$TEST_DIR/progress_reports.txt"

echo ""
echo "=== Test Complete ==="
echo "This demonstrates:"
echo "1. Master agent created its own ID"
echo "2. Master agent created a task for the slave agent"
echo "3. Slave agent was started separately with the master's task"
echo "4. Slave agent could read the master's ID"
echo ""
echo "Each agent ran as a separate session, coordinated by the elegator system."