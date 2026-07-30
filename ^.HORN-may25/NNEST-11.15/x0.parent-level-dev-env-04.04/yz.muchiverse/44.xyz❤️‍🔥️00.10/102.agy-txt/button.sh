#!/bin/bash
# button.sh - launcher for agy-txt (self-contained editor, PLAN.md
# Phase T2 stub). Adapted from 102.editor-📄️00.00/button.sh's own
# real, proven session-isolation shape - same pattern, own project.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        PROFILE="${RUN_PROFILE:-}"
        if [ -z "$PROFILE" ]; then
            if [ -z "$DISPLAY" ]; then
                PROFILE="app"
            else
                PROFILE="widget"
            fi
        fi
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard"
        # docs/ deliberately NOT pre-created (PITFALL 62) - `ln -sfn`
        # below needs SESSION_DIR/docs to not already exist as a real
        # directory, or it nests the symlink one level too deep instead
        # of replacing it, silently routing every relative "docs/..."
        # SAVE_AS into an ephemeral session-local directory that gets
        # rm -rf'd on exit instead of the durable house-level docs/.

        ln -sfn "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        ln -sfn "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        ln -sfn "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        ln -sfn "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        ln -sfn "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        ln -sfn "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null || true
        ln -sfn "$SCRIPT_DIR/docs" "$SESSION_DIR/docs"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/frame_changed.txt
        : > pieces/display/editor_screen_changed.txt

        # Seed buffer + cursor (Phase T3, same shape as editor's own
        # button.sh - reused ops expect these exact file names).
        if [ ! -f pieces/system/editor_buffer.txt ]; then
            printf 'hi agy-txt\n' > pieces/system/editor_buffer.txt
        fi
        cat > pieces/system/editor_state.txt << 'EOF'
file_path=docs/untitled.txt
cursor_pos=-1
last_message=Welcome to agy-txt. Focus EDIT TEXT, Enter to INTERACT.
EOF

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-txt
active_target_id=editor
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="agy-txt"

        if [ -x "./ops/+x/agy_compose_view.+x" ]; then
            ./ops/+x/agy_compose_view.+x >/dev/null 2>&1 || true
        elif [ -x "./ops/+x/agy_compose_stub.+x" ]; then
            ./ops/+x/agy_compose_stub.+x >/dev/null 2>&1 || true
        fi

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        GL_PID=""
        RGB_PID=""
        if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
            # Same three-layer race fix as editor's own button.sh
            # (PITFALL 54) - wait for a real compose before opening GL.
            waited=0
            while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
                sleep 0.1
                waited=$((waited + 1))
            done
            if [ -z "$NO_GL" ] && [ -x ./system/gl_mirror ]; then
                ./system/gl_mirror >/dev/null 2>&1 &
                GL_PID=$!
            fi
            if [ -z "$NO_GL" ] && [ -x ./system/chtpm_rgb_render ]; then
                ./system/chtpm_rgb_render >/dev/null 2>&1 &
                RGB_PID=$!
            fi
        fi

        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        trap 'kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        echo "done"
        ;;
    help|h|-h|--help)
        echo "agy-txt button.sh (Phase T2 stub - see PLAN.md)"
        echo "  compile, c, build   - Build ops + copy system/GL/RGB from wsr-pal"
        echo "  run, r              - Interactive (headless if no DISPLAY)"
        echo "  kill, k, stop       - Kill lingering processes"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
