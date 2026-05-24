#!/bin/sh

# Updated script to unbind (kill process listening on) a given port on any local interface.
# Usage: ./unbind_port.sh <port_number>
# Requires: lsof command (common on Unix-like systems).
# Now searches for any IP binding (not just localhost) on TCP LISTEN.

PORT=${1:?Usage: $0 <port>}

# Find the PID of the process listening on PORT (TCP only, take first if multiple)
PID=$(lsof -ti:$PORT -sTCP:LISTEN -n -P 2>/dev/null | head -1)

if [ -z "$PID" ]; then
    echo "No process found listening on port $PORT"
    # Fallback: Try ss command if available (common on Linux)
    SS_PID=$(ss -tlnp 2>/dev/null | grep ":$PORT " | awk '{print $7}' | cut -d, -f2 | cut -d= -f2 | head -1)
    if [ -n "$SS_PID" ]; then
        echo "Found via ss: Killing PID $SS_PID"
        kill -9 "$SS_PID"
        exit 0
    fi
    # Another fallback: netstat (older systems)
    NETSTAT_PID=$(netstat -tlnp 2>/dev/null | grep ":$PORT " | awk '{print $7}' | cut -d/ -f1 | head -1)
    if [ -n "$NETSTAT_PID" ]; then
        echo "Found via netstat: Killing PID $NETSTAT_PID"
        kill -9 "$NETSTAT_PID"
        exit 0
    fi
    echo "No process found on port $PORT with any tool. Check with 'lsof -i :$PORT' or 'ss -tlnp | grep :$PORT' manually."
    exit 1
fi

# Kill the process forcefully
kill -9 "$PID"
echo "Killed PID $PID listening on port $PORT"
