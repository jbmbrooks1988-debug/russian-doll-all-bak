#!/bin/sh
# EMERGENCY_KILL.sh - Kill all runaway LPNS processes across both +3 directories
# Use when games hang or processes won't die normally

echo "EMERGENCY KILL - Terminating all LPNS processes..."

# Kill with -9 (SIGKILL) to force termination
pkill -9 -f "system/renderer" 2>/dev/null || true
pkill -9 -f "system/keyboard_input" 2>/dev/null || true
pkill -9 -f "prisc\+x.*pal" 2>/dev/null || true
pkill -9 -f "chtpm_parser_pal" 2>/dev/null || true

# Reset quit flags
find /home/no/Desktop -path "*lpns*" -path "*+3" -name "quit_flag.txt" -delete 2>/dev/null || true

# Wait a moment
sleep 0.2

# Verify all dead
if ps aux | grep -E "system/(renderer|keyboard_input)|prisc\+x" | grep -v grep >/dev/null 2>&1; then
    echo "WARNING: Some processes still running"
    ps aux | grep -E "system/(renderer|keyboard_input)|prisc\+x" | grep -v grep
else
    echo "✓ All processes terminated"
fi

echo "Done."
