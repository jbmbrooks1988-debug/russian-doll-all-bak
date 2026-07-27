#!/bin/bash
# test-harn-same/button.sh - MINIMAL entry point for muchi-pal-agent's
# key-injection UX test harness. Unlike 044/041.pal-chain/041.pal-forum,
# this project is SINGLE-INSTANCE by design (button.sh's own "run"
# action reaps any existing session before starting a new one) - so
# this harness runs ONE session, not two concurrent ones. Real logic
# lives in ops/ (tk_inject_key, tk_type_text, tk_focus_item,
# tk_assert_contains - byte-identical copies, project-agnostic),
# scenarios/ holds the actual test sequence.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ACTION="${1:-help}"
OPS_X="$SCRIPT_DIR/ops/+x"

case "$ACTION" in
    compile|c|build)
        mkdir -p "$OPS_X"
        for op in tk_inject_key tk_type_text tk_focus_item tk_assert_contains; do
            gcc -Wall -Wextra -O2 -o "$OPS_X/$op.+x" "$SCRIPT_DIR/ops/$op.c" \
                && echo "OK   $op" || echo "FAIL $op"
        done
        ;;
    demo)
        if [ ! -x "$OPS_X/tk_inject_key.+x" ]; then
            echo "Ops not compiled yet - running compile first..."
            "$0" compile
        fi
        bash "$SCRIPT_DIR/scenarios/demo_list_dir_tool.sh"
        ;;
    kill|k|stop)
        bash "$PROJECT_DIR/pieces/os/kill_all.sh" >/dev/null 2>&1
        sleep 1
        STRAGGLERS=$(ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|manager/\+x/path_nav_manager|prisc\+x" | grep -v grep)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | awk '{print $2}' | xargs -r kill -9
            sleep 1
        fi
        if ps aux | grep -E "system/(orchestrator|chtpm_parser_pal|renderer|keyboard_input)|manager/\+x/path_nav_manager|prisc\+x" | grep -v grep >/dev/null 2>&1; then
            echo "WARNING: some processes are still running - check manually."
        else
            echo "clean"
        fi
        ;;
    help|h|-h|--help|*)
        echo "test-harn-same/button.sh - key-injected UX test harness for muchi-pal-agent"
        echo "Usage: ./test-harn-same/button.sh <compile|demo|kill|help>"
        ;;
esac
