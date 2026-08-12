#!/bin/bash
# open_db_hq.sh — launch db-hq (the CSS-styled database window) as its own
# detached X11 process, same launch shape as open_event_ez.sh.
# Usage: open_db_hq.sh <house_root>
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

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/db-hq.log 2>&1 < /dev/null &
disown 2>/dev/null || true
echo "db-hq launched (log=/tmp/db-hq.log)"
