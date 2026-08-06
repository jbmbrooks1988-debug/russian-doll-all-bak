#!/bin/bash
# open_event_ez.sh — context-menu launch of event-ez for one monster package.
# Usage (from tp_desktop_window METHOD dispatch):
#   open_event_ez.sh <package_dir>
# Sets EZ_PKG_NAME / EZ_PKG_DIR and starts event-ez widget (GL).
set -e
PKG_DIR="${1:-}"
if [ -z "$PKG_DIR" ] || [ ! -d "$PKG_DIR" ]; then
    echo "open_event_ez: need entity package dir as argv[1]" >&2
    exit 1
fi
PKG_DIR="$(cd "$PKG_DIR" && pwd)"
NAME="$(basename "$PKG_DIR")"
# entity package lives at .../MUCHI_RANCHER/entities/<name>
MUCHI_RANCHER="$(cd "$PKG_DIR/../.." && pwd)"
HOUSE_ROOT="$(cd "$MUCHI_RANCHER/../.." && pwd)"
EZ="$HOUSE_ROOT/&.widgits/event-ez"
EVENT_PKG="$PKG_DIR/event_pkg"
mkdir -p "$EVENT_PKG/pages/page_1"

if [ ! -x "$EZ/button.sh" ] && [ ! -f "$EZ/button.sh" ]; then
    echo "open_event_ez: missing $EZ/button.sh" >&2
    exit 1
fi
# ensure ops built
if [ ! -x "$EZ/ops/+x/ez_compose_frame.+x" ]; then
    (cd "$EZ" && sh button.sh compile) || true
fi

export EZ_PKG_NAME="$NAME"
export EZ_PKG_DIR="$EVENT_PKG"
# setsid so taskbar/entity timeout does not kill the editor
setsid nohup env EZ_PKG_NAME="$NAME" EZ_PKG_DIR="$EVENT_PKG" \
    sh "$EZ/button.sh" r >/tmp/event-ez-"$NAME".log 2>&1 < /dev/null &
disown
echo "event-ez launched for $NAME (EZ_PKG_DIR=$EVENT_PKG) log=/tmp/event-ez-$NAME.log"
