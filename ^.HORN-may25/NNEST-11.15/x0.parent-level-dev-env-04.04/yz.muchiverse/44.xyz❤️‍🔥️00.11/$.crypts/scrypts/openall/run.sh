#!/bin/bash
# openall/run.sh - always-open the monads we want, no questions.
# Launches the full desired desktop set unconditionally (ignores the
# autostart.pdl STATE|enabled toggle). Idempotent: each target is only
# launched if a process isn't already hosting it, so re-running never
# duplicates windows. Each launch is detached (setsid nohup), the same
# mechanism crypt_autostart uses.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CRYPTS_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOUSE_DIR="$(cd "$CRYPTS_DIR/.." && pwd)"

TPWIN="$HOUSE_DIR/&.widgits/tile-picker/ops/+x/tp_desktop_window.+x"
TASKBAR="$HOUSE_DIR/&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x"

# Ensure the shared asset drive is mounted (book-stack's bible assets
# live on it) - silent + idempotent, so openall is self-sufficient.
ensure_mount() {
    local uuid="b7ced73c-5231-4462-b98d-64e38fe2df9e"
    local mp="/media/no/$uuid"
    [ -d "$mp" ] && mountpoint -q "$mp" 2>/dev/null && return 0
    udisksctl mount -b "/dev/disk/by-uuid/$uuid" >/dev/null 2>&1
    sleep 1
    mountpoint -q "$mp" 2>/dev/null && echo "mounted $uuid" || echo "WARN: could not mount $uuid (book assets unavailable)"
}

entity_up() {
    pgrep -f "$1" >/dev/null 2>&1
}

launch_entity() {
    local ent="$1"
    if entity_up "$ent"; then
        echo "already open: $(basename "$ent")"
        return 0
    fi
    if [ ! -x "$TPWIN" ]; then
        echo "MISSING tp_desktop_window: $TPWIN"
        return 1
    fi
    setsid nohup "$TPWIN" "$ent" >/dev/null 2>&1 &
    echo "opened: $(basename "$ent")"
}

ensure_mount

launch_entity "$HOUSE_DIR/#.desktop/entities/ava"
launch_entity "$HOUSE_DIR/#.desktop/entities/asa"
launch_entity "$HOUSE_DIR/@.apps/MUCHI_RANCHER/entities/m1_ninjadragon"
launch_entity "$HOUSE_DIR/@.apps/MUCHI_RANCHER/entities/m8_redhorned"
launch_entity "$HOUSE_DIR/#.desktop/entities/book-stack"

if [ -x "$TASKBAR" ] && ! pgrep -f "$TASKBAR" >/dev/null 2>&1; then
    setsid nohup "$TASKBAR" "$HOUSE_DIR" >/dev/null 2>&1 &
    echo "opened: taskbar"
fi

echo "openall done"
