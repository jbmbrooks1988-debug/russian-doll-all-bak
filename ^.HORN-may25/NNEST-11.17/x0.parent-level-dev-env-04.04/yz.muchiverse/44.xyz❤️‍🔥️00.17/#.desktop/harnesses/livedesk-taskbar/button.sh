#!/bin/bash
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"
case "$ACTION" in
    demo|run)
        bash "$SCRIPT_DIR/scenarios/demo_strip_and_popup_nav.sh"
        ;;
    help|*) echo "livedesk-taskbar nav harness: demo (builds + runs the autonomous nav scenario)" ;;
esac
