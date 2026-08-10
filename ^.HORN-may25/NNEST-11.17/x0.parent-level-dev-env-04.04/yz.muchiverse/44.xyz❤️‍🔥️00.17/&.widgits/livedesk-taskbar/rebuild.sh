#!/bin/bash
# Rebuild taskbar binary from source and restart
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
gcc -std=c11 -Wall -O2 ops/tp_taskbar.c -o ops/+x/tp_taskbar.+x -lX11 && echo "✓ Taskbar recompiled" || exit 1
# Kill running taskbar so it restarts with new binary
pkill -f "tp_taskbar.+x" 2>/dev/null || true
