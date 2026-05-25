# The_Elegator_Qwen System

A system for managing AI agents to work on programming tasks, with the ability to start, monitor, and restart AI tasks as needed.

## Overview

The_elegator_qwen is designed to:
- Start AI agents with specific prompts and project contexts
- Monitor their progress and output
- Restart agents if the context becomes strained/corrupted/stuck
- Maintain backup files and progress reports
- Support up to 3 concurrent agents for complex tasks

## Components

### 1. Main Elegator Program (`elegator.c`)
- Written in C for efficiency and portability
- Manages the overall workflow
- Coordinates with AI agents
- Handles backup and progress tracking

### 2. Template Prompt System (`elegator_template_prompt.txt`)
- Reusable prompt template for different projects
- Contains guidelines for AI agents
- Supports project-specific customization

### 3. Agent Management Scripts
- `agent_manager.sh`: Handles starting, monitoring, and stopping AI agents
- `qwen_manager.sh`: Additional functions for qwen-specific operations
- `compile_elegator.sh`: Compiles the main elegator program

### 4. Backup and Progress Tracking
- Automatic backup creation before major changes
- Progress reports in `progress_reports.txt`
- Numbered backup files for easy reference

## Setup

1. Compile the elegator program:
   ```bash
   ./compile_elegator.sh
   ```

2. Ensure you have access to the qwen CLI tool (or modify for ollama/gemini as needed)

## Usage

### Basic Usage
```bash
./elegator <project_path> <task_description>
```

### Example with gitlet project
```bash
./run_elegator_gitlet.sh
```

### Template Usage for Other Projects
1. Create a project directory with your code
2. Modify `elegator_template_prompt.txt` if needed for your specific project
3. Run the elegator with your project path and task:
   ```bash
   ./elegator /path/to/your/project "Your task description here"
   ```

## Features

- **Agent Management**: Starts specialized agents ('employolo_#') for specific tasks
- **Context Monitoring**: Detects when an agent's context becomes corrupted or stuck
- **Backup System**: Creates timestamped backups before major changes
- **Progress Tracking**: Maintains detailed logs in `progress_reports.txt`
- **Flexible Architecture**: Template system allows reuse across different projects

## Architecture

```
User Input
    ↓
Elegator (C program)
    ↓
Agent Manager (Bash scripts)
    ↓
AI Agent (qwen/ollama/gemini)
    ↓
Project Directory
    ↓
Backup & Progress Tracking
```

## Configuration

The system can be adapted for different AI backends:
- For qwen: Use the default configuration
- For ollama: Modify the agent execution commands
- For other models: Adjust the command execution in the C code

all models should use #.config/!.elegator_user_config.txt to derive best practices

## File Structure

- `elegator.c`: Main program logic
- `elegator_template_prompt.txt`: Reusable prompt template
- `agent_manager.sh`: Agent process management
- `qwen_manager.sh`: Additional qwen-specific functions
- `compile_elegator.sh`: Build script
- `run_elegator_gitlet.sh`: Example runner for gitlet project
- `test_elegator_system.sh`: Verification script
- `backup/`: Directory for backup files
- `progress_reports.txt`: Log of all activities

## Testing

Run the test script to verify the system is working:
```bash
./test_elegator_system.sh
```

## Future Enhancements

- Integration with ollama and other AI backends
- Advanced context health checks
- Web-based monitoring interface
- Integration with gitlet for advanced version control
- Support for recursive elegator management