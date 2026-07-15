#!/bin/bash
# button.sh - launcher for egg-pals, same verb convention as mutaclsym's
# button.sh (c/compile, r/run, k/kill...).
#
# Status: the terminal menu loop is real and working (User/Faucet/Store/
# Pets, same three-process shape as mutaclsym: keyboard_input + prisc+x/
# pal + renderer, no ncurses). Hatched pets now have a per-pet detail
# screen (Feed/Clean/Sleep/Train/Export Card/Destroy Card/Open Window)
# and metabolism ticks on their own via pal/main_loop.pal's epoch dispatch
# to tick_pets.+x. Run `./button.sh icons` once before opening a pet's GL
# window if you want the poop/sleep status overlays to render.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Prefer <name>.exe over a same-named extension-less binary if both exist.
# This tree can ship pre-built system/<name> files from whichever platform
# packaged it (e.g. a Mac-built Mach-O). gcc only appends .exe to an -o
# name that has no dot in it, so scripts/build.sh's Windows output lands
# in <name>.exe *without ever touching* that leftover file - unlike
# Linux/Mac, where a rebuild overwrites system/<name> directly. Left
# unfixed, `./system/renderer` finds the stale non-Windows binary first
# (exact literal match) and fails with "Exec format error" instead of
# falling back to the freshly-built .exe.
bin_path() {
    if [ -x "$SCRIPT_DIR/$1.exe" ]; then
        echo "$SCRIPT_DIR/$1.exe"
    else
        echo "$SCRIPT_DIR/$1"
    fi
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    check|verify)
        for b in system/prisc+x system/emoji_gen_atlas system/emoji_xtract system/egg_window \
                 system/keyboard_input system/renderer \
                 ops/+x/generate_egg.+x ops/+x/claim_tokens.+x ops/+x/coin_flip.+x \
                 ops/+x/buy_egg.+x ops/+x/hatch_egg.+x ops/+x/menu_input.+x ops/+x/compose_menu.+x \
                 ops/+x/tick_pets.+x ops/+x/feed_pet.+x ops/+x/clean_pet.+x ops/+x/toggle_sleep.+x \
                 ops/+x/train_pet.+x ops/+x/export_card.+x ops/+x/destroy_card.+x; do
            # Windows builds land as <name>.exe for anything build.sh compiled
            # without a dot in its -o name already (gcc appends .exe itself
            # in that case; ops/+x/*.+x already has a dot, so it's untouched
            # there) - check both spellings rather than assume the platform.
            if [ -x "$SCRIPT_DIR/$b" ] || [ -x "$SCRIPT_DIR/$b.exe" ]; then
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

        "$(bin_path system/renderer)" &
        RENDERER_PID=$!
        "$(bin_path system/prisc+x)" pal/main_loop.pal >/dev/null 2>&1 &
        PRISC_PID=$!
        # Session marker so a floating egg_window (spawned via
        # menu_input.c's spawn_egg_window) can tell when this session
        # ends and close itself instead of being left behind frozen.
        echo "$PRISC_PID" > pieces/system/session.pid
        trap 'kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null; rm -f pieces/system/session.pid' EXIT INT TERM

        "$(bin_path system/keyboard_input)"

        kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null
        rm -f pieces/system/session.pid
        ;;
    icons)
        bash "$SCRIPT_DIR/scripts/gen_icons.sh"
        ;;
    kill|k|stop)
        echo "=== Killing egg-pals processes ==="
        # pkill isn't installed by default under MSYS2/Git-for-Windows -
        # fall back to matching `ps` output and killing by PID directly
        # when it's missing (still works fine on Linux/Mac if some
        # minimal image happens not to ship pkill either).
        kill_matching() {
            if command -v pkill >/dev/null 2>&1; then
                pkill -f "$1" 2>/dev/null
            else
                ps 2>/dev/null | grep -F "$1" | grep -v grep | awk '{print $1}' | while read -r pid; do
                    kill "$pid" 2>/dev/null
                done
            fi
        }
        kill_matching "$SCRIPT_DIR/system/keyboard_input"
        kill_matching "$SCRIPT_DIR/system/renderer"
        kill_matching "$SCRIPT_DIR/system/prisc+x"
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
        echo "  icons               - Pre-render the poop/sleep status-overlay icons (once)"
        echo "  kill, k, stop       - Kill any lingering egg-pals processes"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
