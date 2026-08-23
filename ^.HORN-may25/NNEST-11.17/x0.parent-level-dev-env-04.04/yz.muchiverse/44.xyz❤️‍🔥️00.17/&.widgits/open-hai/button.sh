#!/bin/bash
# button.sh — launch open-hai (taskbar cell 14 "ai") as its own detached
# X11 process, same launch shape as open_db_hq.sh.
# Usage: button.sh <house_root>
#
# REAL FIX 2026-08-13: this used to launch unconditionally, no matter
# how many instances were already running. Live testing found FIVE
# concurrent khtpm_open_hai_render processes alive at once, all racing
# on the same relay file/session dir - `pkill -9 khtpm_open_hai_render`
# does not reliably match this binary given the emoji-laden house-root
# path in argv, so repeated manual kills silently failed and every
# fresh launch piled another instance on top. Root cause + full
# writeup: _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13".
# FIX: kill any existing instance via `pgrep -f` (matches the FULL
# command line, not just the truncated process name - reliable even
# with the emoji path) before launching, TERM then escalate to KILL,
# and confirm exactly one real PID after launch - same proven pattern
# as *.livedesk-taskbar/ops/run_khtpm_strip.sh's own kill_khtpm().
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open-hai button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/ops"
BIN="$OPS_DIR/+x/khtpm_open_hai_render.+x"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_open_hai.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open-hai button.sh: build failed, missing $BIN" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/&.widgits/open-hai/pieces/audit"
mkdir -p "$AUDIT_DIR"

# NOTE: pgrep exits 1 (nonzero) when it finds nothing - under `set -e`
# a bare `pids="$(open_hai_pids)"` assignment would abort the whole
# script the moment no process is found. Every call site below is
# guarded with `|| true` for exactly this reason - lost an hour to
# this exact silent-abort live before catching it (script would print
# the "killing existing instance" line, then just vanish with no
# further output and no launched process, because set -e killed it
# right after the post-kill pgrep found zero remaining PIDs).
open_hai_pids() { pgrep -f "khtpm_open_hai_render\.\+x" 2>/dev/null || true; }

pids="$(open_hai_pids)"
if [ -n "$pids" ]; then
    echo "open-hai button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(open_hai_pids)"
    if [ -n "$pids" ]; then
        echo "open-hai button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" \
    >"$AUDIT_DIR/open-hai.log" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(open_hai_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "open-hai launched (PID $pids, log=$AUDIT_DIR/open-hai.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "open-hai button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "open-hai button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/open-hai.log" 2>/dev/null >&2
    exit 1
fi
