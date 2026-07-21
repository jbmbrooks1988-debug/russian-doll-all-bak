#!/bin/bash
# button.sh - launcher for user-pal, modeled directly on pal-forum's
# own button.sh (real interact+module chtpm pattern). Session-isolated
# from day one (USER-PAL-STANDARD.txt sec. 3) - built AFTER session
# isolation was already proven in pal-forum, so this is the correct
# starting shape, not a retrofit.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # SESSION ISOLATION (pal-standards.txt sec. 23, USER-PAL-STANDARD.txt
        # sec. 3) - every "run" gets a private, throwaway directory for
        # its own ephemeral UI state (keyboard history, interact_relay,
        # gui_state), deleted on exit.
        #
        # SHARED, PERSISTENT, NEVER SESSION-SCOPED (symlinked in, never
        # copied, never deleted on exit): users/ (the real identity
        # registry) and current_login.txt (USER-PAL-STANDARD.txt sec. 2
        # - deliberately NOT like pal-forum's own net/session.txt; this
        # file must outlive any one session, or "save logged in user
        # data in one place" would mean nothing the moment a terminal
        # closes).
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/projects/user-pal/manager"
        mkdir -p "$SCRIPT_DIR/users"  # shared, real - not session-scoped
        touch "$SCRIPT_DIR/current_login.txt"  # ensure the symlink target exists
        ln -s "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        ln -s "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        ln -s "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        ln -s "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        ln -s "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        ln -s "$SCRIPT_DIR/projects/user-pal/pieces" "$SESSION_DIR/projects/user-pal/pieces"
        ln -s "$SCRIPT_DIR/users" "$SESSION_DIR/users"
        ln -s "$SCRIPT_DIR/current_login.txt" "$SESSION_DIR/current_login.txt"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/userpal_screen_changed.txt
        : > projects/user-pal/manager/gui_state.txt

        cat > pieces/system/userpal_menu_state.txt << 'EOF'
last_message=Welcome to user-pal.
EOF
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=user-pal
active_target_id=login
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="user-pal"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/login.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # See pal-forum/button.sh's own identical comment: chtpm_parser_pal
        # has no SIGTERM handler of its own, so its own spawned module
        # (system/prisc+x) must be reaped by cwd match, not a bare
        # `pkill -f` (argv text is identical across every session).
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        # Deletes ONLY this session's own private directory - never the
        # real, shared system/ops/pal/users/current_login.txt (symlinks;
        # `rm -rf` on the session dir removes the symlink itself, not
        # its target).
        trap 'kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        # NOTE: same blunt shape as every other project's own "kill"
        # debug action (pal-forum/pal-chain button.sh) - matches by
        # binary name only, so it is NOT session-scoped and NOT
        # project-scoped (a plain `pkill -f "system/prisc\+x"` matches
        # every project's own module, not just user-pal's). This is
        # fine for a manual debug helper a human runs deliberately; the
        # real "run" path's own exit trap uses kill_own_module() (cwd-
        # scoped) instead, never this. Do not use "kill" while another
        # project's own session might be running.
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/userpal_create_account.+x \
                 ops/+x/userpal_login.+x ops/+x/userpal_logout.+x \
                 ops/+x/userpal_whoami.+x ops/+x/userpal_menu_input.+x \
                 ops/+x/userpal_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "user-pal button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering user-pal processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
