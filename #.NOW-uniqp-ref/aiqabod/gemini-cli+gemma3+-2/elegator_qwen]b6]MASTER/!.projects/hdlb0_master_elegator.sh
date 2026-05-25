#!/bin/bash

# Master Elegator Script for HDLB0 Project
# Project: HDLB0 - Building RISC-V 32 using NAND chips with progressive architecture

set -e  # Exit on any error

echo "=== Starting HDLB0 Master Elegator ==="

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Go up to the main directory where elegator is
cd ..

# Ensure the elegator binary exists
if [ ! -f "./elegator" ]; then
    echo "Elegator binary not found. Compiling..."
    ./compile_elegator.sh
fi

# Create project directory for hdlb0
PROJECT_DIR="./projects/hdlb0_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$PROJECT_DIR"

echo "Created project directory: $PROJECT_DIR"

# Create initial project structure
mkdir -p "$PROJECT_DIR/nand_gates"
mkdir -p "$PROJECT_DIR/riscv_2bit"
mkdir -p "$PROJECT_DIR/riscv_8bit" 
mkdir -p "$PROJECT_DIR/riscv_16bit"
mkdir -p "$PROJECT_DIR/riscv_32bit"
mkdir -p "$PROJECT_DIR/docs"
mkdir -p "$PROJECT_DIR/tests"

# Create initial documentation
cat > "$PROJECT_DIR/README.md" << EOF
# HDLB0 - RISC-V Architecture Built from NAND Gates

## Project Overview
Building RISC-V architecture from the ground up using NAND gates, progressing from 2-bit to 32-bit with backward compatibility.

## Architecture Progression
1. NAND Gates - Basic logic building blocks
2. RISC-V 2-bit - Minimal instruction set
3. RISC-V 8-bit - Extended functionality  
4. RISC-V 16-bit - More complex operations
5. RISC-V 32-bit - Full RISC-V implementation

## Current Status
- [ ] NAND gate designs
- [ ] 2-bit RISC-V implementation
- [ ] 8-bit RISC-V implementation  
- [ ] 16-bit RISC-V implementation
- [ ] 32-bit RISC-V implementation
- [ ] Backward compatibility verification
EOF

# Log the start of the project
echo "$(date): Starting HDLB0 RISC-V from NAND gates project" >> "$PROJECT_DIR/progress_reports.txt"

echo "Project structure created. Starting Master Elegator..."

# Master task: Analyze the project, create a todo list, and start slave agents
./elegator "$PROJECT_DIR" "You are the MASTER ELEGATOR for the HDLB0 project. Your tasks:

1. Analyze the entire project directory structure and files
2. Create a comprehensive TODO list for implementing RISC-V architecture from NAND gates up to 32-bit
3. Prioritize the tasks in the correct order: NAND gates -> 2-bit -> 8-bit -> 16-bit -> 32-bit
4. Create detailed specifications for each stage
5. Design the first NAND gate implementations
6. Plan for backward compatibility between stages
7. Create test plans for each stage

Document your analysis in project_analysis.txt and create a detailed TODO list in todo_list.txt. Then create specific tasks for slave agents in slave_tasks/ directory."

echo ""
echo "=== HDLB0 Master Elegator Completed ==="
echo "Project directory: $PROJECT_DIR"
echo "Analysis and TODO list should be in the project directory"
echo "Slave tasks should be created in $PROJECT_DIR/slave_tasks/"
echo ""
echo "Next steps:"
echo "1. Review the master's analysis and TODO list"
echo "2. Run slave agents with the tasks in slave_tasks/ directory"
echo "3. Monitor progress and iterate as needed"