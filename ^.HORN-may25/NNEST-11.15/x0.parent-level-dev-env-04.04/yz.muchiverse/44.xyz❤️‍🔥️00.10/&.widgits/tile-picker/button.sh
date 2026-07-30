#!/bin/bash
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
case "$ACTION" in
    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        for op in tp_set_brush tp_place tp_place_desktop; do
            gcc -Wall -Wextra -O2 -o "$SCRIPT_DIR/ops/+x/$op.+x" "$SCRIPT_DIR/ops/$op.c" && echo OK $op
        done
        ;;
    help|*)
        echo "tile-picker: compile | help"
        echo "  tp_place          — PLACE_TILE on focused mutaclysm"
        echo "  tp_place_desktop  — stamp brush onto #.desktop/tiles/"
        ;;
esac
