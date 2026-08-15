#!/bin/bash
# button.sh — launch chat-hai (taskbar cell 14 "h-ai" Chat submenu) as its own
# detached X11 process. Follows exact ai-cell pattern but passes chtpm path.
# Usage: button.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "chat-hai button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/ops"
BIN="$OPS_DIR/+x/chat_hai_hq_render.+x"
CHTPM="$HERE/chat-hai.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_chat_hai.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "chat-hai button.sh: build failed, missing $BIN" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/&.hq-apps/chat-hai/pieces/audit"
mkdir -p "$AUDIT_DIR"

LOOP="$OPS_DIR/chat_hai_loop.sh"
loop_pids() { pgrep -f "chat_hai_loop\.sh" 2>/dev/null || true; }

# REAL FIX 2026-08-15: this function used to launch ONLY the renderer -
# no persona actually chats without chat_hai_loop.sh also running,
# leaving a real, correctly-drawn window with an empty/frozen feed and
# no visible symptom pointing at "the loop never started." Single-
# instance guard here too (same class of bug this file's own header
# comment already documents fixing for ai-cell) - if the loop's already
# up (e.g. this button.sh is being re-run just to bring the window back
# after a close), leave it running rather than restarting it and losing
# in-flight round state for no reason.
if [ -z "$(loop_pids)" ]; then
    setsid nohup bash "$LOOP" >/dev/null 2>&1 &
    disown 2>/dev/null || true
    sleep 1
    echo "chat-hai button.sh: chat loop started (pid $(loop_pids | tr '\n' ' '))"
else
    echo "chat-hai button.sh: chat loop already running (pid $(loop_pids | tr '\n' ' '))"
fi

chat_hai_pids() { pgrep -f "chat_hai_hq_render\.\+x" 2>/dev/null || true; }

pids="$(chat_hai_pids)"
if [ -n "$pids" ]; then
    echo "chat-hai button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(chat_hai_pids)"
    if [ -n "$pids" ]; then
        echo "chat-hai button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >"$AUDIT_DIR/chat-hai.log" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(chat_hai_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "chat-hai launched (PID $pids, log=$AUDIT_DIR/chat-hai.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "chat-hai button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "chat-hai button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/chat-hai.log" 2>/dev/null >&2
    exit 1
fi
