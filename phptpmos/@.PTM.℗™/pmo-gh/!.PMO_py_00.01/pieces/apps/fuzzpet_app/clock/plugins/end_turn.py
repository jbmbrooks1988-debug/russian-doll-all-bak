#!/usr/bin/env python3
"""
end_turn.py - Increments both turn and time in pieces/clock/state.txt
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
    
    # Process both time and turn
    updated_lines = []
    time_found = False
    turn_found = False
    
    for line in lines:
        if line.startswith("time "):
            # Extract current time
            current_time_str = line[5:].strip()  # Skip "time "
            
            # Parse current time (HH:MM:SS format)
            try:
                hours, minutes, seconds = map(int, current_time_str.split(":"))
            except (ValueError, IndexError):
                # If parsing fails, use default
                hours, minutes, seconds = 8, 0, 0
            
            # Increment by 5 minutes
            minutes += 5
            
            # Handle overflow
            if minutes >= 60:
                minutes -= 60
                hours += 1
                if hours >= 24:
                    hours = hours % 24
            
            # Format the new time
            new_time_str = f"{hours:02d}:{minutes:02d}:{seconds:02d}"
            
            # Update the line
            updated_lines.append(f"time {new_time_str}\n")
            time_updated = True
        elif line.startswith("turn "):
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
    
    # If no time line was found, add one
    if not time_updated:
        updated_lines.append(f"time 08:05:00\n")
    
    # If no turn line was found, add one
    if not turn_updated:
        updated_lines.append(f"turn 1\n")
    
    # Write the updated content back to the file
    with open(state_file, "w") as f:
        f.writelines(updated_lines)

if __name__ == "__main__":
    main()