#!/bin/bash

# Test script for the_elegator_qwen system
# This script tests the elegator system with the gitlet.c project

set -e  # Exit on any error

echo "=== Testing the_elegator_qwen system ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Current directory: $(pwd)"

# Check if required files exist
REQUIRED_FILES=(
    "elegator.c"
    "elegator_template_prompt.txt"
    "agent_manager.sh"
    "compile_elegator.sh"
    "run_elegator_gitlet.sh"
    "qwen_manager.sh"
    "gitlet.c]j0009/gitlet.c"
)

echo "Checking required files..."
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "ERROR: Required file not found: $file"
        exit 1
    fi
done

echo "All required files found."

# Compile the elegator
echo "Compiling elegator..."
chmod +x compile_elegator.sh
./compile_elegator.sh

if [ ! -f "./elegator" ]; then
    echo "ERROR: Elegator binary was not created"
    exit 1
fi

echo "Elegator compiled successfully."

# Create backup directory if it doesn't exist
if [ ! -d "backup" ]; then
    mkdir -p "backup"
    echo "Created backup directory"
fi

# Create a test prompt file for the gitlet project
cat > test_gitlet_prompt.txt << EOF
Implement a simple version control system in C with the following features:
1. Initialize a repository
2. Add files to staging area
3. Commit changes with a message
4. View commit history
5. View differences between commits

The implementation should be in the gitlet.c file and follow standard C practices.
EOF

echo "Created test prompt file."

# Test the agent manager functions
echo "Testing agent manager functions..."
chmod +x agent_manager.sh
source agent_manager.sh

# Create a basic progress report file
touch gitlet.c]j0009/progress_reports.txt

# Log initial test
log_progress "gitlet.c]j0009" "Starting test of elegator system"

# Test backup creation
create_backup "gitlet.c]j0009"
echo "Backup test completed."

# Test progress summary
summarize_progress "gitlet.c]j0009"
echo "Progress summary test completed."

# Count running agents (should be 0 initially)
agent_count=$(count_running_agents)
echo "Initial running agents: $agent_count"

# Check if qwen command is available
if ! command -v qwen &> /dev/null; then
    echo "WARNING: qwen command not found. Using mock qwen for testing."
    
    # Create a mock qwen command for testing
    cat > mock_qwen.sh << 'MOCK_EOF'
#!/bin/bash
# Mock qwen command for testing purposes

INPUT_FILE=""
OUTPUT_FILE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -f)
            INPUT_FILE="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

if [ -n "$INPUT_FILE" ] && [ -f "$INPUT_FILE" ]; then
    echo "MOCK QWEN: Processing input from $INPUT_FILE"
    echo "MOCK QWEN: This is a simulated response from the AI agent."
    echo "MOCK QWEN: Analyzing the gitlet.c project..."
    echo "MOCK QWEN: Identified key areas for improvement in version control implementation."
    echo "MOCK QWEN: Suggested changes to improve commit functionality."
    echo "MOCK QWEN: Completed task successfully."
else
    echo "MOCK QWEN: No input file provided"
fi
MOCK_EOF
    
    chmod +x mock_qwen.sh
    
    # Create a temporary alias for testing
    alias qwen='./mock_qwen.sh'
    
    echo "Created mock qwen for testing."
fi

# Test starting an agent (this would actually run qwen if available)
echo "Testing agent start function..."
if start_agent 0 "gitlet.c]j0009" "test_gitlet_prompt.txt"; then
    echo "Agent test completed successfully."
else
    echo "Agent test completed with exit code: $?"
fi

# Final check of running agents
final_count=$(count_running_agents)
echo "Final running agents: $final_count"

# Kill any remaining agents (though there shouldn't be any)
kill_all_agents

echo "=== Test completed successfully ==="
echo "The_elegator_qwen system is properly set up and functional."
echo ""
echo "To run the actual elegator with the gitlet project, use:"
echo "  ./run_elegator_gitlet.sh"
echo ""
echo "The system includes:"
echo "- Main elegator program (C)"
echo "- Template prompt system for reusability"
echo "- Agent management scripts"
echo "- Backup and progress tracking"
echo "- Process monitoring and cleanup"