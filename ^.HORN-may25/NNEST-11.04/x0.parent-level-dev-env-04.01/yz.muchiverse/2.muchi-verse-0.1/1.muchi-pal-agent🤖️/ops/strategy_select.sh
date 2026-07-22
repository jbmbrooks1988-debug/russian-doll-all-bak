#!/bin/bash
# strategy_select.sh - pre-parse user input, select strategy (A/B/C), return action
# Sets env vars for the calling process to decide what to do next
# Strategy A: pre-execute tool
# Strategy B: send to Gemma with TOOL: instructions
# Strategy C: respond locally without Gemma

set -e

PROJECT_ROOT="${PRISC_PROJECT_ROOT:-.}"

# Read weights
read_weight() {
    local key="$1"
    local weights_file="$PROJECT_ROOT/pieces/registry/strategy_weights.txt"
    grep "^${key}=" "$weights_file" 2>/dev/null | cut -d'=' -f2 || echo "0"
}

# Parse user input to detect what tool is needed
detect_tool() {
    local input="$1"
    local input_lower=$(echo "$input" | tr '[:upper:]' '[:lower:]')

    # Return: TOOL_NAME or "none"
    if echo "$input_lower" | grep -qE 'list|dir|files|ls|show'; then
        echo "list_dir"
    elif echo "$input_lower" | grep -qE 'read|view|cat|open|contents|display'; then
        echo "read_file"
    elif echo "$input_lower" | grep -qE 'write|create|save|new file'; then
        echo "write_file"
    elif echo "$input_lower" | grep -qE 'run|exec|execute|command|cmd|shell'; then
        echo "exec_cmd"
    elif echo "$input_lower" | grep -qE 'search|find|grep'; then
        echo "search_in_files"
    elif echo "$input_lower" | grep -qE 'edit|replace|change|modify'; then
        echo "edit_file"
    elif echo "$input_lower" | grep -qE 'speak|say|voice'; then
        echo "speak"
    elif echo "$input_lower" | grep -qE 'search.*web|google|internet'; then
        echo "web_search"
    else
        echo "none"
    fi
}

# Detect meta requests (Strategy C triggers)
detect_meta() {
    local input="$1"
    local input_lower=$(echo "$input" | tr '[:upper:]' '[:lower:]')

    if echo "$input_lower" | grep -qE '(^|[^a-z])help(\s|$)|^\?|what (tools|can|features|capabilities)|^(what|list) (tools|capabilities)|^tell.*tools'; then
        echo "meta_help"
    elif echo "$input_lower" | grep -qE '^what.*(state|status)|^(status|state|current state)'; then
        echo "meta_status"
    elif echo "$input_lower" | grep -qE '^(show|display).*(history|log|conversation)|^(history|log|conversation)\?'; then
        echo "meta_history"
    else
        echo "none"
    fi
}

# Weighted random selection
weighted_choice() {
    local w_a=$(read_weight "strategy_a")
    local w_b=$(read_weight "strategy_b")
    local w_c=$(read_weight "strategy_c")
    local total=$(echo "$w_a + $w_b + $w_c" | bc)

    if [ "$total" = "0" ]; then
        echo "B"  # fallback
        return
    fi

    local rand=$((RANDOM % 100))
    local cum_a=$(echo "scale=0; $w_a * 100 / $total" | bc)
    local cum_b=$(echo "scale=0; ($w_a + $w_b) * 100 / $total" | bc)

    if [ "$rand" -lt "$cum_a" ]; then
        echo "A"
    elif [ "$rand" -lt "$cum_b" ]; then
        echo "B"
    else
        echo "C"
    fi
}

# Main
main() {
    if [ $# -lt 1 ]; then
        echo "Usage: strategy_select.sh <user_input>"
        exit 1
    fi

    local user_input="$1"

    # Check for meta requests first (always use Strategy C)
    local meta=$(detect_meta "$user_input")
    if [ "$meta" != "none" ]; then
        echo "strategy=C"
        echo "meta_type=$meta"
        return 0
    fi

    # Detect tool needed
    local tool=$(detect_tool "$user_input")
    echo "detected_tool=$tool"

    # If no tool detected, force Strategy B (let Gemma try)
    if [ "$tool" = "none" ]; then
        echo "strategy=B"
        return 0
    fi

    # Tool detected: choose strategy (A or B weighted)
    local strategy=$(weighted_choice)
    echo "strategy=$strategy"
}

main "$@"
