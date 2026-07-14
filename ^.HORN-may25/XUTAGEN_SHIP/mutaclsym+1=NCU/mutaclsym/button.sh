#!/bin/bash
# button.sh - launcher for mutaclsym, same verb convention as TPMOS's
# button.sh (c/compile, r/run, k/kill...). Scaled down on purpose: unlike
# full TPMOS, mutaclsym has no orchestrator/GL-OS/watchdog - the manager
# binary itself spawns and kills its one prisc+x child, so "run" just
# execs the manager and "kill" only needs to catch orphans (e.g. if the
# manager was itself killed -9 before it could reap prisc+x).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        exec ./manager/mutaclsym_manager
        ;;
    kill|k|stop)
        echo "=== Killing mutaclsym processes ==="
        pkill -f "$SCRIPT_DIR/manager/mutaclsym_manager" 2>/dev/null
        pkill -f "$SCRIPT_DIR/system/prisc\+x" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x manager/mutaclsym_manager ops/+x/move_player.+x ops/+x/end_turn.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        ;;
    help|h|-h|--help)
        echo "mutaclsym button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries (prisc+x, manager, ops)"
        echo "  run, r, start       - Run the manager (starts prisc+x itself)"
        echo "  kill, k, stop       - Kill any lingering manager/prisc+x processes"
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
