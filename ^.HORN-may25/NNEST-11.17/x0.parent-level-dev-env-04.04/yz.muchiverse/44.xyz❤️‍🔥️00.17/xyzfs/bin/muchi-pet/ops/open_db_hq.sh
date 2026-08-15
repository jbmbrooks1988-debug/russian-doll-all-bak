#!/bin/bash
# open_db_hq.sh — launch db-hq (the CSS-styled database window) as its own
# detached X11 process, same launch shape as open_event_ez.sh.
# Usage: open_db_hq.sh <house_root>
#
# REAL FIX 2026-08-13: this used to launch unconditionally, no matter
# how many db-hq instances were already running. The SAME live bug was
# found+fixed first in ai-cell's button.sh (five concurrent processes
# all racing on shared state, root cause + full writeup in
# _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13") - applying the
# identical proven fix here rather than waiting to hit it live in
# db-hq too. db-hq is single-instance-per-house (one dashboard window
# at a time is the intended UX, matching ai-cell/taskbar), so a
# blanket kill-before-launch by binary name is correct here - unlike
# events-hq, which is legitimately multi-instance (one per entity).
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_db_hq: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_hq_render.+x"
CHTPM="$HOUSE_ROOT/&.hq-apps/db-hq/dashboard.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_db_hq.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_db_hq: build failed, missing $BIN" >&2
    exit 1
fi

mkdir -p "$HOUSE_ROOT/common_events"

# pgrep exits 1 (nonzero) when nothing matches - guarded with `|| true`
# everywhere under `set -e` (a bare unguarded assignment from a failing
# command substitution silently aborts the whole script - lost real
# time to this exact bug fixing ai-cell's button.sh first).
db_hq_pids() { pgrep -f "khtpm_hq_render\.\+x" 2>/dev/null || true; }

pids="$(db_hq_pids)"
if [ -n "$pids" ]; then
    echo "open_db_hq: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(db_hq_pids)"
    if [ -n "$pids" ]; then
        echo "open_db_hq: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/db-hq.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(db_hq_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "db-hq launched (PID $pids, log=/tmp/db-hq.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "open_db_hq: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "open_db_hq: FAILED to launch - check the log:" >&2
    cat /tmp/db-hq.log 2>/dev/null >&2
    exit 1
fi
