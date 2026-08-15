#!/bin/bash
# button_taskbar_settings.sh — launch the taskbar HQ menu's "Settings"
# window as its own detached X11 process, same launch shape (build-if-
# missing, single-instance guard via pgrep -f full-cmdline match, TERM
# then KILL escalation, confirm exactly one PID) as ai-cell's own
# button.sh - real fix ported from the 2026-08-13 concurrent-process
# incident (_.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13"), not
# re-invented per-window.
# Usage: button_taskbar_settings.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "taskbar-settings button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

OPS_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$OPS_DIR/+x/khtpm_taskbar_settings_render.+x"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_taskbar_settings.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "taskbar-settings button.sh: build failed, missing $BIN" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/#.desktop/taskbar-settings-audit"
mkdir -p "$AUDIT_DIR"

settings_pids() { pgrep -f "khtpm_taskbar_settings_render\.\+x" 2>/dev/null || true; }

pids="$(settings_pids)"
if [ -n "$pids" ]; then
    echo "taskbar-settings button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(settings_pids)"
    if [ -n "$pids" ]; then
        echo "taskbar-settings button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" \
    >"$AUDIT_DIR/taskbar-settings.log" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(settings_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "taskbar-settings launched (PID $pids, log=$AUDIT_DIR/taskbar-settings.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "taskbar-settings button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "taskbar-settings button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/taskbar-settings.log" 2>/dev/null >&2
    exit 1
fi
