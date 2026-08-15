#!/bin/bash
# button_taskbar_stats.sh — launch the taskbar HQ menu's "Stats"
# window as its own detached X11 process (same launch shape as settings).
# Usage: button_taskbar_stats.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "taskbar-stats button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

STATS_SCRIPT="$HOUSE_ROOT/&.hq-apps/stats-hq/open_stats_hq.sh"

if [ ! -x "$STATS_SCRIPT" ]; then
    echo "taskbar-stats button.sh: missing $STATS_SCRIPT" >&2
    exit 1
fi

stats_pids() { pgrep -f "khtpm_hq_render\.\+x.*stats-hq/dashboard\.chtpm" 2>/dev/null || true; }

pids="$(stats_pids)"
if [ -n "$pids" ]; then
    echo "taskbar-stats button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(stats_pids)"
    if [ -n "$pids" ]; then
        echo "taskbar-stats button.sh: still alive after TERM, escalating to KILL"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup bash "$STATS_SCRIPT" "$HOUSE_ROOT" \
    >/tmp/taskbar-stats.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(stats_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "taskbar-stats launched (PID $pids)"
fi
