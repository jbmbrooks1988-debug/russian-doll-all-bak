#!/bin/sh
# EMERGENCY_KILL.sh - FAST nuclear kill of project binaries.
#
# Uses pkill -x (exact process NAME) — not cmdline scan, not /proc walk,
# not workspace-wide find. Safe vs Chrome (process name is "chrome").

echo "EMERGENCY KILL..."

# TERM first (quick), then KILL — no long grace if nothing was up
HIT=0
for name in orchestrator keyboard_input chtpm_parser_pal chtpm_rgb_render \
            gl_mirror egg_window avatar_window renderer 'prisc+x'; do
    if pgrep -x "$name" >/dev/null 2>&1; then
        pkill -x -TERM "$name" 2>/dev/null || true
        HIT=1
    fi
done

if [ "$HIT" = "1" ]; then
    sleep 0.1
    for name in orchestrator keyboard_input chtpm_parser_pal chtpm_rgb_render \
                gl_mirror egg_window avatar_window renderer 'prisc+x'; do
        pkill -x -KILL "$name" 2>/dev/null || true
    done
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Targeted stale-file cleanup only (NOT find over whole tree)
for rel in \
    "101.mutaclsym🧟‍♂️️+18.01/debug.txt" \
    "101.mutaclsym🧟‍♂️️+18.01/pieces/display/gl_key_debug.log" \
    "101.mutaclsym🧟‍♂️️+18.01/pieces/os/proc_list.txt"
do
    : > "$SCRIPT_DIR/$rel" 2>/dev/null || true
done

# Common project roots for window.pid / quit_flag (shallow, fixed paths)
for root in \
    "$SCRIPT_DIR/0.user-pal👤️/01.avatar-creation👤️" \
    "$SCRIPT_DIR/01.muchi-pals-🥚️-13.01" \
    "$SCRIPT_DIR/101.mutaclsym🧟‍♂️️+18.01"
do
    [ -d "$root" ] || continue
    rm -f "$root/pieces/world_01/map_lobby"/*/window.pid 2>/dev/null || true
    rm -f "$root/pieces/system/quit_flag.txt" 2>/dev/null || true
    rm -f "$root/pieces/system/gl_focus.lock" 2>/dev/null || true
    : > "$root/pieces/system/avatar_window_pids.txt" 2>/dev/null || true
done

left=""
for name in orchestrator keyboard_input chtpm_parser_pal chtpm_rgb_render \
            gl_mirror egg_window avatar_window renderer 'prisc+x'; do
    if pgrep -x "$name" >/dev/null 2>&1; then
        left="$left $name"
    fi
done
if [ -n "$left" ]; then
    echo "WARNING still running:$left"
else
    echo "All project processes terminated."
fi
echo "Done."
