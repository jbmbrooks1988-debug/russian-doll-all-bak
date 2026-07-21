#!/bin/bash
# button.sh - launcher for muchi-pal-agent (renamed 2026-07-20 from
# muchi-pal-chat/muchi-pal-llm - it does much more than "LLM chat":
# tool execution, and planned rl/fsm/script responders alongside the
# LLM providers - see ROADMAP-models.txt).
# REAL CHTPM UPGRADE (2026-07-20, CHAT-INTEGRATION-ARCHITECTURE.txt
# sec. 1): this project used to launch system/prisc+x directly, no
# chtpm layer at all, with keyboard_input's own raw keycodes hand-
# buffered by ops/buffer_key.c (now deleted). "run" now launches
# system/chtpm_parser_pal pieces/chtpm/layouts/chat.chtpm instead -
# THAT process's own <module> tag launches system/prisc+x pal/
# main_loop_chtpm.pal as its own persistent child, same shape as every
# other project in this family. Real <cli_io> (chat.chtpm) now owns
# typing/backspace/live-echo natively.
#
# SESSION ISOLATION FROM THIS SAME PASS (pal-standards.txt sec. 23) -
# copied from pal-forum's own proven "run" action shape, not re-
# designed. SHARED (symlinked, never copied): system/ops/pal/pieces-
# chtpm/default_op.txt/pieces-registry (static, read-only) and
# pieces/world_01/ (the REAL persistent chat session data - state.txt,
# context_log.txt - this project's own equivalent of pal-forum's own
# users/ or pal-chain's own wallets/).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/projects/muchi-pal-agent/manager" "$SESSION_DIR/debug"
        mkdir -p "$SCRIPT_DIR/pieces/world_01"
        ln -s "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        ln -s "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        ln -s "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        ln -s "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        ln -s "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        ln -s "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry"
        ln -s "$SCRIPT_DIR/pieces/world_01" "$SESSION_DIR/pieces/world_01"
        ln -s "$SCRIPT_DIR/sandbox" "$SESSION_DIR/sandbox"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/chat_screen_changed.txt
        : > projects/muchi-pal-agent/manager/gui_state.txt
        : > pieces/apps/player_app/cli_buffers.txt
        : > debug/frame_history.txt

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=muchi-pal-agent
active_target_id=chat
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="muchi-pal-agent"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/chat.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # chtpm_parser_pal has no SIGTERM handler of its own; its own
        # spawned module (system/prisc+x) must be reaped by cwd match,
        # not a bare `pkill -f` (argv text is identical across every
        # session) - see pal-forum/button.sh's own identical comment.
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        trap 'kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/send_message.+x ops/+x/check_response.+x \
                 ops/+x/execute_tool.+x ops/+x/deny_tool.+x ops/+x/switch_model.+x \
                 ops/+x/compose_frame.+x ops/+x/json_parser.+x ops/+x/connect_op.+x \
                 ops/+x/text_to_pal_prompt.+x \
                 ops/+x/file_ops.+x ops/+x/cmd_exec.+x ops/+x/edit_file.+x \
                 ops/+x/list_dir.+x ops/+x/search_in_files.+x ops/+x/web_search.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        ;;
    help|h|-h|--help)
        echo "muchi-pal-agent button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries (prisc+x, keyboard_input, renderer, chtpm_parser_pal, ops)"
        echo "  run, r, start       - Run the chat (real chtpm UI, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering muchi-pal-agent processes"
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
