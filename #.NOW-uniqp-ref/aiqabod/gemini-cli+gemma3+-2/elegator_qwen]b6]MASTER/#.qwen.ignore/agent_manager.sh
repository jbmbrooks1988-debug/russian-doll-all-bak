#!/bin/bash

# Agent manager script for the_elegator_qwen
# This script handles starting, monitoring, and managing qwen agent processes

AGENT_PID_FILE="current_agents.pid"
MAX_AGENTS=3

# Function to start a qwen agent
start_agent() {
    local agent_num=$1
    local project_dir=$2
    local prompt_file=$3
    
    if [ $agent_num -ge $MAX_AGENTS ]; then
        echo "Maximum number of agents ($MAX_AGENTS) reached"
        return 1
    fi
    
    echo "Starting agent $agent_num for project: $project_dir"
    
    # Change to project directory and run qwen
    cd "$project_dir" && {
        qwen -f "$prompt_file" > "agent_${agent_num}_output.txt" 2>&1 &
        local agent_pid=$!
        
        # Record the agent PID
        echo "$agent_pid" >> "$AGENT_PID_FILE"
        echo "Started agent $agent_num with PID: $agent_pid"
        
        # Wait for the agent to complete
        wait $agent_pid
        local exit_code=$?
        
        echo "Agent $agent_num completed with exit code: $exit_code"
        # Remove the PID from the file
        sed -i "/^$agent_pid$/d" "$AGENT_PID_FILE" 2>/dev/null || true
        
        return $exit_code
    }
}

# Function to check if an agent is running
is_agent_running() {
    local pid=$1
    kill -0 $pid 2>/dev/null
}

# Function to kill all running agents
kill_all_agents() {
    if [ -f "$AGENT_PID_FILE" ]; then
        while IFS= read -r pid; do
            if [ ! -z "$pid" ] && is_agent_running $pid; then
                echo "Killing agent with PID: $pid"
                kill -TERM $pid 2>/dev/null
                # Wait a bit before force killing
                sleep 1
                if is_agent_running $pid; then
                    kill -KILL $pid 2>/dev/null
                fi
            fi
        done < "$AGENT_PID_FILE"
        
        # Clear the PID file
        > "$AGENT_PID_FILE"
    fi
}

# Function to count running agents
count_running_agents() {
    local count=0
    if [ -f "$AGENT_PID_FILE" ]; then
        while IFS= read -r pid; do
            if [ ! -z "$pid" ] && is_agent_running $pid; then
                ((count++))
            fi
        done < "$AGENT_PID_FILE"
    fi
    echo $count
}

# Function to create a backup of the project
create_backup() {
    local project_dir=$1
    local backup_dir="${project_dir}/backup"
    
    # Create backup directory if it doesn't exist
    mkdir -p "$backup_dir"
    
    # Create timestamped backup
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_name="${backup_dir}/backup_${timestamp}"
    
    echo "Creating backup: $backup_name"
    cp -r "$project_dir"/* "$backup_name" 2>/dev/null || true
    cp -r "$project_dir"/.[^.]* "$backup_name" 2>/dev/null || true  # Include hidden files except . and ..
    
    echo "Backup created: $backup_name"
}

# Function to log progress
log_progress() {
    local project_dir=$1
    local message=$2
    local timestamp=$(date)
    
    echo "[$timestamp] $message" >> "${project_dir}/progress_reports.txt"
    echo "Logged: $message"
}

# Function to check if context is too strained/corrupted/stuck
check_context_health() {
    local project_dir=$1
    local agent_output_file=$2
    
    # Check if the agent output indicates problems
    if [ -f "$agent_output_file" ]; then
        # Check for common failure indicators
        if grep -i -E "error|failed|cannot|unable|stuck|corrupted|context.*strai" "$agent_output_file" > /dev/null; then
            echo "Context health check failed - potential issues detected"
            return 1
        fi
    fi
    
    # If no issues found
    echo "Context appears healthy"
    return 0
}

# Function to summarize progress for a new agent
summarize_progress() {
    local project_dir=$1
    local summary_file="${project_dir}/progress_summary.txt"
    
    echo "=== PROGRESS SUMMARY ===" > "$summary_file"
    echo "Date: $(date)" >> "$summary_file"
    echo "Project: $project_dir" >> "$summary_file"
    echo "" >> "$summary_file"
    
    if [ -f "${project_dir}/progress_reports.txt" ]; then
        echo "Recent Progress Reports:" >> "$summary_file"
        tail -20 "${project_dir}/progress_reports.txt" >> "$summary_file"
    fi
    
    echo "" >> "$summary_file"
    echo "Recent Agent Outputs:" >> "$summary_file"
    for output_file in "${project_dir}"/agent_*_output.txt; do
        if [ -f "$output_file" ]; then
            echo "--- $(basename "$output_file") ---" >> "$summary_file"
            tail -10 "$output_file" >> "$summary_file"
            echo "" >> "$summary_file"
        fi
    done
    
    echo "Summary created: $summary_file"
}