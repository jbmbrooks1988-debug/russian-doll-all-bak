#!/bin/bash
# button.sh - launcher for mutaclsym, same verb convention as TPMOS's
# button.sh (c/compile, r/run, k/kill...).
#
# No ncurses anywhere: "run" starts three independent processes, same
# shape as real TPMOS -
#   - system/keyboard_input : owns the real terminal in raw mode, reads
#     keys itself, appends bare keycodes to history.txt. Runs in the
#     foreground since it's the one that needs the controlling tty.
#   - system/prisc+x pal/main_loop.pal : reads history.txt, dispatches
#     to move_player/end_turn/compose_frame ops, halts itself on 'q'.
#   - system/renderer : cooked-mode stdout writer, polls the frame
#     pulse marker, prints current_frame.txt, logs every frame to
#     frame_history.txt for audit. Backgrounded.
# "run" tracks both background PIDs and kills them (per the
# cdda-tpm-std-fast.txt rule: never leave an untracked subprocess
# running) once keyboard_input exits.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        mkdir -p pieces/system pieces/display pieces/apps/player_app
        : > pieces/system/quit_flag.txt
        : > pieces/apps/player_app/history.txt

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="mutaclsym"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/prisc+x pal/main_loop.pal >/dev/null 2>&1 &
        PRISC_PID=$!
        trap 'kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null' EXIT INT TERM

        ./system/keyboard_input

        kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null
        ;;
    kill|k|stop)
        echo "=== Killing mutaclsym processes ==="
        pkill -f "$SCRIPT_DIR/system/keyboard_input" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/renderer" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/prisc\+x" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 ops/+x/move_player.+x ops/+x/end_turn.+x ops/+x/compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        ;;
    help|h|-h|--help)
        echo "mutaclsym button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries (prisc+x, keyboard_input, renderer, ops)"
        echo "  run, r, start       - Run the game (keyboard_input + prisc+x + renderer)"
        echo "  kill, k, stop       - Kill any lingering mutaclsym processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        echo ""
        echo "Recommended workflow:"
        echo "  1. ./button.sh compile"
        echo "  2. ./button.sh check"
        echo "  3. ./button.sh run"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
