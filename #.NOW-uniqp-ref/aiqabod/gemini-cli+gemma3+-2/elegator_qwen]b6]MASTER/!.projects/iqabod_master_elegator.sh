#!/bin/bash

# Master Elegator Script for IQABOD Project
# Project: IQABOD - Token-centric LLM for games and apps

set -e  # Exit on any error

echo "=== Starting IQABOD Master Elegator ==="

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

# Create project directory for iqabod
PROJECT_DIR="./projects/iqabod_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$PROJECT_DIR"

echo "Created project directory: $PROJECT_DIR"

# Create initial project structure
mkdir -p "$PROJECT_DIR/vocab"
mkdir -p "$PROJECT_DIR/models"
mkdir -p "$PROJECT_DIR/training"
mkdir -p "$PROJECT_DIR/tests"
mkdir -p "$PROJECT_DIR/data"

# Create initial documentation
cat > "$PROJECT_DIR/README.md" << EOF
# IQABOD - Token-Centric LLM for Games and Apps

## Project Overview
A token-centric language model designed for use in games and applications, with human-readable and customizable data.

## Goals
1. Expand vocabulary with meaningful tokens
2. Train model for coherent conversations
3. Implement bias adjustment for better outputs
4. Create game/app integration capabilities
5. Maintain human-readable data formats

## Current Status
- [ ] Vocabulary analysis and expansion
- [ ] Initial model training
- [ ] Bias adjustment and tuning
- [ ] Conversation coherence testing
- [ ] Human-readable data formats
EOF

# Log the start of the project
echo "$(date): Starting IQABOD token-centric LLM project" >> "$PROJECT_DIR/progress_reports.txt"

echo "Project structure created. Starting Master Elegator..."

# Master task: Analyze the project, create a todo list, and start slave agents
./elegator "$PROJECT_DIR" "You are the MASTER ELEGATOR for the IQABOD project. Your tasks:

1. Analyze the requirements for a token-centric LLM for games and apps
2. Create a comprehensive TODO list for expanding the vocabulary and training the model
3. Design a plan for adding tokens to achieve coherent conversations
4. Plan for manual bias adjustment to optimize outputs
5. Create specifications for human-readable and customizable data formats
6. Design testing procedures for conversation quality
7. Plan for game and app integration

Document your analysis in project_analysis.txt and create a detailed TODO list in todo_list.txt. Then create specific tasks for slave agents in slave_tasks/ directory, focusing on vocabulary expansion, model training, and bias adjustment."

echo ""
echo "=== IQABOD Master Elegator Completed ==="
echo "Project directory: $PROJECT_DIR"
echo "Analysis and TODO list should be in the project directory"
echo "Slave tasks should be created in $PROJECT_DIR/slave_tasks/"
echo ""
echo "Next steps:"
echo "1. Review the master's analysis and TODO list"
echo "2. Run slave agents with the tasks in slave_tasks/ directory"
echo "3. Monitor progress and iterate as needed"