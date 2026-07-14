#!/bin/bash
# button.sh - launcher for egg-pals, same verb convention as mutaclsym's
# button.sh (c/compile, r/run, k/kill...).
#
# Status: the terminal menu loop is real and working (User/Faucet/Store/
# Pets, same three-process shape as mutaclsym: keyboard_input + prisc+x/
# pal + renderer, no ncurses). The egg-shaped GL window (egg_window.c)
# still draws a plain circle and is not wired into the menu yet - opening
# a specific pet from the Pets screen is the next step, see
# dox/01-architecture.md.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    check|verify)
        for b in system/prisc+x system/emoji_gen_atlas system/emoji_xtract system/egg_window \
                 system/keyboard_input system/renderer \
                 ops/+x/generate_egg.+x ops/+x/claim_tokens.+x ops/+x/coin_flip.+x \
                 ops/+x/buy_egg.+x ops/+x/menu_input.+x ops/+x/compose_menu.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        ;;
    demo|d)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        echo "=== Minting an egg for user_01 (bypasses the store's token cost) ==="
        EGG_ID=$(./ops/+x/generate_egg.+x user_01)
        echo "Minted: $EGG_ID"
        echo "--- state.txt ---"
        cat "pieces/world_01/map_lobby/$EGG_ID/state.txt"
        echo "--- user_01 inventory.txt ---"
        cat "pieces/world_01/map_lobby/user_01/inventory.txt"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        mkdir -p pieces/system pieces/display pieces/apps/player_app
        : > pieces/system/quit_flag.txt
        : > pieces/apps/player_app/history.txt

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="egg-pals"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/prisc+x pal/main_loop.pal >/dev/null 2>&1 &
        PRISC_PID=$!
        trap 'kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null' EXIT INT TERM

        ./system/keyboard_input

        kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null
        ;;
    kill|k|stop)
        echo "=== Killing egg-pals processes ==="
        pkill -f "$SCRIPT_DIR/system/keyboard_input" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/renderer" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/prisc\+x" 2>/dev/null
        echo "done"
        ;;
    help|h|-h|--help)
        echo "egg-pals button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries"
        echo "  check, verify       - Verify all binaries exist"
        echo "  demo, d             - Mint one test egg end to end and print its state"
        echo "  run, r, start       - Run the menu (User/Faucet/Store/Pets)"
        echo "  kill, k, stop       - Kill any lingering egg-pals processes"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
