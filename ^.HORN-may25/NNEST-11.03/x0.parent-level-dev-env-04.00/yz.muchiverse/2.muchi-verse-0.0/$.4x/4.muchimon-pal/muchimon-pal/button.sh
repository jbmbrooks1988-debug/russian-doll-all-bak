#!/bin/bash
# button.sh - launcher for muchimon-pal, same verb convention as TPMOS's
# button.sh (c/compile, r/run, k/kill...).
#
# No ncurses anywhere: "run" starts these processes, same shape as real
# TPMOS -
#   - system/keyboard_input : owns the real terminal in raw mode, reads
#     keys itself, appends bare keycodes to history.txt. Runs in the
#     foreground since it's the one that needs the controlling tty.
#   - system/prisc+x pal/main_loop.pal : reads history.txt, dispatches
#     to move_player/end_turn/compose_frame ops, halts itself on 'q'.
#   - system/renderer : cooked-mode stdout writer, polls the frame
#     pulse marker, prints current_frame.txt, logs every frame to
#     frame_history.txt for audit. Backgrounded.
#   - system/gl_mirror : a GLUT window that blits whatever
#     ops/compose_rgb_frame.+x (wired into pal/main_loop.pal alongside
#     compose_frame, so it updates every gameplay tick) writes to
#     pieces/display/rgb_frame.raw - a plain RGBA32 buffer computed by
#     portable CPU C, zero GL calls anywhere except gl_mirror.c itself
#     (see GOVERNING CONSTRAINT in 2.muchi-verse/GRAND-ARCHITECTURE.md).
#     Same "one state, two renderers" mirror the ASCII path already is;
#     this one just also happens to open a window, and per direct
#     intent it pops up automatically alongside the terminal - it's a
#     SECOND LIVE INPUT SOURCE usable simultaneously with the terminal
#     (real keyboard/arrow forwarding via its own GLUT callbacks -
#     confirmed working via direct synthetic-key testing once the
#     window has real focus). "run" launches it automatically if
#     system/gl_mirror was actually built (best-effort in
#     scripts/build.sh - some environments lack GLUT dev libs/a
#     display, and the window failing to open there is not fatal to
#     the rest of "run"). Set NO_GL=1 to skip launching it explicitly
#     (e.g. for a headless/no-DISPLAY test run).
# "run" tracks every background PID it started and kills them (per the
# cdda-tpm-std-fast.txt rule: never leave an untracked subprocess
# running) once keyboard_input exits.
#
# "gl" below is a way to (re-)launch just the mirror window standalone
# - useful if "run" was started with NO_GL=1, or without a display
# available at the time, or after the window was closed without
# quitting the whole game. gl_mirror.c's own receipt-writing
# (pieces/display/gl_display.receipt.txt) is how its correctness gets
# verified either way, not the window itself.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        mkdir -p pieces/system pieces/display pieces/apps/player_app
        : > pieces/display/message_log.txt
        : > pieces/system/quit_flag.txt
        : > pieces/apps/player_app/history.txt
        # Defensive cleanup, matching real 1.TPMOS's run_chtpm.sh's own
        # documented "JOYSTICK FIX": a stale gl_focus.lock left behind by
        # an abnormally-terminated previous gl_mirror (crash, kill -9, a
        # SIGHUP gl_mirror now catches but didn't used to) is harmless to
        # keyboard_input.c now (it no longer checks this file at all -
        # see that file's own header comment on why), but gl_mirror.c
        # itself still writes/reads it, so start from a known-clean state
        # regardless.
        rm -f pieces/system/gl_focus.lock

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="muchimon-pal"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/prisc+x pal/main_loop.pal >/dev/null 2>&1 &
        PRISC_PID=$!

        GL_PID=""
        if [ -z "$NO_GL" ] && [ -x "system/gl_mirror" ]; then
            ./system/gl_mirror >/tmp/muchimon-pal_gl_mirror.log 2>&1 &
            GL_PID=$!
            echo "GL/RGB mirror window launching (pid $GL_PID) - a second live" >&2
            echo "input source alongside this terminal. Click it to give it real" >&2
            echo "OS keyboard focus if you want to control the game from there;" >&2
            echo "the terminal keeps working regardless (arrow-key movement only" >&2
            echo "applies once you're past the title screen, which is digit+Enter" >&2
            echo "only by design). If no window appears (no DISPLAY/GLUT), see" >&2
            echo "/tmp/muchimon-pal_gl_mirror.log - the terminal session below is" >&2
            echo "unaffected either way." >&2
        fi

        trap 'kill "$RENDERER_PID" "$PRISC_PID" $GL_PID 2>/dev/null' EXIT INT TERM

        ./system/keyboard_input

        kill "$RENDERER_PID" "$PRISC_PID" $GL_PID 2>/dev/null
        ;;
    gl|mirror)
        cd "$SCRIPT_DIR"
        if [ ! -x "system/gl_mirror" ]; then
            echo "system/gl_mirror not built (GLUT/GL may not be available - see scripts/build.sh output)"
            exit 1
        fi
        mkdir -p pieces/display
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="muchimon-pal"
        echo "Launching gl_mirror standalone - './button.sh run' now launches this"
        echo "automatically, so you only need this verb to (re-)launch just the"
        echo "window on its own (e.g. after closing it without quitting the game,"
        echo "or on a NO_GL=1 run). Make sure './button.sh run' is also running"
        echo "(or was run first) so pieces/display/rgb_frame.raw actually gets"
        echo "updated each tick. Correctness is verifiable via"
        echo "pieces/display/gl_display.receipt.txt and rgb_frame.receipt.txt"
        echo "without needing to see the window."
        ./system/gl_mirror
        ;;
    generate|gen)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="muchimon-pal"
        shift
        ./ops/+x/generate_map.+x "$@"
        ;;
    kill|k|stop)
        echo "=== Killing muchimon-pal processes ==="
        pkill -f "$SCRIPT_DIR/system/keyboard_input" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/renderer" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/prisc\+x" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/gl_mirror" 2>/dev/null
        sleep 0.2
        rm -f "$SCRIPT_DIR/pieces/system/gl_focus.lock"
        echo "done"
        ;;
    check|verify)
        # R0-R5 required set
        missing=0
        for b in system/prisc+x system/keyboard_input system/renderer \
                 ops/+x/move_player.+x ops/+x/end_turn.+x ops/+x/compose_frame.+x ops/+x/pdl_reader.+x \
                 ops/+x/choice.+x ops/+x/compose_rgb_frame.+x \
                 ops/+x/talk.+x ops/+x/activity.+x ops/+x/scene_reader.+x \
                 ops/+x/pickup.+x ops/+x/drop.+x \
                 ops/+x/save_game.+x ops/+x/title_input.+x ops/+x/compose_title_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
                missing=1
            fi
        done
        if [ "$missing" -ne 0 ]; then exit 1; fi
        if [ -x "$SCRIPT_DIR/system/gl_mirror" ]; then
            echo "OK   system/gl_mirror"
        else
            echo "SKIP system/gl_mirror (optional - needs GLUT/GL, see scripts/build.sh output)"
        fi
        ;;
    help|h|-h|--help)
        echo "muchimon-pal button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries (prisc+x, keyboard_input, renderer, ops)"
        echo "  run, r, start       - Run the game (keyboard_input + prisc+x + renderer +"
        echo "                        gl_mirror, if built - set NO_GL=1 to skip the GL window)"
        echo "  gl, mirror          - (Re-)launch just the GL/RGB mirror window standalone"
        echo "  kill, k, stop       - Kill any lingering muchimon-pal processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  generate, gen <map_id> <seed> [w] [h] [link_map_id] [link_x] [link_y]"
        echo "                      - Procedurally generate a new map (authoring-time"
        echo "                        tool, deterministic per seed) and wire a"
        echo "                        bidirectional stairway back into an existing map"
        echo "                        (defaults: 80x40, linked from map_02 at (36,13))"
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
