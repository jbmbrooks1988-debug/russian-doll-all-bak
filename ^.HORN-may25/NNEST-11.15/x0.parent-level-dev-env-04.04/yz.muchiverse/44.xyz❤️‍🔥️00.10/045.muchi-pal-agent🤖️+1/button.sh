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
# SESSION ISOLATION FROM THIS SAME PASS (xyzos-standards.txt sec. 23) -
# copied from pal-forum's own proven "run" action shape, not re-
# designed. SHARED (symlinked, never copied): system/ops/pal/pieces-
# chtpm/default_op.txt/pieces-registry (static, read-only).
#
# CORRECTED 2026-07-21 (jul-21-gemma-fix.txt has the full live-caught
# trace - direct user correction after a real double-logged-response
# bug): pieces/world_01/ was ORIGINALLY classified as shared/symlinked
# here, on the mistaken assumption it was this project's equivalent of
# pal-chat-irc's own users/rooms/ (real multi-user data every session
# legitimately mutates together). It isn't - world_01/session_01/chat/
# state.txt holds ai_state/curl_pid/pending_tool_name, single-owner "am
# I mid-flight on ONE specific API call right now" bookkeeping, the
# direct structural equivalent of pal-chat-irc's own PRIVATE, per-
# session pieces/apps/player_app/state.txt. Sharing it meant two
# concurrent `run`s (old sessions left running because a closed
# terminal sends SIGHUP, which the EXIT/INT/TERM trap below never
# caught) both polled and mutated the same file, producing the exact
# same assistant reply logged twice for one turn. world_01 is now
# COPIED into $SESSION_DIR per run (private, exclusive - see below),
# seeded from the last run's own copy for conversation continuity, and
# copied back out on exit so history still persists across runs -
# continuity and exclusivity are separate concerns, this gets both.
PAL_MODE=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Parse --pal flag from any position, find the action verb
ACTION="help"
for arg in "$@"; do
    case "$arg" in
        --pal) PAL_MODE=1 ;;
        *) ACTION="$arg" ;;
    esac
done

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        gcc -o system/orchestrator system/orchestrator.c 2>/dev/null && echo "OK   system/orchestrator" || echo "SKIP system/orchestrator"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        if [ ! -x "system/orchestrator" ]; then
            echo "Compiling orchestrator..."
            gcc -o system/orchestrator system/orchestrator.c 2>/dev/null
        fi

        # SINGLE-INSTANCE GUARD
        reap_stale_sessions() {
            local pid exe
            for pid in $(pgrep -f "system/prisc\+x|system/chtpm_parser_pal|system/renderer|system/keyboard_input|path_nav_manager\.\+x" 2>/dev/null); do
                exe="$(readlink -f "/proc/$pid/exe" 2>/dev/null)"
                case "$exe" in
                    "$SCRIPT_DIR"/*) kill -9 "$pid" 2>/dev/null ;;
                esac
            done
            rm -rf "$SCRIPT_DIR/pieces/sessions"/*
        }
        reap_stale_sessions

        # SESSION ISOLATION
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/projects/muchi-pal-agent/manager"
        rm -rf "$SESSION_DIR/debug"
        mkdir -p "$SESSION_DIR/debug"
        mkdir -p "$SCRIPT_DIR/pieces/world_01"

        # world_01 is COPIED (never symlinked) into the session
        mkdir -p "$SESSION_DIR/pieces/world_01"
        if [ -d "$SCRIPT_DIR/pieces/world_01" ] && [ "$(ls -A "$SCRIPT_DIR/pieces/world_01" 2>/dev/null)" ]; then
            cp -r "$SCRIPT_DIR/pieces/world_01/." "$SESSION_DIR/pieces/world_01/"
        fi
        mkdir -p "$SESSION_DIR/pieces/world_01/session_01/chat"

        # Blanket-symlink every top-level project entry into the session
        for entry in "$SCRIPT_DIR"/*; do
            name="$(basename "$entry")"
            case "$name" in
                pieces|projects) continue ;;
            esac
            ln -s "$entry" "$SESSION_DIR/$name"
        done

        for entry in "$SCRIPT_DIR/pieces"/*; do
            name="$(basename "$entry")"
            case "$name" in
                apps|display|system|keyboard|sessions|world_01) continue ;;
            esac
            ln -s "$entry" "$SESSION_DIR/pieces/$name"
        done

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/chat_screen_changed.txt
        : > pieces/apps/player_app/view_changed.txt
        : > pieces/apps/player_app/view.txt
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

        if [ "$PAL_MODE" -eq 1 ]; then
            export PAL_LAYOUT="pieces/chtpm/layouts/chat.chtpm"
        else
            export PAL_LAYOUT=""
        fi

        exec "$SCRIPT_DIR/system/orchestrator"
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "path_nav_manager\.\+x" 2>/dev/null
        bash "$SCRIPT_DIR/pieces/os/kill_all.sh"
        # See #.haiku+/!.xyzos-pitfalls+1.txt PITFALL 20/21/23 - verify
        # independently rather than trusting kill_all.sh's own message
        # (an unescaped + in its own pattern was silently killing
        # nothing for path_nav_manager until fixed this session).
        sleep 0.5
        STRAGGLERS=$(pgrep -f "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|manager/\+x/path_nav_manager" 2>/dev/null)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | xargs -r kill -9 2>/dev/null
            sleep 0.5
        fi
        if pgrep -f "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|manager/\+x/path_nav_manager" >/dev/null 2>&1; then
            echo "WARNING: some muchi-pal-agent processes are still running - check manually."
        fi
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/send_message.+x ops/+x/check_response.+x \
                 ops/+x/execute_tool.+x ops/+x/deny_tool.+x ops/+x/switch_model.+x \
                 ops/+x/compose_frame.+x ops/+x/json_parser.+x ops/+x/connect_op.+x \
                 ops/+x/text_to_pal_prompt.+x ops/+x/tts_speak.+x \
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
        echo "Usage: ./button.sh <action> [--pal]"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries (prisc+x, keyboard_input, renderer, orchestrator, ops)"
        echo "  run, r, start       - Run the chat (orchestrator manages all processes)"
        echo "  --pal               - Use PAL script mode (passed to run)"
        echo "  kill, k, stop       - Kill any lingering muchi-pal-agent processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        echo ""
        echo "Recommended workflow:"
        echo "  1. ./button.sh compile"
        echo "  2. ./button.sh check"
        echo "  3. ./button.sh run          # C mode"
        echo "  3. ./button.sh run --pal    # PAL mode"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
