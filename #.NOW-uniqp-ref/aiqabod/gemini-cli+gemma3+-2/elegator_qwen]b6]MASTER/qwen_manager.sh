#!/bin/bash

# Script to manage qwen processes
# Usage: source qwen_manager.sh

# Function to start a qwen agent with a specific prompt
start_qwen_agent() {
    local prompt_file="$1"
    local output_file="$2"
    local project_dir="$3"
    
    if [ -z "$prompt_file" ] || [ -z "$output_file" ] || [ -z "$project_dir" ]; then
        echo "Usage: start_qwen_agent <prompt_file> <output_file> <project_dir>"
        return 1
    fi
    
    echo "Starting qwen agent with prompt: $prompt_file"
    cd "$project_dir" && qwen -f "$prompt_file" > "$output_file" 2>&1 &
    local agent_pid=$!
    
    echo "Started qwen agent with PID: $agent_pid"
    echo "$agent_pid" > "agent_${agent_pid}.pid"
    
    return 0
}

# Function to check if a qwen agent is still running
check_agent_status() {
    local pid="$1"
    
    if [ -z "$pid" ]; then
        echo "Usage: check_agent_status <pid>"
        return 1
    fi
    
    if kill -0 "$pid" 2>/dev/null; then
        echo "Agent $pid is running"
        return 0
    else
        echo "Agent $pid is not running"
        return 1
    fi
}

# Function to kill a qwen agent
kill_agent() {
    local pid="$1"
    
    if [ -z "$pid" ]; then
        echo "Usage: kill_agent <pid>"
        return 1
    fi
    
    if kill -9 "$pid" 2>/dev/null; then
        echo "Killed agent $pid"
        rm -f "agent_${pid}.pid"
        return 0
    else
        echo "Failed to kill agent $pid"
        return 1
    fi
}

# Function to list all running qwen agents
list_agents() {
    echo "Looking for agent PID files..."
    for pid_file in agent_*.pid; do
        if [ -f "$pid_file" ]; then
            local pid=$(cat "$pid_file")
            if check_agent_status "$pid"; then
                echo "Agent $(basename "$pid_file" .pid) with PID $pid is running"
            else
                echo "Agent $(basename "$pid_file" .pid) with PID $pid is not running - cleaning up PID file"
                rm -f "$pid_file"
            fi
        fi
    done
}

# Function to kill all qwen agents
kill_all_agents() {
    echo "Killing all qwen agents..."
    for pid_file in agent_*.pid; do
        if [ -f "$pid_file" ]; then
            local pid=$(cat "$pid_file")
            kill_agent "$pid"
        fi
    done
}

# Function to backup current project state
create_backup() {
    local project_dir="$1"
    local backup_dir="$2"
    
    if [ -z "$project_dir" ] || [ -z "$backup_dir" ]; then
        echo "Usage: create_backup <project_dir> <backup_dir>"
        return 1
    fi
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_name="${backup_dir}/backup_${timestamp}"
    
    echo "Creating backup of $project_dir to $backup_name"
    mkdir -p "$backup_dir"
    cp -r "$project_dir" "$backup_name"
    
    echo "Backup created: $backup_name"
    return 0
}

# Function to log progress
log_progress() {
    local project_dir="$1"
    local message="$2"
    
    if [ -z "$project_dir" ] || [ -z "$message" ]; then
        echo "Usage: log_progress <project_dir> <message>"
        return 1
    fi
    
    local log_file="${project_dir}/progress_reports.txt"
    local timestamp=$(date)
    
    echo "[$timestamp] $message" >> "$log_file"
    echo "Logged: $message"
    return 0
}

echo "Qwen agent management functions loaded."
echo "Available functions: start_qwen_agent, check_agent_status, kill_agent, list_agents, kill_all_agents, create_backup, log_progress"