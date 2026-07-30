#!/bin/bash
# button.sh - text-editor-xyz app launcher
#
# Architecture (per pal-standards §36):
#   Editor and file-menu widget are SEPARATE PROGRAMS, each with their
#   own session directory, system binaries, ops, and PAL loops.
#   They communicate via file-mediated widget cmd bus (inbox/status).
#   App launcher starts both simultaneously and kills both on exit.
#
#   Editor: foreground session (owns TTY, has keyboard_input)
#   Widget: background session (widget profile, TTY→/dev/null)
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

EDITOR_DIR="$(ls -d "$HOUSE_DIR/102."*"editor"* 2>/dev/null | head -1)"
FM_DIR="$(ls -d "$HOUSE_DIR/&.widgits/file-menu"* 2>/dev/null | head -1)"

err() { echo "$*" >&2; exit 1; }
[ -n "$EDITOR_DIR" ] && [ -d "$EDITOR_DIR" ] || err "Editor not found in $HOUSE_DIR"
[ -n "$FM_DIR" ] && [ -d "$FM_DIR" ] || err "File-menu widget not found in $HOUSE_DIR"

SESSION_ID="$(date +%s)-$$"
EDITOR_SESSION="/tmp/.text-editor-xyz-editor-$SESSION_ID"

run_app() {
    # ── 1. Create EDITOR session directory ──
    mkdir -p "$EDITOR_SESSION/pieces/system" "$EDITOR_SESSION/pieces/display" \
             "$EDITOR_SESSION/pieces/apps/player_app" "$EDITOR_SESSION/pieces/keyboard" \
             "$EDITOR_SESSION/projects/agy-editor/manager" \
             "$EDITOR_SESSION/docs"

    ln -sfn "$EDITOR_DIR/system" "$EDITOR_SESSION/system"
    ln -sfn "$EDITOR_DIR/ops" "$EDITOR_SESSION/ops"
    ln -sfn "$EDITOR_DIR/pal" "$EDITOR_SESSION/pal"
    ln -sfn "$EDITOR_DIR/default_op.txt" "$EDITOR_SESSION/default_op.txt"
    ln -sfn "$EDITOR_DIR/pieces/chtpm" "$EDITOR_SESSION/pieces/chtpm"
    ln -sfn "$EDITOR_DIR/projects/agy-editor/pieces" "$EDITOR_SESSION/projects/agy-editor/pieces"
    ln -sfn "$EDITOR_DIR/docs" "$EDITOR_SESSION/docs"

    cd "$EDITOR_SESSION"

    # ── 2. Init editor state ──
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/editor_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > projects/agy-editor/manager/gui_state.txt

    if [ ! -f pieces/system/editor_buffer.txt ]; then
        printf 'hi   \360\237\230\205 \360\237\230\206 \360\237\220\247\n' > pieces/system/editor_buffer.txt
    fi
    cat > pieces/system/editor_state.txt << 'EOF'
file_path=docs/untitled.txt
cursor_pos=-1
last_message=text-editor-xyz ready. F4 for FILE MENU.
EOF
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-editor
active_target_id=editor
EOSTATE

    export PRISC_PROJECT_ROOT="$EDITOR_SESSION"
    export PRISC_PROJECT_ID="agy-editor"

    # ── 3. Set up widget cmd bus (editor's inbox for widget commands) ──
    mkdir -p pieces/system/widget_cmds
    : > pieces/system/widget_cmds/inbox.txt
    : > pieces/system/widget_cmds/status.txt

    # ── 4. Compose initial editor frame ──
    if [ -x ./ops/+x/editor_compose_frame.+x ]; then
        PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
            ./ops/+x/editor_compose_frame.+x >/dev/null 2>&1 || true
    fi

    # ── 5. Write house root for widget/ledger discovery ──
    echo "$HOUSE_DIR" > pieces/system/house_root.txt

    # ── 6. Register in xyzfs runtime ledger ──
    LEDGER_APPEND="$FM_DIR/ops/+x/ledger_append.+x"
    if [ -x "$LEDGER_APPEND" ]; then
        PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
            "$LEDGER_APPEND" ONLINE editor agy-editor "$EDITOR_SESSION" $$ \
            "text-editor-xyz" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true
    fi

    # ── 7. Start editor daemons (background) ──
    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    # ── 7. Start widget cmd drainer (background) ──
    WIDGET_CMDS_PID=""
    if [ -x "$EDITOR_DIR/ops/+x/editor_widget_cmds.+x" ]; then
        (
            cd "$EDITOR_SESSION"
            while [ -f pieces/system/widget_cmds/inbox.txt ]; do
                PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
                    "$EDITOR_DIR/ops/+x/editor_widget_cmds.+x" 8 >/dev/null 2>&1 || true
                sleep 0.2
            done
        ) &
        WIDGET_CMDS_PID=$!
    fi

    # ── 8. Start file-menu widget (background, widget profile, TTY→/dev/null)
    #      gl_mirror opens the GL window; keyboard forwarded to interact_relay.txt ──
    cd "$FM_DIR"
    RUN_PROFILE=widget \
        bash "$FM_DIR/button.sh" r "$EDITOR_SESSION" >/dev/null 2>&1 &
    WIDGET_PID=$!

    cd "$EDITOR_SESSION"

    # ── 9. Cleanup trap ──
    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            if [ "$cwd" = "$EDITOR_SESSION" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    }

    cleanup() {
        if [ -x "$LEDGER_APPEND" ]; then
            PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
                "$LEDGER_APPEND" OFFLINE editor agy-editor "$EDITOR_SESSION" $$ \
                "text-editor-xyz" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true
        fi
        kill "$WIDGET_CMDS_PID" "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null || true
        kill "$WIDGET_PID" 2>/dev/null || true
        kill_own_module
        mkdir -p "$SCRIPT_DIR/sessions"
        if [ -f pieces/system/editor_buffer.txt ]; then
            cp pieces/system/editor_buffer.txt "$SCRIPT_DIR/sessions/autosave.txt" 2>/dev/null || true
        fi
        rm -rf "$EDITOR_SESSION" 2>/dev/null || true
    }
    trap cleanup EXIT INT TERM

    # ── 10. Keyboard input (foreground, blocking) ──
    : > pieces/apps/player_app/history.txt
    ./system/keyboard_input

    cleanup
}

case "$ACTION" in
    compile|c|build)
        echo "text-editor-xyz: checking dependencies..."
        echo "  editor: $EDITOR_DIR"
        echo "  file-menu: $FM_DIR"
        echo "--- Editor ---"
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal; do
            [ -x "$EDITOR_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        for b in ops/+x/editor_menu_input.+x ops/+x/editor_compose_frame.+x \
                 ops/+x/editor_widget_cmds.+x; do
            [ -x "$EDITOR_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        echo "--- File-menu widget ---"
        for b in system/prisc+x system/chtpm_parser_pal; do
            [ -x "$FM_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        for b in ops/+x/fm_set_focus.+x ops/+x/fm_enqueue_cmd.+x \
                 ops/+x/fm_compose_frame.+x ops/+x/fm_menu_input.+x; do
            [ -x "$FM_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        echo "--- Layouts ---"
        [ -f "$FM_DIR/pieces/chtpm/layouts/file-menu.chtpm" ] && echo "OK   file-menu.chtpm" || echo "MISS file-menu.chtpm"
        [ -f "$FM_DIR/pal/main_loop_chtpm.pal" ] && echo "OK   main_loop_chtpm.pal" || echo "MISS main_loop_chtpm.pal"
        [ -f "$EDITOR_DIR/pieces/chtpm/layouts/editor.chtpm" ] && echo "OK   editor.chtpm" || echo "MISS editor.chtpm"
        [ -f "$EDITOR_DIR/pal/main_loop_chtpm.pal" ] && echo "OK   editor main_loop_chtpm.pal" || echo "MISS editor main_loop_chtpm.pal"
        ;;
    run|r|start)
        run_app
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        rm -rf /tmp/.text-editor-xyz-* 2>/dev/null
        echo "done"
        ;;
    help|h|-h|--help)
        echo "text-editor-xyz — Text editor with file-menu widget GL window"
        echo ""
        echo "Architecture (pal-standards §36):"
        echo "  Editor and file-menu widget are SEPARATE PROGRAMS."
        echo "  Editor: foreground (owns TTY, INTERACT canvas)."
        echo "  Widget: background (GL window, no TTY)."
        echo "  Communication: file-mediated widget cmd bus (inbox.txt)."
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile      - Check dependencies"
        echo "  run          - Launch editor + file-menu widget"
        echo "  kill         - Kill processes"
        echo "  help         - This help"
        ;;
    *)
        echo "Unknown: $ACTION (try ./button.sh help)"
        exit 1
        ;;
esac
