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
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"

        # SINGLE-INSTANCE GUARD (2026-07-21, jul-21-gemma-fix.txt) - a
        # closed terminal sends SIGHUP, which the EXIT/INT/TERM trap
        # below never caught, so old sessions could keep running forever
        # in the background. Confirmed live: three separate `run`s
        # stacked up over an hour, all three polling/mutating what was
        # then a shared world_01/state.txt at once. world_01 is private
        # per session now (see below), which removes the DATA race, but
        # stacked zombie sessions are still wasted/confusing processes.
        #
        # WIDENED 2026-07-21, SAME DAY (jul-21-gemma-fix.txt has the full
        # trace) - the FIRST version of this guard only matched by cwd
        # falling under pieces/sessions/, on the assumption every
        # instance of these binaries was launched via `run`'s own
        # SESSION_DIR. Live-caught wrong: two processes (a prisc+x +
        # chtpm_parser_pal pair) had been running for over an HOUR with
        # cwd == the project root itself - launched directly from there,
        # bypassing button.sh entirely (almost certainly an earlier
        # agent's own direct-invocation testing per handoff.txt's own
        # "PRISC_PROJECT_ROOT='.' ops/+x/..." discipline, run against the
        # full chtpm_parser_pal/prisc+x pair instead of just a single op
        # binary). The cwd-only guard silently ignored them - they don't
        # live under pieces/sessions/ at all - so they kept polling and
        # mutating the CANONICAL pieces/world_01 directly, forever,
        # racing every subsequent properly-isolated session regardless of
        # this fix. Match by binary identity now (any live instance of
        # these binaries whose own exe resolves under THIS project root),
        # not by where it happens to be running from - that's the only
        # way to also catch a rogue direct launch, not just a stacked
        # `run`. ALWAYS re-verify with `ps` after calling this - a killed
        # PID that turns out to still be alive next time you check is a
        # sign something (sandboxing, permissions, a supervisor) silently
        # blocked the signal; don't trust a clean exit code alone.
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

        # Clean up stale P2P presence files (from sessions that didn't exit cleanly)
        cleanup_stale_presence() {
            local presence_dir=$(find "$SCRIPT_DIR" -path "*net/presence" -type d 2>/dev/null | head -1)
            if [ -d "$presence_dir" ]; then
                for pf in "$presence_dir"/*_node-*.txt 2>/dev/null; do
                    [ -f "$pf" ] || continue
                    local pid=$(grep "^pid=" "$pf" 2>/dev/null | cut -d= -f2)
                    if [ -n "$pid" ] && ! ps -p "$pid" > /dev/null 2>&1; then
                        rm -f "$pf"
                    fi
                done
            fi
        }
        cleanup_stale_presence

        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/projects/muchi-pal-agent/manager"
        # Clean debug directory to ensure no old frames carry over
        rm -rf "$SESSION_DIR/debug"
        mkdir -p "$SESSION_DIR/debug"
        mkdir -p "$SCRIPT_DIR/pieces/world_01"

        # world_01 is COPIED (never symlinked/shared) into the session -
        # see the corrected header comment above. Seeded from the
        # canonical copy for conversation continuity across runs; this
        # session gets its own private, exclusive copy to mutate, copied
        # back out on exit (see the trap below).
        mkdir -p "$SESSION_DIR/pieces/world_01"
        if [ -d "$SCRIPT_DIR/pieces/world_01" ] && [ "$(ls -A "$SCRIPT_DIR/pieces/world_01" 2>/dev/null)" ]; then
            cp -r "$SCRIPT_DIR/pieces/world_01/." "$SESSION_DIR/pieces/world_01/"
        fi
        mkdir -p "$SESSION_DIR/pieces/world_01/session_01/chat"

        # Blanket-symlink every top-level project entry into the session,
        # instead of a hand-maintained list (system/ops/pal/manager/...).
        # The hand-maintained version is exactly how manager/ (the compiled
        # binary directory for path_nav_manager.+x, chat.chtpm's second
        # <module>) went missing from this list for an entire feature: the
        # failure is completely silent (launch_extra_module()'s own
        # execv() just _exit(1)s in the child, nothing written anywhere -
        # see pal-standards.txt sec. 29.7), so a forgotten directory here
        # is indistinguishable from any other bug until someone reads this
        # script. `pieces/` and `projects/` are excluded - they're where
        # session-scoped, per-run WRITABLE state lives (built fresh below,
        # never shared across sessions); everything else at the project
        # root is read-only shared material (binaries, ops, layouts,
        # config) safe to link as-is.
        for entry in "$SCRIPT_DIR"/*; do
            name="$(basename "$entry")"
            case "$name" in
                pieces|projects) continue ;;
            esac
            ln -s "$entry" "$SESSION_DIR/$name"
        done

        # pieces/ itself is a MIX: chtpm/registry (shared, read-only)
        # alongside apps/display/system/keyboard/sessions/world_01
        # (session-scoped, built fresh via the mkdir -p/cp -r above -
        # excluded here so symlinking doesn't collide with those real
        # directories/files, and `sessions` itself is excluded so a
        # session never symlinks its own parent directory into itself).
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

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/chat.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # chtpm_parser_pal has no SIGTERM handler of its own; its own
        # spawned module (system/prisc+x) must be reaped by cwd match,
        # not a bare `pkill -f` (argv text is identical across every
        # session) - see pal-forum/button.sh's own identical comment.
        # path_nav_manager.+x is the SAME situation now that chat.chtpm has
        # a second <module> line (launch_extra_module() in
        # chtpm_parser_pal.c, forked independently of system/prisc+x) - it
        # needs the same cwd-matched reap or it leaks as an orphaned
        # background process every single `button.sh run`.
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
            for pid in $(pgrep -f "path_nav_manager\.\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        # Copies this session's own private world_01 back over the
        # canonical shared copy so conversation history/state persists
        # for the NEXT `run` - exclusivity while live, continuity across
        # runs (see the corrected header comment above). Last session to
        # exit wins, which is the expected, reasonable semantic for a
        # single local user running one terminal at a time (the
        # reap_stale_sessions() guard above means that's now the only
        # case that reaches here at all).
        persist_world_01() {
            mkdir -p "$SCRIPT_DIR/pieces/world_01"
            cp -r "$SESSION_DIR/pieces/world_01/." "$SCRIPT_DIR/pieces/world_01/" 2>/dev/null
        }

        # HUP added (2026-07-21, jul-21-gemma-fix.txt) - a closed
        # terminal sends SIGHUP, not INT/TERM, and this trap never caught
        # it, which is the actual, confirmed reason old sessions kept
        # running in the background for hours (see reap_stale_sessions()
        # above for the belt-and-suspenders fix on the NEXT run too).
        trap 'persist_world_01; kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM HUP

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
        pkill -f "path_nav_manager\.\+x" 2>/dev/null
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
