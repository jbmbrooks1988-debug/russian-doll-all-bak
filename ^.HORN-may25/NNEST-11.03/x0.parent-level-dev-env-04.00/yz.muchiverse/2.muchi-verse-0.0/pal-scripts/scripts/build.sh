#!/bin/bash
# scripts/build.sh - pal-scripts/ only needs its own system/prisc+x
# (synced from shared-ops/prisc+x.c) to run any .pal meta-orchestration
# script here - no ops/, no chtpm layer, this directory is CLI-driven
# only (CHAT-INTEGRATION-ARCHITECTURE.txt sec. 6's "meta-orchestration
# llm scripts/ folder", now real - see pal-chat-agent.pal's own header
# comment for the first concrete script).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

CFLAGS="-Wall -Wextra -O2"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
echo "--- Build Complete ---"
ls -l system/prisc+x
