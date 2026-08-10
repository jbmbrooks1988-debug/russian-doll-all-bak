#!/bin/bash
# play_event.sh <package_dir> — run event_pkg/pages/page_1/event.pal via prisc+x
set -e
PKG="${1:-}"
if [ -z "$PKG" ] || [ ! -d "$PKG" ]; then
  echo "play_event: need package dir" >&2
  exit 1
fi
PKG="$(cd "$PKG" && pwd)"
NAME="$(basename "$PKG")"
PAL="$PKG/event_pkg/pages/page_1/event.pal"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MR_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOUSE_ROOT="$(cd "$MR_ROOT/../.." && pwd)"
PRISC="$HOUSE_ROOT/101.mutaclsym🧟‍♂️️+18.01/system/prisc+x"
if [ ! -x "$PRISC" ]; then
  echo "play_event: missing prisc+x at $PRISC" >&2
  exit 1
fi
if [ ! -f "$PAL" ]; then
  echo "play_event: no event script yet: $PAL" >&2
  # still leave a readable note in ledger
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play: no page_1/event.pal for $NAME" >> "$PKG/master_ledger.txt"
  exit 1
fi
# Run from muta system dir so relative default_op.txt noise is reduced if any
cd "$(dirname "$PRISC")"
"$PRISC" "$PAL" >> "$PKG/master_ledger.txt" 2>&1 || true
# Snapshot inventory after play
if [ -f "$PKG/inventory.txt" ]; then
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play event page_1 for $NAME | inventory: $(tr '\n' ' ' < "$PKG/inventory.txt")" >> "$PKG/master_ledger.txt"
  # keep gold.txt / menu in sync
  g=$(grep -E '^qolq=' "$PKG/inventory.txt" 2>/dev/null | head -1 | cut -d= -f2 | tr -d '\r\n ')
  [ -n "$g" ] && echo -n "$g" > "$PKG/gold.txt"
else
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] Play event page_1 for $NAME | no inventory.txt yet" >> "$PKG/master_ledger.txt"
fi
echo "play_event: done $NAME"
