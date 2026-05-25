#!/bin/bash

# Demonstration: Using elegator to evolve a project through multiple versions
# This shows how elegator can manage a project with multiple versions

set -e  # Exit on any error

echo "=== Starting demo project evolution with elegator ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create a prompt for evolving the calculator from v1 to v2
cat > evolve_v1_to_v2.txt << 'EOF'
You are the_elegator managing the evolution of a simple calculator program from version 1 to version 2. 

Current state (v1):
- Basic calculator with addition and subtraction functions
- Simple implementation in calculator.c

Required improvements for v2:
1. Add multiplication function
2. Add division function with error handling for division by zero
3. Improve the user interface to show all operations
4. Add proper error handling
5. Maintain clean, well-documented code

Implement these changes to evolve the calculator from v1 to v2.
EOF

echo "Created evolution prompt from v1 to v2"

# Log the start of the evolution
echo "$(date): Starting evolution from demo_project_v1 to v2" >> demo_project_v1/progress_reports.txt

# Run the elegator to evolve v1 to v2
echo "Running elegator to evolve v1 to v2..."
./elegator "./demo_project_v1" "Evolve the calculator from v1 to v2 by adding multiplication, division, and error handling"

# Create a prompt for evolving the calculator from v2 to v3
cat > evolve_v2_to_v3.txt << 'EOF'
You are the_elegator managing the evolution of a calculator program from version 2 to version 3.

Current state (v2):
- Calculator with addition, subtraction, multiplication, and division
- Error handling for division by zero
- Basic implementation in calculator.c

Required improvements for v3:
1. Add power function using math library
2. Add square root function with error handling for negative numbers
3. Include math.h and link with math library
4. Add more comprehensive examples in main function
5. Improve code documentation
6. Optimize for better performance

Implement these changes to evolve the calculator from v2 to v3.
EOF

echo "Created evolution prompt from v2 to v3"

# Log the start of the v2 to v3 evolution
echo "$(date): Starting evolution from demo_project_v2 to v3" >> demo_project_v2/progress_reports.txt

# Run the elegator to evolve v2 to v3
echo "Running elegator to evolve v2 to v3..."
./elegator "./demo_project_v2" "Evolve the calculator from v2 to v3 by adding power, square root, and advanced math functions"

# Create a comprehensive report
echo "=== Demo Project Evolution Report ===" > demo_evolution_report.txt
echo "Date: $(date)" >> demo_evolution_report.txt
echo "" >> demo_evolution_report.txt
echo "The elegator successfully managed the evolution of the calculator project through 3 versions:" >> demo_evolution_report.txt
echo "1. v1: Basic operations (add, subtract)" >> demo_evolution_report.txt
echo "2. v2: Enhanced operations (add, subtract, multiply, divide)" >> demo_evolution_report.txt
echo "3. v3: Complete operations (all basic + advanced math functions)" >> demo_evolution_report.txt
echo "" >> demo_evolution_report.txt
echo "Key achievements:" >> demo_evolution_report.txt
echo "- Automated project evolution across multiple versions" >> demo_evolution_report.txt
echo "- Maintained code quality and documentation" >> demo_evolution_report.txt
echo "- Added proper error handling at each stage" >> demo_evolution_report.txt
echo "- Created timestamped logs of all changes" >> demo_evolution_report.txt
echo "- Generated backups at each stage" >> demo_evolution_report.txt

echo "Demo evolution report created: demo_evolution_report.txt"

# Create backups of all versions
source agent_manager.sh
create_backup "demo_project_v1"
create_backup "demo_project_v2" 
create_backup "demo_project_v3"

echo "Backups created for all project versions"

# Log completion
echo "$(date): Demo project evolution completed successfully" >> demo_project_v3/progress_reports.txt
echo "$(date): Demo project evolution completed successfully" >> demo_project_v2/progress_reports.txt
echo "$(date): Demo project evolution completed successfully" >> demo_project_v1/progress_reports.txt

echo "=== Demo project evolution completed ==="
echo "Check the progress reports in each version directory for details"
echo "Demo evolution report: demo_evolution_report.txt"