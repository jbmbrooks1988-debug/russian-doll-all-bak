#!/bin/bash
# button.sh - launcher for pal-chat-irc, modeled directly on pal-forum's
# own button.sh (real interact+module chtpm pattern). BUILT SESSION-
# ISOLATED AND USER-PAL-INTEGRATED FROM DAY ONE (PAL-CHAT-IRC-
# STANDARD.txt sec. 2/1) - the first project in this family with both
# from its very first version, not retrofitted.
#
# "run" launches, alongside the normal chtpm/renderer/keyboard trio, one
# persistent palnet_peer.+x instance (own_kind=irc_node, seek_kind=
# irc_node - full mesh) and chat_inbox_watcher.+x (started automatically,
# matching pal-forum's own "start mining/watching launches the watcher
# too" precedent - a chat app is even less usable than a wallet if
# incoming messages just don't show up by default).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # SESSION ISOLATION (pal-standards.txt sec. 23) - every "run"
        # gets a private, throwaway directory for its own ephemeral UI
        # state, deleted on exit. SHARED (symlinked, never copied):
        # system/ops/pal/pieces/chtpm/default_op.txt/projects pieces
        # (static, read-only) and users/, rooms/ (the REAL persistent
        # database every session of this project must see and mutate
        # together).
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/net" "$SESSION_DIR/projects/pal-chat-irc/manager" \
                 "$SESSION_DIR/debug"
        mkdir -p "$SCRIPT_DIR/users" "$SCRIPT_DIR/rooms"
        ln -s "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        ln -s "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        ln -s "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        ln -s "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        ln -s "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        ln -s "$SCRIPT_DIR/projects/pal-chat-irc/pieces" "$SESSION_DIR/projects/pal-chat-irc/pieces"
        ln -s "$SCRIPT_DIR/users" "$SESSION_DIR/users"
        ln -s "$SCRIPT_DIR/rooms" "$SESSION_DIR/rooms"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/chat_screen_changed.txt
        : > net/outbox.txt
        : > net/inbox.txt
        : > projects/pal-chat-irc/manager/gui_state.txt
        : > pieces/apps/player_app/cli_buffers.txt
        : > debug/frame_history.txt

        cat > pieces/system/chat_menu_state.txt << 'EOF'
last_message=Welcome to pal-chat-irc.
EOF
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=pal-chat-irc
active_target_id=login
EOSTATE

        # USER-PAL AUTO-FILL REMOVED (direct user correction, 2026-07-20:
        # "i dont want it to default to saying 'jb'... default still has
        # 'jb' in it for some reason" - reported as unwanted even after
        # the field-label fix (shared-ops/chtpm_parser_pal.c), so this is
        # a real "don't want this feature" call, not just a display
        # nit). See USER-PAL-STANDARD.txt sec. 4 for the design, kept
        # documented but marked reverted - login screens start blank
        # again, matching every project's own pre-user-pal behavior.

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="pal-chat-irc"
        # See pal-forum/button.sh's own identical fix/comment (real bug,
        # live-caught 2026-07-20): palnet_peer.c's own
        # resolve_presence_root() defaults to "one level above
        # PRISC_PROJECT_ROOT", which is wrong once that points at a
        # session directory - must be pinned to the real, shared
        # presence dir explicitly or P2P discovery silently breaks.
        export PRISC_NET_ROOT="$SCRIPT_DIR/../net/presence"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/login.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        RGB_PID=""
        if [ -x "system/chtpm_rgb_render" ]; then
            ./system/chtpm_rgb_render >/tmp/pal_chat_irc_chtpm_rgb_render.log 2>&1 &
            RGB_PID=$!
        fi

        NET_PID=""
        if [ -z "$NO_NET" ] && [ -x "ops/+x/palnet_peer.+x" ]; then
            ./ops/+x/palnet_peer.+x irc_node pal-chat-irc - net/outbox.txt net/inbox.txt irc_node >/tmp/pal_chat_irc_palnet_peer.log 2>&1 &
            NET_PID=$!
        fi

        WATCHER_PID=""
        if [ -z "$NO_NET" ] && [ -x "ops/+x/chat_inbox_watcher.+x" ]; then
            ./ops/+x/chat_inbox_watcher.+x >/tmp/pal_chat_irc_inbox_watcher.log 2>&1 &
            WATCHER_PID=$!
        fi

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

        trap 'kill "$RENDERER_PID" "$CHTPM_PID" $RGB_PID $NET_PID $WATCHER_PID 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" $RGB_PID $NET_PID $WATCHER_PID 2>/dev/null
        kill_own_module
        ;;
    watcher|w)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        echo "Starting chat_inbox_watcher (Ctrl+C to stop)..."
        ./ops/+x/chat_inbox_watcher.+x
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "ops/+x/palnet_peer" 2>/dev/null
        pkill -f "ops/+x/chat_inbox_watcher" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/chat_create_user.+x \
                 ops/+x/chat_switch_user.+x ops/+x/chat_post_message.+x \
                 ops/+x/chat_inbox_watcher.+x ops/+x/chat_menu_input.+x \
                 ops/+x/chat_compose_frame.+x ops/+x/palnet_peer.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "pal-chat-irc button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  watcher, w          - Run the inbox watcher (receives messages from peers)"
        echo "  kill, k, stop       - Kill any lingering pal-chat-irc processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
