#!/bin/bash
# open_stats_hq.sh — launch stats-hq (real CSS-styled Harnecient stats
# dashboard) as its own detached X11 process. Same launcher shape as
# open_db_hq.sh (single-instance guard included, same class of bug
# fixed there 2026-08-13 applied here from day one — see
# _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13").
#
# Real fix for the HQ menu's "stats" row (2026-08-13, direct report:
# "stats window still isn't opening... i thot it would open a
# dashboard with css styling"): the row used to shell out to
# compute_stats.sh then try xdg-open on a plain .txt file, unreliable
# in this sandboxed desktop and not what was actually wanted. This
# launches a REAL khtpm CSS-styled window instead, reusing db-hq's own
# generic renderer (khtpm_hq_render.c, unmodified) pointed at a
# dashboard.chtpm regenerated fresh from dashboard.template.chtpm with
# real numbers substituted in every launch (not a static snapshot).
#
# Usage: open_stats_hq.sh <house_root> [session_id]
set -e
HOUSE_ROOT="${1:-}"
SESSION_ID="${2:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_stats_hq: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
# REAL FIX 2026-08-13 (direct live report: "stats window still not
# opening"): the previous resolution used `find -name
# "*livedesk-taskbar"` which matched the HARNESS dir first
# (#.desktop/harnesses/khtpm-livedesk-taskbar, the first find result),
# never the real ops dir - so "$OPS_DIR/+x/khtpm_hq_render.+x" never
# existed and the launch always bailed with "build failed". The house
# standard (open_db_hq.sh/open_event_ez.sh) is the LITERAL relative
# path "$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops" - *.monads and
# *.livedesk-taskbar are literal directory names, not globs.
OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"

BIN="$OPS_DIR/+x/khtpm_hq_render.+x"

if [ ! -x "$BIN" ]; then
    [ -d "$OPS_DIR" ] && (cd "$OPS_DIR" && sh build_db_hq.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_stats_hq: build failed, missing $BIN" >&2
    exit 1
fi

STATS_DIR="$HOUSE_ROOT/%.harnesses/harnecient-fsm"
AI_CELL_DIR="$HOUSE_ROOT/&.widgits/ai-cell"
COMPUTE="$STATS_DIR/compute_stats.sh"

# Use per-session stats if SESSION_ID provided, otherwise aggregate
if [ -n "$SESSION_ID" ]; then
    SUMMARY="$STATS_DIR/session-stats/$SESSION_ID.txt"
    TITLE="Session Stats: $SESSION_ID"
else
    SUMMARY="$STATS_DIR/stats_summary.txt"
    TITLE="Overall Stats"
fi

TEMPLATE="$HERE/dashboard.template.chtpm"
CHTPM="$HERE/dashboard.chtpm"

# Regenerate real numbers fresh every launch - not a cached/stale view.
if [ -z "$SESSION_ID" ] && [ -x "$COMPUTE" ]; then
    bash "$COMPUTE" >/dev/null 2>&1 || true
fi
# For session stats, ensure they're calculated
if [ -z "$SESSION_ID" ] || [ ! -f "$SUMMARY" ]; then
    bash "$AI_CELL_DIR/calculate_session_stats.sh" >/dev/null 2>&1 || true
fi

TABS_FILE="/tmp/stats-hq-tabs-$$.xml"
CONTENT_FILE="/tmp/stats-hq-content-$$.xml"
> "$TABS_FILE"
> "$CONTENT_FILE"

# Read all sessions into array
sessions=()
for stats_file in $(ls -1 "$STATS_DIR/session-stats"/*.txt 2>/dev/null | sort -rn | head -20); do
    [ -f "$stats_file" ] && sessions+=("$stats_file")
done

# Generate tabs
idx=0
for stats_file in "${sessions[@]}"; do
    session_date="$(grep -oE 'Date:\s*[^$]+' "$stats_file" | cut -d: -f2- | xargs || echo 'unknown')"
    active=""
    [ "$idx" -eq 0 ] && active=' class="active"'
    printf '    <tab label="%s"%s/>\n' "$session_date" "$active" >> "$TABS_FILE"
    idx=$((idx + 1))
done

# Generate content for FIRST session only
if [ ${#sessions[@]} -gt 0 ]; then
    stats_file="${sessions[0]}"
    total_rows="$(grep -oE 'Total Turns:\s*[0-9]+' "$stats_file" | grep -oE '[0-9]+$' || echo 0)"
    user_msgs="$(grep -oE 'User Messages:\s*[0-9]+' "$stats_file" | grep -oE '[0-9]+$' || echo 0)"
    ai_msgs="$(grep -oE 'AI Responses:\s*[0-9]+' "$stats_file" | grep -oE '[0-9]+$' || echo 0)"
    tool_calls="$(grep -oE 'Tool Calls Detected:\s*[0-9]+' "$stats_file" | grep -oE '[0-9]+$' || echo 0)"
    session_date="$(grep -oE 'Date:\s*[^$]+' "$stats_file" | cut -d: -f2- | xargs || echo 'unknown')"

    delegation_pct="0.0"
    [ "$total_rows" -gt 0 ] && delegation_pct=$(echo "scale=1; ($tool_calls * 100) / $total_rows" | bc 2>/dev/null || echo "0.0")

    printf '    <title class="block-title" label="Session: %s"/>\n' "$session_date" >> "$CONTENT_FILE"
    printf '    <text class="stat-label" label="User Messages: %s"/>\n' "$user_msgs" >> "$CONTENT_FILE"
    printf '    <text class="stat-label" label="AI Responses: %s"/>\n' "$ai_msgs" >> "$CONTENT_FILE"
    printf '    <text class="stat-value" label="Total Turns: %s"/>\n' "$total_rows" >> "$CONTENT_FILE"
    printf '    <text class="stat-value" label="Tool Calls: %s   Delegation: %s%%"/>\n' "$tool_calls" "$delegation_pct" >> "$CONTENT_FILE"
else
    printf '    <title class="block-title" label="No Sessions"/>\n' >> "$CONTENT_FILE"
    printf '    <text class="caveat-text" label="No session stats found."/>\n' >> "$CONTENT_FILE"
fi

# Generate CHTPM
{
    while IFS= read -r line; do
        case "$line" in
            __SESSION_TABS__)
                cat "$TABS_FILE"
                ;;
            __SESSION_CONTENT__)
                cat "$CONTENT_FILE"
                ;;
            *)
                echo "$line"
                ;;
        esac
    done < "$TEMPLATE"
} > "$CHTPM"

rm -f "$TABS_FILE" "$CONTENT_FILE"

# pgrep exits 1 (nonzero) when nothing matches - guarded with `|| true`
# everywhere under `set -e` (a bare unguarded assignment from a
# failing command substitution silently aborts the whole script - see
# _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13" for the full
# incident this was first found in).
stats_hq_pids() { pgrep -f "khtpm_hq_render\.\+x .*stats-hq/dashboard\.chtpm" 2>/dev/null || true; }

pids="$(stats_hq_pids)"
if [ -n "$pids" ]; then
    echo "open_stats_hq: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(stats_hq_pids)"
    if [ -n "$pids" ]; then
        echo "open_stats_hq: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/stats-hq.log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(stats_hq_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "stats-hq launched (PID $pids, log=/tmp/stats-hq.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "open_stats_hq: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "open_stats_hq: FAILED to launch - check the log:" >&2
    cat /tmp/stats-hq.log 2>/dev/null >&2
    exit 1
fi
