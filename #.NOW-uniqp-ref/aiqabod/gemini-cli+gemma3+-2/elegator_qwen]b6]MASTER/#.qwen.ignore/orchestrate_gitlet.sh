#!/bin/bash

# Demonstration script: Using elegator to orchestrate gitlet.c improvements
# This script will use the elegator to manage AI agents for improving the gitlet.c project

set -e  # Exit on any error

echo "=== Starting gitlet.c orchestration with elegator ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a specific prompt for gitlet.c improvements
cat > gitlet_improvement_prompt.txt << 'EOF'
You are the_elegator managing improvements to gitlet.c, a git-like version control system implemented in C. The current implementation has basic functionality but needs enhancements.

Current gitlet.c features:
- Repository initialization (init)
- File staging (add)
- Committing changes (commit)
- Viewing commit history (log)
- Status checking (status)
- Branch management (branch)
- Checking out branches (checkout)
- Removing files (rm)
- Pushing to remote (push)
- Pulling from remote (pull)
- Merging branches (merge)
- Forking repositories (fork)

Your task is to enhance gitlet.c with the following improvements:
1. Add better error handling throughout the code
2. Improve the hash function to be more robust
3. Add a diff command to show differences between commits/files
4. Add a reset command to undo changes
5. Add a clone command to copy remote repositories
6. Improve the user interface with better messages
7. Add configuration management features
8. Optimize performance for large repositories

Focus on one enhancement at a time, implement it properly with error checking, and ensure it integrates well with the existing codebase. Write clean, well-documented code following C best practices.
EOF

echo "Created improvement prompt for gitlet.c"

# Log the start of the orchestration
echo "$(date): Starting gitlet.c orchestration with elegator" >> gitlet.c]j0009/progress_reports.txt

# Run the elegator with the gitlet project and improvement task
echo "Running elegator to improve gitlet.c..."
./elegator "./gitlet.c]j0009" "Improve the gitlet.c version control system with better error handling, new commands (diff, reset, clone), and performance optimizations"

echo "Elegator session completed for gitlet.c improvements"

# Create a backup of the current state after the first agent run
echo "Creating backup of current state..."
source agent_manager.sh
create_backup "gitlet.c]j0009"

# Log the backup
echo "$(date): Created backup after initial agent run" >> gitlet.c]j0009/progress_reports.txt

# Now let's run a second agent to continue improvements
cat > gitlet_optimization_prompt.txt << 'EOF'
You are the_elegator continuing improvements to gitlet.c. Review the changes made by the previous agent and continue with additional enhancements:

1. Add support for .gitletignore files to ignore specified patterns
2. Implement a stash command to temporarily save changes
3. Add better memory management to prevent leaks
4. Implement a rebase command for better history management
5. Add support for signed commits
6. Improve the command-line argument parsing
7. Add unit tests for the core functionality
8. Document the API with comments

Focus on implementing these features while maintaining compatibility with existing functionality.
EOF

echo "Created second prompt for continued improvements"

# Run a second agent for continued improvements
echo "Running second agent for continued improvements..."
./elegator "./gitlet.c]j0009" "Continue improving gitlet.c with .gitletignore support, stash command, memory management, rebase, and unit tests"

echo "Second elegator session completed"

# Final backup and log
create_backup "gitlet.c]j0009"
echo "$(date): Created final backup after all agent runs" >> gitlet.c]j0009/progress_reports.txt

echo "=== Gitlet.c orchestration completed ==="
echo "Progress reports are available in gitlet.c]j0009/progress_reports.txt"
echo "Backups are available in gitlet.c]j0009/backup/"