#!/usr/bin/env python3
"""
increment_turn.py - Increments turn in pieces/clock/state.txt by 1
Based on the proven C implementation design
"""

import os
import sys

def main():
    # Ensure clock piece directory exists
    os.makedirs("pieces/clock", exist_ok=True)
    
    # Read the current state file
    state_file = "pieces/clock/state.txt"
    
    # Initialize with default state if file doesn't exist
    if not os.path.exists(state_file):
        with open(state_file, "w") as f:
            f.write("turn 0\ntime 08:00:00\n")
    
    with open(state_file, "r") as f:
        lines = f.readlines()
    
    # Process lines to find and update turn
    updated_lines = []
    turn_found = False
    
    for line in lines:
        if line.startswith("turn "):
            # Extract current turn
            current_turn_str = line[5:].strip()  # Skip "turn "
            
            try:
                current_turn = int(current_turn_str)
            except ValueError:
                # If parsing fails, use default
                current_turn = 0
            
            # Increment turn
            new_turn = current_turn + 1
            
            # Update the line
            updated_lines.append(f"turn {new_turn}\n")
            turn_updated = True
        else:
            updated_lines.append(line)
    
    # If no turn line was found, add one
    if not turn_updated:
        updated_lines.append(f"turn 1\n")
    
    # Write the updated content back to the file
    with open(state_file, "w") as f:
        f.writelines(updated_lines)

if __name__ == "__main__":
    main()